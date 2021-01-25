/*
 *    Copyright (C) 2019-2021 Joshua Boudreau <jboudreau@45drives.com>
 *    
 *    This file is part of autotier.
 * 
 *    autotier is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 * 
 *    autotier is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 * 
 *    You should have received a copy of the GNU General Public License
 *    along with autotier.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "tierEngine.hpp"

#include <regex>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#define NUM_COMMANDS 8
enum command_enum {ONESHOT, STATUS, PIN, CONFIG, HELP, UNPIN, LPIN, LPOP, MOUNTPOINT};

int get_command_index(const char *cmd);
/* return switchable index determined by regex match of cmd against
 * each entry in command_list
 */

void pin(int optind, int argc, char *argv[], TierEngine &autotier);
/* grab tier name and set up list of files for TierEngine::pin_files()
 */

void usage(void);
/* print usage message to std::cout
 */
