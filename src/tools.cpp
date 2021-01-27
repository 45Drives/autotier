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

#include "tools.hpp"
#include "alert.hpp"
#include "tierEngine.hpp"
#include <iostream>
#include <regex>
#include <vector>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

int get_command_index(const char *cmd){
	std::regex command_list[NUM_COMMANDS] = {
		std::regex("^[Oo]neshot|ONESHOT$"),
		std::regex("^[Ss]tatus|STATUS$"),
		std::regex("^[Pp]in|PIN$"),
		std::regex("^[Cc]onfig|CONFIG$"),
		std::regex("^[Hh]elp|HELP$"),
		std::regex("^[Uu]npin|UNPIN$"),
		std::regex("^[Ll]ist-[Pp]ins?|LIST-PINS?$"),
		std::regex("^[Ll]ist-[Pp]opularity|LIST-POPULARITY$")
	};
	for(int itr = 0; itr < NUM_COMMANDS; itr++){
		if(regex_match(cmd, command_list[itr]))
			return itr;
	}
	return MOUNTPOINT;
}

void pin(int optind, int argc, char *argv[], TierEngine &autotier){
	if(argc - optind < 2){
		Logging::log.error("Invalid argument(s).", false);
		usage();
		exit(EXIT_FAILURE);
	}
	std::string tier_name = argv[optind++];
	std::vector<fs::path> files;
	while(optind < argc){
		files.emplace_back(argv[optind++]);
	}
	Logging::log.message("Pinning files to " + tier_name, 1);
	autotier.pin_files(tier_name, files);
}

void usage(){
	Logging::log.message(
		"autotier Copyright (C) 2019-2020  Josh Boudreau <jboudreau@45drives.com>\n"
		"This program is released under the GNU General Public License v3.\n"
		"See <https://www.gnu.org/licenses/> for more details.\n"
		"\n"
		"Usage:\n"
		"  autotier <command> [<flags>]\n"
		"Commands:\n"
		"  oneshot     - execute tiering only once\n"
		"  run         - start tiering of files as daemon\n"
		"  status      - list info about defined tiers\n"
		"  pin <\"tier name\"> <\"path/to/file\">...\n"
		"              - pin file(s) to tier using tier name in config file\n"
		"              - if a path to a directory is passed, all top-level files\n"
		"                will be pinned\n"
		"              - \"path/to/file\" must be relative to the autotier mountpoint\n"
		"  unpin <path/to/file>...\n"
		"              - remove pin from file(s)\n"
		"              - \"path/to/file\" must be relative to the autotier mountpoint\n"
		"  config      - display current configuration file\n"
		"  list-pins   - show all pinned files\n"
		"  list-popularity\n"
		"              - print list of all tier files sorted by frequency of use\n"
		"  help        - display this message\n"
		"Flags:\n"
		"  -h, --help  - display this message and cancel current command\n"
		"  -c, --config <path/to/config>\n"
		"              - override configuration file path (default /etc/autotier.conf)\n"
		"  -m, --mountpoint <path/to/mountpoint>\n"
		"              - override mountpoint from configuration file\n"
		"  -o, --fuse-options <comma,separated,list>\n"
		"              - mount options to pass to fuse (see man mount.fuse)\n"
		"  --verbose   - set log level to 2\n"
		"  --quiet     - set log level to 0 (no output)\n",
		1
	);
}
