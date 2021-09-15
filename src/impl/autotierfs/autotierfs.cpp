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

#include "version.hpp"
#include "alert.hpp"
#include "config.hpp"
#include "fusePassthrough.hpp"
#include "tools.hpp"
#include <iomanip>
#include <sstream>
#include <cstring>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

extern "C" {
	#include <getopt.h>
}

int main(int argc, char *argv[]){
	int opt;
	int option_ind = 0;
	bool print_version = false;
	fs::path config_path = DEFAULT_CONFIG_PATH;
	fs::path mountpoint;
	char *fuse_opts = NULL;
	
	ConfigOverrides config_overrides;
	
	static struct option long_options[] = {
		{"config",         required_argument, 0, 'c'},
		{"help",           no_argument,       0, 'h'},
		{"fuse-options",   required_argument, 0, 'o'},
		{"quiet",          no_argument,       0, 'q'},
		{"verbose",        no_argument,       0, 'v'},
		{"version",        no_argument,       0, 'V'},
		{0, 0, 0, 0}
	};
	
	/* Get CLI options.
	 */
	while((opt = getopt_long(argc, argv, "c:ho:qvV", long_options, &option_ind)) != -1){
		switch(opt){
			case 'c':
				config_path = optarg;
				break;
			case 'h':
				fs_usage();
				exit(EXIT_SUCCESS);
			case 'o':
				fuse_opts = optarg;
				break;
			case 'v':
				config_overrides.log_level_override = ConfigOverride<int>(2);
				break;
			case 'q':
				config_overrides.log_level_override = ConfigOverride<int>(0);
				break;
			case 'V':
				print_version = true;
				break;
			case '?':
				break; // getopt_long prints errors
			default:
				abort();
		}
	}
	
	if(print_version){
		Logging::log.message("autotier " VERS, Logger::log_level_t::NONE);
		if(!config_overrides.log_level_override.overridden() || config_overrides.log_level_override.value() >= 1){
			Logging::log.message(
				u8"   ┓\n"
				u8"└─ ┃ ├─\n"
				u8"└─ ┣ ├─\n"
				u8"└─ ┃ └─\n"
				u8"   ┛",
				1
			);
		}
		if(config_overrides.log_level_override.value() >= 2){
			Logging::log.message(
				"The logo shows three separate tiers on the left being combined into one storage space on the right.\n"
				"The use of " u8"└─" " to represent filesystem hierarchy was inspired by the output of `tree`.",
				1
			);
		}
		exit(EXIT_SUCCESS);
	}
	
	/* Grab mountpoint.
	 */
	if(optind >= argc){
		Logging::log.error("No mountpoint passed.");
		fs_usage();
		exit(EXIT_FAILURE);
	}
	
	/* Initialize filesystem and mount
	 * it to the mountpoint if it exists.
	 */
	mountpoint = argv[optind];
	if(!is_directory(mountpoint)){
		Logging::log.error("Invalid mountpoint or command: " + mountpoint.string());
		fs_usage();
		exit(EXIT_FAILURE);
	}
	try {
		FusePassthrough at_filesystem(config_path, config_overrides);
		Logging::log.set_output(SYSLOG);
		at_filesystem.mount_fs(mountpoint, fuse_opts);
	} catch (const ffd::ConfigException &e) {
		Logging::log.error(e.what());
		exit(EXIT_FAILURE);
	}
	
	
	return 0;
}
