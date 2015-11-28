// Copyright © 2011, 2012, 2014, 2015 Richard Kettlewell.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#include <config.h>
#include "rsbackup.h"
#include "Conf.h"
#include "Command.h"
#include "Errors.h"
#include "IO.h"
#include "Utils.h"
#include "Store.h"
#include "Database.h"
#include "Prune.h"
#include "BulkRemove.h"
#include <algorithm>
#include <regex>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cerrno>

PrunePolicy::PrunePolicy(const std::string &name) {
  if(!policies)
    policies = new policies_type();
  (*policies)[name] = this;
}

const std::string &PrunePolicy::get(const Volume *volume,
                                    const std::string &name) const {
  auto it = volume->pruneParameters.find(name);
  if(it != volume->pruneParameters.end())
    return it->second;
  else
    throw ConfigError("missing pruning parameter '" + name + "'");
}

const std::string &PrunePolicy::get(const Volume *volume,
                                    const std::string &name,
                                    const std::string &def) const {
  auto it = volume->pruneParameters.find(name);
  if(it != volume->pruneParameters.end())
    return it->second;
  else
    return def;
}

const PrunePolicy *PrunePolicy::find(const std::string &name) {
  auto it = policies->find(name);
  if(it == policies->end())
    throw ConfigError("unrecognized pruning policy '" + name + "'");
  return it->second;
}

void validatePrunePolicy(const Volume *volume) {
  const PrunePolicy *policy = PrunePolicy::find(volume->prunePolicy);
  policy->validate(volume);
}

void backupPrunable(std::vector<Backup *> &onDevice,
                    std::map<Backup *, std::string> &prune,
                    int total) {
  if(onDevice.size() == 0)
    return;
  const Volume *volume = onDevice.at(0)->volume;
  const PrunePolicy *policy = PrunePolicy::find(volume->prunePolicy);
  return policy->prunable(onDevice, prune, total);
}

PrunePolicy::policies_type *PrunePolicy::policies;

// Remove old and incomplete backups
void pruneBackups() {
  // Make sure all state is available
  config.readState();

  // Figure out which backups are obsolete, if any
  std::vector<Backup *> oldBackups;
  for(auto &h: config.hosts) {
    const Host *host = h.second;
    if(!host->selected())
      continue;
    for(auto &v: host->volumes) {
      Volume *volume = v.second;
      if(!volume->selected())
        continue;
      // For each device, the complete backups on that device
      std::map<std::string, std::vector<Backup *>> onDevices;
      // Total backups of this volume
      int total = 0;
      for(Backup *backup: volume->backups) {
        switch(backup->getStatus()) {
        case UNKNOWN:
        case UNDERWAY:
        case FAILED:
          if(command.pruneIncomplete) {
            // Prune incomplete backups.  Anything that failed is counted as
            // incomplete (a succesful retry will overwrite the log entry).
            backup->contents = std::string("status=")
              + backup_status_names[backup->getStatus()];
            oldBackups.push_back(backup);
          }
          break;
        case PRUNING:
          // Both commands continue pruning anything that has started being
          // pruned.  log should already be set.
          oldBackups.push_back(backup);
          break;
        case PRUNED:
          break;
        case COMPLETE:
          if(command.prune) {
            onDevices[backup->deviceName].push_back(backup);
            ++total;
          }
          break;
        }
      }
      for(auto &od: onDevices) {
        std::vector<Backup *> &onDevice = od.second;
        std::map<Backup *, std::string> prune;
        backupPrunable(onDevice, prune, total);
        for(auto &p: prune) {
          Backup *backup = p.first;
          backup->contents = p.second;
          oldBackups.push_back(backup);
          --total;
        }
      }
    }
  }

  // Return straight away if there's nothing to do
  if(oldBackups.size() == 0)
    return;

  // Update the status of everything we're pruning
  if(command.act) {
    config.getdb().begin();
    for(Backup *b: oldBackups) {
      if(b->getStatus() != PRUNING) {
        b->setStatus(PRUNING);
        b->pruned = Date::now();
        b->update(config.getdb());
      }
    }
    config.getdb().commit();
    // We don't catch DatabaseBusy here; the prune just fails.
  }

  // Identify devices
  config.identifyDevices(Store::Enabled);

  // Identify removable backups and schedule their deletion
  EventLoop e;
  ActionList al(&e);
  // TODO manual object lifetime management, refactor
  struct Removable {
    BulkRemove *br;
    Backup *backup;
    Removable(BulkRemove *br_, Backup *backup_): br(br_), backup(backup_) {}
  };
  std::vector<Removable> removables;
  for(auto backup: oldBackups) {
    Device *device = config.findDevice(backup->deviceName);
    Store *store = device->store;
    // Can't delete backups from unavailable stores
    if(!store || store->state != Store::Enabled)
      continue;
    std::string backupPath = backup->backupPath();
    std::string incompletePath = backupPath + ".incomplete";
    try {
      // Schedule removal of the backup
      if(command.verbose)
        IO::out.writef("INFO: pruning %s because: %s\n",
                       backupPath.c_str(),
                       backup->contents.c_str());
      if(command.act) {
        // Create the .incomplete flag file so that the operator knows this
        // backup is now partial
        IO ifile;
        ifile.open(incompletePath, "w");
        ifile.close();
        // Actually remove the backup
        BulkRemove *br = new BulkRemove(backupPath);
        br->uses(device->name);
        removables.push_back(Removable(br, backup));
        al.add(br);
      }
    } catch(std::runtime_error &exception) {
      // Log anything that goes wrong
      error("failed to remove %s: %s\n", backupPath.c_str(), exception.what());
      ++errors;
    }
  }

  if(!command.act)
    assert(removables.size() == 0);

  // Attempt to delete backups
  al.go();

  if(command.act) {
    // Complain about failed prunes
    for(auto &removable: removables) {
      const std::string backupPath = removable.backup->backupPath();
      if(removable.br->getStatus()) {
        // Log failed prunes
        error("failed to remove %s: %s\n",
              backupPath.c_str(),
              SubprocessFailed::format("rm",
                                       removable.br->getStatus()).c_str());
      } else {
        const std::string incompletePath = backupPath + ".incomplete";
        // Remove the 'incomplete' marker.
        if(command.verbose)
          IO::out.writef("INFO: removing %s\n", incompletePath.c_str());
        if(unlink(incompletePath.c_str()) < 0 && errno != ENOENT) {
          error("removing %s: %s", incompletePath.c_str(), strerror(errno));
          ++errors;
        }
      }
    }
    // Update persistent state
    for(;;) {
      int retries = 0;
      try {
        config.getdb().begin();
        for(auto &removable: removables) {
          if(!removable.br->getStatus()) {
            removable.backup->setStatus(PRUNED);
            removable.backup->pruned = Date::now();
            removable.backup->update(config.getdb());
          }
        }
        config.getdb().commit();
      } catch(DatabaseBusy &) {
        // Keep trying, database should be in sync with reality
        // Log a message every second or so
        if(!(retries++ & 1023))
          warning("pruning: retrying database update");
        // Wait a millisecond and try again
        usleep(1000);
        continue;
      }
      break;                            // success
    }
    // Update internal state
    for(auto &removable: removables) {
      if(!removable.br->getStatus())
        removable.backup->volume->removeBackup(removable.backup);
      delete removable.br;
    }
  }
}

// Remove old prune logfiles
void prunePruneLogs() {
  // Delete status=PRUNED records that are too old
  Database::Statement(config.getdb(),
                      "DELETE FROM backup"
                      " WHERE status=?"
                      " AND pruned < ?",
                      SQL_INT, PRUNED,
                      SQL_INT64, (int64_t)(Date::now()
                                           - config.keepPruneLogs * 86400),
                      SQL_END);

  // Delete pre-sqlitification pruning logs
  // TODO: one day this code can be removed.
  Date today = Date::today();

  // Regexp for parsing the filename
  // Format is prune-YYYY-MM-DD.log
  std::regex r("^prune-([0-9]+-[0-9]+-[0-9]+)\\.log$");

  Directory d;
  d.open(config.logs);
  std::string f;
  while(d.get(f)) {
    std::smatch mr;
    if(!std::regex_match(f, mr, r))
      continue;
    Date d = Date(mr[1]);
    int age = today - d;
    if(age <= config.keepPruneLogs)
      continue;
    std::string path = config.logs + PATH_SEP + f;
    if(command.verbose)
      IO::out.writef("INFO: removing %s\n", path.c_str());
    if(command.act)
      if(unlink(path.c_str()) < 0)
        throw IOError("removing " + path, errno);
  }
}
