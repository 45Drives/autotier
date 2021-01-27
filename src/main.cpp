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

#include "alert.hpp"
#include "config.hpp"
#include "tierEngine.hpp"
#include "tools.hpp"
#include "fusePassthrough.hpp"
#include <sstream>
#include <cstring>

extern "C" {
	#include <getopt.h>
}

int main(int argc, char *argv[]){
	/* parse flags, get command,
	 * construct TierEngine,
	 * execute command.
	 */
	int opt;
	int option_ind = 0;
	int cmd;
	int log_lvl = 1;
	bool daemon_mode = false;
	fs::path config_path = DEFAULT_CONFIG_PATH;
	fs::path mountpoint;
	char *fuse_opts = NULL;
	
	static struct option long_options[] = {
		{"config",		     required_argument, 0, 'c'},
		{"help",           no_argument,       0, 'h'},
		{"fuse-options",   required_argument, 0, 'o'},
		{"verbose",        no_argument,       &log_lvl, 2},
		{"quiet",          no_argument,       &log_lvl, 0},
		{0, 0, 0, 0}
	};
	
	while((opt = getopt_long(argc, argv, "c:ho:BS", long_options, &option_ind)) != -1){
		switch(opt){
		case 0:
			// flag set
			break;
		case 'c':
			config_path = optarg;
			break;
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
			break;
		case 'o':
			fuse_opts = optarg;
			break;
		case '?':
			break; // getopt_long prints errors
		default:
			abort();
		}
	}
	
	Logging::log = Logger(log_lvl);
	
	if(optind < argc){
		cmd = get_command_index(argv[optind]);
	}else{
		Logging::log.error("No command passed.", false);
		usage();
		exit(EXIT_FAILURE);
	}
	
	if(cmd == MOUNTPOINT){
		mountpoint = argv[optind];
		daemon_mode = true;
		Logging::log = Logger(log_lvl, SYSLOG);
		FusePassthrough at_filesystem(config_path);
		at_filesystem.mount_fs(mountpoint, fuse_opts);
	}else{
		bool read_only = true;
		TierEngine autotier(config_path, read_only);
		optind++;
		switch(cmd){
			case ONESHOT:
				autotier.begin(daemon_mode);
				break;
			case STATUS:
				autotier.print_tier_info();
				break;
			case PIN:
				pin(optind, argc, argv, autotier);
				break;
			case CONFIG:
				Logging::log.message("Config file: (" + config_path.string() + ")", 1);
				{
					std::ifstream f(config_path.string());
					std::stringstream ss;
					ss << f.rdbuf();
					Logging::log.message(ss.str(), 1);
				}
				break;
			case UNPIN:
				if(argc - optind < 1){
					Logging::log.error("No file names passed.", false);
					usage();
					exit(EXIT_FAILURE);
				}
				autotier.unpin(optind, argc, argv);
				break;
			case LPIN:
				Logging::log.message("Pinned files:", 1);
				autotier.launch_crawlers(&TierEngine::emplace_file);
				autotier.print_file_pins();
				break;
			case LPOP:
				autotier.launch_crawlers(&TierEngine::emplace_file);
				autotier.sort();
				autotier.print_file_popularity();
				break;
			case HELP:
				usage();
				exit(EXIT_SUCCESS);
			default:
				usage();
				exit(EXIT_FAILURE);
				break;
		}
	}
	return 0;
}


