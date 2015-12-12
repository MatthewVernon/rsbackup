//-*-C++-*-
// Copyright © 2015 Richard Kettlewell.
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
#ifndef HISTORYGRAPH_H
#define HISTORYGRAPH_H

#include "Render.h"
#include "Conf.h"

struct HistoryGraphContext;

/** @brief Host name labels */
class HostLabels: public Render::Grid {
public:
  /** @brief Constructor
   * @param ctx Configuration context
   */
  HostLabels(HistoryGraphContext &ctx);
};

/** @brief Volume name labels */
class VolumeLabels: public Render::Grid {
public:
  /** @brief Constructor
   * @param ctx Configuration context
   */
  VolumeLabels(HistoryGraphContext &ctx);
};

/** @brief Key showing mapping of device names to colors */
class DeviceKey: public Render::Grid {
public:
  /** @brief Constructor
   * @param ctx Configuration context
   */
  DeviceKey(HistoryGraphContext &ctx);

  /** @brief Return the device row number for a backup
   * @param backup Backup
   * @return Device row number
   */
  unsigned device_row(const Backup *backup) const {
    return device_rows.find(backup->deviceName)->second;
  }

  /** @brief Return the color for a device by number
   * @param row Device row number
   * @return Color
   */
  const Render::Color &device_color(unsigned row) const;

  /** @brief Return the color for a backup
   * @param backup Backup
   * @return Color
   */
  const Render::Color &device_color(const Backup *backup) const {
    return device_color(device_row(backup));
  }

private:
  /** @brief Configuration context */
  HistoryGraphContext &context;

  /** @brief Mapping of device names to device rows */
  std::map<std::string,unsigned> device_rows;
};

/** @brief Visualization of backup history */
class HistoryGraphContent: public Render:: Widget {
public:
  /** @brief Constructor
   * @param ctx Configuration context
   * @param device_key Corresponding @ref DeviceKey structure
   */
  HistoryGraphContent(HistoryGraphContext &ctx,
                      const DeviceKey &device_key);

  /** @brief Set the rot height
   * @param h Row height
   */
  void set_row_height(double h) {
    row_height = h;
    changed();
  }

  /** @brief Render the vertical guides */
  void render_vertical_guides();

  /** @brief Render the horizontal guides */
  void render_horizontal_guides();

  /** @brief Render the data */
  void render_data();

  void set_extent() override;
  void render() override;

  /** @brief Earliest date of any backup */
  Date earliest;

  /** @brief Latest date of any backup */
  Date latest;

private:
  /** @brief Height of a single row
   *
   * Set by @ref set_extent.
   */
  double row_height = 0;

  /** @brief Configuration context */
  HistoryGraphContext &context;

  /** @brief Corresponding @ref DeviceKey object */
  const DeviceKey &device_key;

  /** @brief Number of rows */
  unsigned rows;
};

/** @brief Time-axis labels */
class TimeLabels: public Render::Container {
public:
  /** @brief Constructor
   * @param ctx Configuration context
   * @param content Corresponding @ref HistoryGraphContent object
   */
  TimeLabels(HistoryGraphContext &ctx,
             HistoryGraphContent &content);

  void set_extent() override;
private:
  /** @brief Configuration context */
  HistoryGraphContext &context;

  /** @brief Corresponding @ref HistoryGraphContent object */
  HistoryGraphContent &content;
};

/** @brief Complete graph showing backup history */
class HistoryGraph: public Render::Grid {
public:
  /** @brief Constructor
   * @param ctx Configuration context
   */
  HistoryGraph(HistoryGraphContext &ctx);

  /** @brief Configuration context */
  HistoryGraphContext &context;

  /** @brief Host name labels */
  HostLabels host_labels;

  /** @brief Volume name labels */
  VolumeLabels volume_labels;

  /** @brief Key showing mapping of device names to colors */
  DeviceKey device_key;

  /** @brief Visualization of backup history */
  HistoryGraphContent content;

  /** @brief Time-axis labels */
  TimeLabels time_labels;

  void set_extent() override;
  void render() override;
};

/** @brief Configuration context for @ref HistoryGraph */
struct HistoryGraphContext: public Render::Context {
  /** @brief Constructor */
  HistoryGraphContext();

  /** @brief Horizontal padding */
  double xpad = 8;

  /** @brief Vertical padding */
  double ypad = 2;

  /** @brief Width of a single day */
  double day_width = 4;

  /** @brief Height of the rectangle for a backup */
  double device_indicator_height = 2;

  /** @brief Width of a the rectangle in a @ref DeviceKey */
  double device_key_indicator_width = 16;

  /** @brief Map of color names to values
   *
   * The known color names are:
   * - @c background, the background color
   * - @c foreground, use for all text
   * - @c month_guide, used for the vertical month guide bars
   * - @c volume_guide, used for the horizontal lines between volumes of the same host
   * - @c host_guide, used for the horizontal lines between hosts
   * - @c deviceN for 0<=N<=5, colors for devices
   */
  std::map<std::string, Render::Color> colors;
};

#endif /* HISTORYGRAPH_H */