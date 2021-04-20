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
#include "version.hpp"
#include "config.hpp"
#include "stripWhitespace.hpp"
#include <sstream>

extern "C" {
	#include <getopt.h>
}

fs::path get_run_path(const fs::path &config_path){
	fs::path run_path("/var/lib/autotier");
	std::string line, key, value;
	
	// open file
	std::ifstream config_file(config_path.string());
	if(!config_file){
		Logging::log.error("Failed to open config file.");
		exit(EXIT_FAILURE);
	}
	
	while(config_file){
		getline(config_file, line);
		
		strip_whitespace(line);
		// full line comments:
		if(line.empty() || line.front() == '#')
			continue; // ignore comments
		
		if(line.front() != '['){
			std::stringstream line_stream(line);
			
			getline(line_stream, key, '=');
			strip_whitespace(key);
			if(key != "Metadata Path")
				continue;
			
			getline(line_stream, value);
			strip_whitespace(value);
			
			run_path = value;
			break;
		}
	}
	
	return run_path / std::to_string(std::hash<std::string>{}(config_path.string()));;
}

int main(int argc, char *argv[]){
	int opt;
	int option_ind = 0;
	int cmd;
	bool print_version = false;
	bool json = false;
	fs::path config_path = DEFAULT_CONFIG_PATH;
	
	static struct option long_options[] = {
		{"config",         required_argument, 0, 'c'},
		{"help",           no_argument,       0, 'h'},
		{"json",           no_argument,       0, 'j'},
		{"verbose",        no_argument,       0, 'v'},
		{"quiet",          no_argument,       0, 'q'},
		{"version",        no_argument,       0, 'V'},
		{0, 0, 0, 0}
	};
	
	/* Get CLI options.
	 */
	while((opt = getopt_long(argc, argv, "c:hjvqV", long_options, &option_ind)) != -1){
		switch(opt){
			case 'c':
				config_path = optarg;
				break;
			case 'h':
				cli_usage();
				exit(EXIT_SUCCESS);
			case 'j':
				json = true;
				break;
			case 'v':
				Logging::log.set_level(2);
				break;
			case 'q':
				Logging::log.set_level(0);
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
		Logging::log.message("autotier " VERS, 0);
		Logging::log.message(
			u8"   ┓\n"
			u8"└─ ┃ ├─\n"
			u8"└─ ┣ ├─\n"
			u8"└─ ┃ └─\n"
			u8"   ┛",
			1
		);
		Logging::log.message(
			"The logo shows three separate tiers on the left being combined into one storage space on the right.\n"
			"The use of " u8"└─" " to represent filesystem hierarchy was inspired by the output of `tree`.",
			2
		);
		exit(EXIT_SUCCESS);
	}
	
	/* Grab command.
	 */
	if(optind < argc){
		cmd = get_command_index(argv[optind]);
	}else{
		Logging::log.error("No command passed.");
		cli_usage();
		exit(EXIT_FAILURE);
	}
	
	if(cmd == -1){
		Logging::log.error("Invalid command: " + std::string(argv[optind]));
		cli_usage();
		exit(EXIT_FAILURE);
	}
	
	/* Process ad hoc command.
		*/
	if(cmd == HELP){
		cli_usage();
	}else{
		std::vector<std::string> payload;
		payload.push_back(argv[optind++]); // push command name
		if(cmd == PIN){
			if(optind == argc){
				Logging::log.error("No arguments passed.");
				exit(EXIT_FAILURE);
			}
			payload.push_back(argv[optind++]); // push tier name
		}
		if(cmd == PIN || cmd == UNPIN || cmd == WHICHTIER){
			std::list<std::string> paths;
			while(optind < argc)
				paths.push_back(argv[optind++]);
			if(paths.empty()){
				Logging::log.error("No arguments passed.");
				exit(EXIT_FAILURE);
			}
			sanitize_paths(paths);
			if(paths.empty()){
				Logging::log.error("No remaining valid paths.");
				exit(EXIT_FAILURE);
			}
			for(const std::string &path : paths)
				payload.emplace_back(path);
		}else if(cmd == STATUS){
			std::stringstream ss;
			ss << std::boolalpha << json << std::endl;
			payload.emplace_back(ss.str());
		}else if(cmd == ONESHOT){
			while(optind < argc){
				payload.push_back(argv[optind++]);
			}
		}
		
		fs::path run_path = get_run_path(config_path);
		if(run_path == ""){
			Logging::log.error("Could not find metadata path.");
			exit(EXIT_FAILURE);
		}
		
		try{
			send_fifo_payload(payload, run_path / "request.pipe");
		}catch(const fifo_exception &err){
			Logging::log.error(err.what());
			exit(126);
		}
		
		Logging::log.message("Waiting for filesystem response...", 2);
		
		try{
			get_fifo_payload(payload, run_path / "response.pipe");
		}catch(const fifo_exception &err){
			Logging::log.error(err.what());
			exit(EXIT_FAILURE);
		}
		
		if(payload.front() == "OK"){
			Logging::log.message("Response OK.", 2);
			for(std::vector<std::string>::iterator itr = std::next(payload.begin()); itr != payload.end(); ++itr){
				Logging::log.message(*itr, 0);
			}
		}else{
			for(std::vector<std::string>::iterator itr = std::next(payload.begin()); itr != payload.end(); ++itr){
				Logging::log.error(*itr);
			}
			exit(EXIT_FAILURE);
		}
	}
	
	return 0;
}
