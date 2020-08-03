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

#include "tools.hpp"
#include "tierEngine.hpp"
#include "tier.hpp"
#include "file.hpp"
#include "config.hpp"

#include <sys/xattr.h>
#include <iostream>
#include <regex>
#include <vector>
#include <boost/filesystem.hpp>

bool recursive_flag_set = false;

std::regex command_list[NUM_COMMANDS] = {
	std::regex("^[Rr]un|RUN$"),
	std::regex("^[Oo]neshot|ONESHOT$"),
	std::regex("^[Ss]tatus|STATUS$"),
	std::regex("^[Pp]in|PIN$"),
	std::regex("^[Cc]onfig|CONFIG$"),
	std::regex("^[Hh]elp|HELP$"),
	std::regex("^[Uu]npin|UNPIN$"),
	std::regex("^[Ll]ist-[Pp]ins?|LIST-PINS?$"),
	std::regex("^[Ll]ist-[Pp]opularity|LIST-POPULARITY$")
};

int get_command_index(int argc, char *argv[]){
	if(argc < 2){ // no command
		return ERR;
	}
	std::string command = argv[1];
	for(int itr = 0; itr < NUM_COMMANDS; itr++){
		if(regex_match(command,command_list[itr]))
			return itr;
	}
	return ERR;
}

void parse_flags(int argc, char *argv[], fs::path &config_path){
	for(int i = 0; i < argc; i++){
		if(regex_match(argv[i],std::regex("^-c|--config$"))){
			if(++i >= argc){
				usage();
				exit(1);
			}
			config_path = fs::path(argv[i]);
		}else if(regex_match(argv[i],std::regex("^-r|--recursive$"))){
			recursive_flag_set = true;
		}
	}
}

void pin(int argc, char *argv[], TierEngine &autotier){
	if(argc < 4){
		usage();
		exit(1);
	}
	std::string tier_name = argv[2];
	std::vector<fs::path> files;
	for(int i = 3; i < argc; i++){
		files.emplace_back(argv[i]);
	}
	Log("Pinning files to " + tier_name, 2);
	autotier.pin_files(tier_name, files);
}

void usage(){
	std::cout <<
	"autotier Copyright (C) 2019-2020  Josh Boudreau <jboudreau@45drives.com>\n"
  "This program is released under the GNU General Public License v3.\n"
  "See <https://www.gnu.org/licenses/> for more details.\n"
  "\n"
	"Usage:\n"
	"  autotier <command> <flags> [{-c|--config} </path/to/config>]\n"
	"commands:\n"
	"  oneshot     - execute tiering only once\n"
	"  run         - start tiering of files as daemon\n"
	"  status      - list info about defined tiers\n"
	"  pin <\"tier name\"> <\"path/to/file\">...\n"
	"              - pin file(s) to tier using tier name in config file\n"
	"              - if a path to a directory is passed, all top-level files\n"
	"                will be pinned\n"
	"              - \"path/to/file\" must be relative to the autotier mountpoint.\n"
	"  unpin <path/to/file>...\n"
	"              - remove pin from file(s)\n"
	"              - \"path/to/file\" must be relative to the autotier mountpoint.\n"
	"  config      - display current configuration file\n"
	"  list-pins   - show all pinned files\n"
	"  list-popularity\n"
	"              - print list of all tier files sorted by frequency of use\n"
	"  help        - display this message\n"
	"flags:\n"
	"  -c --config <path/to/config>\n"
	"              - override configuration file path (default /etc/autotier.conf)\n"
	<< std::endl;
}
