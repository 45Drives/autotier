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

#include "config.hpp"
#include "tierEngine.hpp"
#include "tools.hpp"
#include "fusePassthrough.hpp"
#include "alert.hpp"
#include <thread>
#include <iostream>
#include <string.h>
#include <getopt.h>

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
	
	if(optind < argc){
		cmd = get_command_index(argv[optind]);
	}else{
		Logging::log.error("No command passed.", false);
		usage();
		exit(EXIT_FAILURE);
	}
	
	TierEngine autotier(config_path);
	
	if(cmd == MOUNTPOINT){
		mountpoint = argv[optind];
		daemon_mode = true;
		pid_t pid = fork(); // fork if run else goto parent
		if(pid == -1){
			Logging::log.error("Error forking!");
		}else if(pid == 0){
			// child
			FusePassthrough at_filesystem(autotier.get_tiers(), autotier.get_db());
			at_filesystem.mount_fs(mountpoint, fuse_opts);
			return 0;
		}
	}
	
	switch(cmd){
		case MOUNTPOINT:
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
			std::cout << "Config file: (" << config_path.string() << ")" << std::endl;
			std::cout << std::ifstream(config_path.string()).rdbuf();
			break;
		case UNPIN:
			if(argc - optind < 1){
				std::cerr << "No file names passed." << std::endl;
				usage();
				exit(1);
			}
			autotier.unpin(optind, argc, argv);
			break;
		case LPIN:
			std::cout << "Pinned files:" << std::endl;
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
	return 0;
}


