//-*-C++-*-
// Copyright © Richard Kettlewell.
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
#ifndef LOCATION_H
#define LOCATION_H
/** @file Location.h
 * @brief File locations
 */

#include <string>

/** @brief A location within a file */
struct Location {
  /** @brief Constructor */
  Location() {}

  /** @brief Constructor */
  Location(const std::string &path, int line): path(path), line(line) {}

  /** @brief Containing filename */
  std::string path;

  /** @brief Line number */
  int line = -1;
};

#endif
