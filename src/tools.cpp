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
#include "file.hpp"
#include <iostream>
#include <regex>
#include <vector>
#include <sstream>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

extern "C" {
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <unistd.h>
}

int get_command_index(const char *cmd){
	std::regex command_list[NUM_COMMANDS] = {
		std::regex("^[Oo]neshot|ONESHOT$"),
		std::regex("^[Pp]in|PIN$"),
		std::regex("^[Uu]npin|UNPIN$"),
		std::regex("^[Ss]tatus|STATUS$"),
		std::regex("^[Cc]onfig|CONFIG$"),
		std::regex("^[Hh]elp|HELP$"),
		std::regex("^[Ll]ist-[Pp]ins?|LIST-PINS?$"),
		std::regex("^[Ll]ist-[Pp]opularity|LIST-POPULARITY$")
	};
	for(int itr = 0; itr < NUM_COMMANDS; itr++){
		if(regex_match(cmd, command_list[itr]))
			return itr;
	}
	return MOUNTPOINT;
}

void sanitize_paths(std::vector<std::string> &paths){
	std::vector<std::string> non_existing_args, directories;
	for(std::vector<std::string>::iterator itr = paths.begin(); itr != paths.end(); ++itr){
		fs::path path = (fs::current_path() / *itr).string();
		if(!fs::exists(path))
			non_existing_args.push_back(*itr);
		else if(fs::is_directory(path))
			directories.push_back(*itr);
		else
			itr->assign(fs::canonical(path).string());
	}
	if(!non_existing_args.empty()){
		std::string err_msg = "Some files passed for pinning do not exist. Offender(s):";
		for(const std::string &str : non_existing_args)
			err_msg += " " + str;
		Logging::log.error(err_msg, false);
	}
	if(!directories.empty()){
		std::string err_msg = "autotier cannot pin entire directories. Pass files instead. Offender(s):";
		for(const std::string &str : directories)
			err_msg += " " + str;
		Logging::log.error(err_msg, false);
	}
	if(!non_existing_args.empty() || !directories.empty()){
		exit(EXIT_FAILURE);
	}
}

void send_fifo_payload(const std::vector<std::string> &payload, const fs::path &pipe_path){
	WorkPipe *request_pipe;
	
	try{
		request_pipe = new WorkPipe(pipe_path, O_WRONLY | O_NONBLOCK);
	}catch(const int &errno_){
		switch(errno_){
			case EACCES:
				Logging::log.error("No permission to create pipe.");
				break;
			case EEXIST:
				Logging::log.error("Pipe already exists!");
				break;
			case ENOTDIR:
				Logging::log.error("Path to create pipe in is not a directory.");
				break;
			case ENXIO:
				Logging::log.error(
					"Pipe is not connected, autotier seems to not be mounted.\n"
					"If it is mounted, make sure to run this command as the user who mounted."
				);
				break;
			default:
				Logging::log.error("Unhandled error while creating pipe: " + std::to_string(errno_));
				break;
		}
	}

	if(request_pipe->put(payload) == -1)
		Logging::log.error("Writing to pipe failed.");
	
	delete request_pipe;
}

void get_fifo_payload(std::vector<std::string> &payload, const fs::path &pipe_path){
	WorkPipe *response_pipe;
	
	try{
		response_pipe = new WorkPipe(pipe_path, O_RDONLY);
	}catch(const int &errno_){
		switch(errno_){
			case EACCES:
				Logging::log.error("No permission to create pipe.");
				break;
			case EEXIST:
				Logging::log.error("Pipe already exists!");
				break;
			case ENOTDIR:
				Logging::log.error("Path to create pipe in is not a directory.");
				break;
			case ENXIO:
				Logging::log.error(
					"Pipe is not connected, autotier seems to not be mounted.\n"
					"If it is mounted, make sure to run this command as the user who mounted."
				);
				break;
			default:
				Logging::log.error("Unhandled error while creating pipe: " + std::to_string(errno_));
				break;
		}
	}
	
	if(response_pipe->get(payload) == -1)
		Logging::log.error("Reading from pipe failed. errno: " + std::to_string(errno));
	
	delete response_pipe;
}

WorkPipe::WorkPipe(fs::path pipe_path, int flags){
	int res = mkfifo(pipe_path.c_str(), 0755);
	if(res == -1){
		if(errno != EEXIST) throw errno; // allow fail if exists
	}
	fd_ = open(pipe_path.c_str(), flags);
	if(fd_ == -1){
		throw errno;
	}
}

WorkPipe::~WorkPipe(void){
	close(fd_);
}

int WorkPipe::get(std::vector<std::string> &payload) const{
	payload.clear();
	char buff[256];
	int res;
	std::stringstream ss;
	std::string token;
	while((res = read(fd_, buff, sizeof(buff)))){
		if(res == -1)
			return res;
		ss.write(buff, res);
	}
	while(ss){
		getline(ss, token);
		if(!token.empty() && token[0] != '\0') payload.push_back(token);
	}
	return 0;
}

int WorkPipe::put(const std::vector<std::string> &payload) const{
	char buff[256];
	int res;
	std::stringstream ss;
	for(const std::string &str : payload){
		ss << str << '\n';
	}
	ss << '\0';
	while(ss){
		ss.read(buff, sizeof(buff));
		res = ss.gcount();
		res = write(fd_, buff, res);
		if(res == -1)
			return res;
	}
	return 0;
}

int WorkPipe::set_flags(int flags) const{
	int curr_flags;
	if((curr_flags = fcntl(fd_, F_GETFL, 0)) == -1)
		curr_flags = 0;
	return fcntl(fd_, F_SETFL, curr_flags | flags);
}

int WorkPipe::clear_flags(int flags) const{
	int curr_flags;
	if((curr_flags = fcntl(fd_, F_GETFL, 0)) == -1)
		curr_flags = 0;
	return fcntl(fd_, F_SETFL, curr_flags & ~flags);
}

void usage(){
	Logging::log.message(
		"autotier Copyright (C) 2019-2021  Josh Boudreau <jboudreau@45drives.com>\n"
		"This program is released under the GNU General Public License v3.\n"
		"See <https://www.gnu.org/licenses/> for more details.\n"
		"\n"
		"Usage:\n"
		"  mount filesystem:\n"
		"    autotier [<flags>] <mountpoint> [-o <fuse,options,...>]\n"
		"  ad hoc commands:\n"
		"    autotier [<flags>] <command> [<arg1 arg2 ...>]\n"
		"Commands:\n"
		"  oneshot     - execute tiering only once\n"
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
		"  -o, --fuse-options <comma,separated,list>\n"
		"              - mount options to pass to fuse (see man mount.fuse)\n"
		"  --verbose   - set log level to 2\n"
		"  --quiet     - set log level to 0 (no output)\n",
		1
	);
}
