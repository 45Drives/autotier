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
#include <thread>

inline bool config_passed(int argc, char *argv[]){
	return (argc >= 3 && (strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--config") == 0));
}

#ifdef THREAD
void fuse_thread(const fs::path &mountpoint, std::list<Tier> &tiers){
	FusePassthrough at_filesystem(tiers);
	int res = at_filesystem.mount_fs(mountpoint);
}
#endif

static void launch_daemon(int argc, char *argv[]){
	bool daemon_mode = false;
	fs::path config_path = DEFAULT_CONFIG_PATH;
	parse_flags(argc, argv, config_path);
	TierEngine autotier(config_path);
  
#ifdef THREAD
	std::thread fuse;
#endif
	
	int cmd = get_command_index(argc, argv);

#ifdef THREAD
	if(cmd == RUN)
		fuse = std::thread(fuse_thread, autotier.get_mountpoint(), std::ref(autotier.get_tiers()));
#else
	pid_t pid = (cmd == RUN)? fork() : 1; // fork if run else goto parent
	if(pid == -1){
		error(FORK);
		exit(errno);
	}else if(pid == 0){
		// child
		FusePassthrough at_filesystem(autotier.get_tiers());
		at_filesystem.mount_fs(autotier.get_mountpoint());
	}else{
		// parent
#endif
	
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
	default:
		usage();
		exit(1);
		break;
	}
	if(cmd == RUN){
		// kill fusermount
		int fusermount_pid = fork();
		switch(fusermount_pid){
		case 0:
			// child
			execlp("umount", "umount", "/mnt/autotier", NULL);
			error(MOUNT);
			exit(EXIT_FAILURE);
		case -1:
			error(FORK);
			exit(EXIT_FAILURE);
		default:
			// parent
			break;
		}
#ifdef THREAD
		fuse.join(); // wait for child to exit
#endif
	}
#ifndef THREAD
	}
#endif
}

int main(int argc, char *argv[]){
	launch_daemon(argc, argv);
	
	return 0;
}


