/*
		Copyright (C) 2019-2020 Joshua Boudreau <jboudreau@45drives.com>
		
		This file is part of autotier.

		autotier is free software: you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation, either version 3 of the License, or
		(at your option) any later version.

		autotier is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with autotier.	If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include "tierEngine.hpp"

#include <regex>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#define NUM_COMMANDS 9
enum command_enum {RUN, ONESHOT, STATUS, PIN, CONFIG, HELP, UNPIN, LPIN, LPOP};

enum args {PROG, COMMAND, OPTS};

extern std::regex command_list[NUM_COMMANDS];

int get_command_index(const char *cmd);

void pin(int optind, int argc, char *argv[], TierEngine &autotier);

void usage(void);
