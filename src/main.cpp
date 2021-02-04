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
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

extern "C" {
	#include <getopt.h>
	#include <fcntl.h>
}

int main(int argc, char *argv[]){
	int opt;
	int option_ind = 0;
	int cmd;
	fs::path config_path = DEFAULT_CONFIG_PATH;
	fs::path mountpoint;
	char *fuse_opts = NULL;
	
	ConfigOverrides config_overrides;
	
	static struct option long_options[] = {
		{"config",		     required_argument, 0, 'c'},
		{"help",           no_argument,       0, 'h'},
		{"fuse-options",   required_argument, 0, 'o'},
		{"verbose",        no_argument,       0, 'v'},
		{"quiet",          no_argument,       0, 'q'},
		{0, 0, 0, 0}
	};
	
	/* Get CLI options.
	 */
	while((opt = getopt_long(argc, argv, "c:ho:vq", long_options, &option_ind)) != -1){
		switch(opt){
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
			case 'v':
				config_overrides.log_level_override = ConfigOverride<int>(2);
				break;
			case 'q':
				config_overrides.log_level_override = ConfigOverride<int>(0);
				break;
			case '?':
				break; // getopt_long prints errors
			default:
				abort();
		}
	}
	
	/* Grab command or mountpoint.
	 */
	if(optind < argc){
		cmd = get_command_index(argv[optind]);
	}else{
		Logging::log.error("No command passed.", false);
		usage();
		exit(EXIT_FAILURE);
	}
	
	if(cmd == MOUNTPOINT){
		/* Initialize filesystem and mount
		 * it to the mountpoint if it exists.
		 */
		mountpoint = argv[optind];
		if(!is_directory(mountpoint)){
			Logging::log.error("Invalid mountpoint or command: " + mountpoint.string(), false);
			usage();
			exit(EXIT_FAILURE);
		}
		FusePassthrough at_filesystem(config_path, config_overrides);
		Logging::log.set_output(SYSLOG);
		at_filesystem.mount_fs(mountpoint, fuse_opts);
	}else{
		/* Process ad hoc command.
		 */
		switch(cmd){
			case ONESHOT:
			case PIN:
			case UNPIN:
				/* Pass command through to the filesystem
				 * tier engine through a named FIFO pipe.
				 */
				{
					std::vector<std::string> payload;
					payload.push_back(argv[optind++]); // push command name
					if(cmd == PIN) payload.push_back(argv[optind++]); // push tier name
					if(cmd == PIN || cmd == UNPIN){
						std::vector<std::string> paths;
						while(optind < argc)
							paths.push_back(argv[optind++]);
						if(paths.empty())
							Logging::log.error("No arguments passed.");
						sanitize_paths(paths);
						for(const std::string &path : paths)
							payload.emplace_back(path);
					}
					
					fs::path run_path = pick_run_path(config_path);
					
					send_fifo_payload(payload, run_path / "request.pipe");
					
					Logging::log.message("Waiting for filesystem response...", 2);
					
					get_fifo_payload(payload, run_path / "response.pipe");
					
					if(payload.front() == "OK"){
						Logging::log.message("Response OK.", 2);
						for(std::vector<std::string>::iterator itr = std::next(payload.begin()); itr != payload.end(); ++itr){
							Logging::log.message(*itr, 1);
						}
					}else{
						for(std::vector<std::string>::iterator itr = std::next(payload.begin()); itr != payload.end(); ++itr){
							Logging::log.error(*itr, false);
						}
						exit(EXIT_FAILURE);
					}
				}
				break;
				/* Other commands can just execute in the calling process
				 * by opening the database as read-only.
				 */
			case STATUS:
				{
					bool read_only = true;
					TierEngine autotier(config_path, config_overrides, read_only);
					autotier.print_tier_info();
				}
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
			case HELP:
				usage();
				break;
			case LPIN:
				Logging::log.message("Pinned files:", 1);
				{
					bool read_only = true;
					TierEngine autotier(config_path, config_overrides, read_only);
					autotier.launch_crawlers(&TierEngine::print_file_pins);
				}
				break;
			case LPOP:
				Logging::log.message("File popularity:", 1);
				{
					bool read_only = true;
					TierEngine autotier(config_path, config_overrides, read_only);
					autotier.launch_crawlers(&TierEngine::print_file_popularity);
				}
				break;
		}
	}
	return 0;
}
