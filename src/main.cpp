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
	int opt;
	int option_ind = 0;
	bool daemon_mode = false;
	fs::path config_path = DEFAULT_CONFIG_PATH;
	fs::path mountpoint;
	char *fuse_opts = NULL;
	
	if(argc < 2){
		std::cerr << "No command passed." << std::endl;
		usage();
		exit(EXIT_FAILURE);
	}
	
	int cmd = get_command_index(argv[1]);
	
	if(cmd != ERR){
		// skip command arg
		argv[1] = argv[0]; // overwrite command with prog name
	  argc--;
	  argv++; // point to command
	}
	
	static struct option long_options[] = {
		{"config",		   required_argument, 0, 'c'},
		{"help",         no_argument,       0, 'h'},
		{"mountpoint",   required_argument, 0, 'm'},
		{"fuse-options", required_argument, 0, 'o'},
		{"verbose",      no_argument,       &log_lvl, 2},
		{"quiet",        no_argument,       &log_lvl, 0},
		{0, 0, 0, 0}
	};
	
	while((opt = getopt_long(argc, argv, "c:hm:o:", long_options, &option_ind)) != -1){
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
		case 'm':
			mountpoint = optarg;
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
	
	if(cmd == ERR){
		std::cerr << "Unknown command: " << argv[1] << std::endl;
		exit(EXIT_FAILURE);
	}
	
	TierEngine autotier(config_path);
	if(mountpoint.empty()) mountpoint = autotier.get_mountpoint(); // grab from config
	
	pid_t pid = (cmd == RUN)? fork() : 1; // fork if run else goto parent
	if(pid == -1){
		error(FORK);
		exit(EXIT_FAILURE);
	}else if(pid == 0){
		// child
		FusePassthrough at_filesystem(autotier.get_tiers());
		at_filesystem.mount_fs(mountpoint, fuse_opts);
	}else{
		switch(cmd){
		case RUN:
			daemon_mode = true;
		case ONESHOT:
			autotier.begin(daemon_mode);
			break;
		case STATUS:
			autotier.print_tier_info();
			break;
		case PIN:
			pin(argc, argv, autotier);
			break;
		case CONFIG:
			std::cout << "Config file: (" << config_path.string() << ")" << std::endl;
			std::cout << std::ifstream(config_path.string()).rdbuf();
			break;
		case UNPIN:
			if(argc < 3){
				usage();
				exit(1);
			}
			autotier.unpin(argc, argv);
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
		if(cmd == RUN){
			// kill fusermount
			int fusermount_pid = fork();
			switch(fusermount_pid){
			case 0:
				// child
				execlp("umount", "umount", mountpoint.c_str(), NULL);
				error(MOUNT);
				exit(EXIT_FAILURE);
			case -1:
				error(FORK);
				exit(EXIT_FAILURE);
			default:
				// parent
				break;
			}
		}
	}
	
	//launch_daemon(argc, argv);
	
	return 0;
}


