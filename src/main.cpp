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
	int cmd;
	bool daemon_mode = false;
	int byte_format = BYTES;
	fs::path config_path = DEFAULT_CONFIG_PATH;
	fs::path mountpoint;
	char *fuse_opts = NULL;
	
	static struct option long_options[] = {
		{"config",		     required_argument, 0, 'c'},
		{"help",           no_argument,       0, 'h'},
		{"mountpoint",     required_argument, 0, 'm'},
		{"fuse-options",   required_argument, 0, 'o'},
		{"binary",         no_argument,       0, 'B'},
		{"SI",             no_argument,       0, 'S'},
		{"verbose",        no_argument,       &log_lvl, 2},
		{"quiet",          no_argument,       &log_lvl, 0},
		{0, 0, 0, 0}
	};
	
	while((opt = getopt_long(argc, argv, "c:hm:o:BS", long_options, &option_ind)) != -1){
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
		case 'B':
			byte_format = POWTWO;
			break;
		case 'S':
			byte_format = POWTEN;
			break;
		case '?':
			break; // getopt_long prints errors
		default:
			abort();
		}
	}
	
	if(optind < argc){
		cmd = get_command_index(argv[optind++]);
	}else{
		std::cerr << "No command passed." << std::endl;
		usage();
		exit(EXIT_FAILURE);
	}
	
	if(cmd == ERR){
		std::cerr << "Unknown command: " << argv[optind-1] << std::endl;
		exit(EXIT_FAILURE);
	}
	
	TierEngine autotier(config_path);
	if(mountpoint.empty()) mountpoint = autotier.get_mountpoint(); // grab from config
	autotier.get_config()->byte_format = byte_format;
	
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
			__attribute__((fallthrough));
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
	
	return 0;
}


