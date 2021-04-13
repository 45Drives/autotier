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
	#include <grp.h>
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
		std::regex("^[Ll]ist-[Pp]opularity|LIST-POPULARITY$"),
		std::regex("^[Ww]hich-[Tt]ier|WHICH-TIER$")
	};
	for(int itr = 0; itr < NUM_COMMANDS; itr++){
		if(regex_match(cmd, command_list[itr]))
			return itr;
	}
	return -1;
}

void sanitize_paths(std::list<std::string> &paths){
	std::vector<std::list<std::string>::iterator> non_existing_args, directories;
	for(std::list<std::string>::iterator itr = paths.begin(); itr != paths.end(); ++itr){
		fs::path path = *itr;
		if(path.is_relative())
			path = (fs::current_path() / *itr);
		if(!fs::exists(path))
			non_existing_args.push_back(itr);
		else if(fs::is_directory(path))
			directories.push_back(itr);
		else
			itr->assign(fs::canonical(path).string());
	}
	if(!non_existing_args.empty()){
		std::string err_msg = "Some files passed do not exist. Offender(s):";
		for(const std::list<std::string>::iterator &itr : non_existing_args){
			err_msg += "\n" + *itr;
			paths.erase(itr);
		}
		Logging::log.warning(err_msg);
	}
	if(!directories.empty()){
		std::string err_msg = "autotier cannot process entire directories. Pass files instead, or use `find <path> -type f | xargs autotier <command>`. Offender(s):";
		for(const std::list<std::string>::iterator &itr : directories){
			err_msg += "\n" + *itr;
			paths.erase(itr);
		}
		Logging::log.warning(err_msg);
	}
}

void send_fifo_payload(const std::vector<std::string> &payload, const fs::path &pipe_path){
	WorkPipe *request_pipe;
	
	try{
		request_pipe = new WorkPipe(pipe_path, O_WRONLY | O_NONBLOCK);
	}catch(const int &errno_){
		switch(errno_){
			case ENOENT:
				throw(fifo_exception("Pipe does not exist, has autotier been mounted?"));
			case EACCES:
				throw(fifo_exception("No permission to create pipe."));
			case EEXIST:
				throw(fifo_exception("Pipe already exists!"));
			case ENOTDIR:
				throw(fifo_exception("Path to create pipe in is not a directory."));
			case ENXIO:
				throw(fifo_exception(
					"Pipe is not connected, autotier seems to not be mounted. "
					"If it is mounted, make sure to run this command as the user who mounted."
				));
			default:
				throw(fifo_exception("Unhandled error while creating pipe: " + std::to_string(errno_)));
		}
	}

	if(request_pipe->put(payload) == -1)
		throw(fifo_exception("Writing to pipe failed."));
	
	delete request_pipe;
}

void get_fifo_payload(std::vector<std::string> &payload, const fs::path &pipe_path){
	WorkPipe *response_pipe;
	
	try{
		response_pipe = new WorkPipe(pipe_path, O_RDONLY);
	}catch(const int &errno_){
		switch(errno_){
			case ENOENT:
				throw(fifo_exception("Pipe does not exist!"));
			case EACCES:
				throw(fifo_exception("No permission to create pipe."));
			case EEXIST:
				throw(fifo_exception("Pipe already exists!"));
			case ENOTDIR:
				throw(fifo_exception("Path to create pipe in is not a directory."));
			case ENXIO:
				throw(fifo_exception(
					"Pipe is not connected, autotier seems to not be mounted."
					"If it is mounted, make sure to run this command as the user who mounted."
				));
			case EINTR:
				Logging::log.message("Ad hoc server woken from blocked IO.", 2);
				return;
			default:
				throw(fifo_exception("Unhandled error while creating pipe: " + std::to_string(errno_)));
		}
	}
	
	if(response_pipe->get(payload) == -1){
		switch(errno){
			case EINTR:
				Logging::log.message("Ad hoc server woken from blocked IO.", 2);
				return;
			default:
				throw(fifo_exception("Reading from pipe failed. errno: " + std::to_string(errno)));
		}
	}
	
	delete response_pipe;
}

WorkPipe::WorkPipe(fs::path pipe_path, int flags){
	if(access(pipe_path.c_str(), F_OK) != 0){
		// create pipe
		mode_t new_mask = 002;
		mode_t old_mask = umask(new_mask);
		if(mkfifo(pipe_path.c_str(), 0660) == -1){
			if(errno != EEXIST) throw errno; // allow fail if exists
		}
		umask(old_mask);
		struct group *group = getgrnam("autotier");
		if(group != nullptr){
			if(chown(pipe_path.c_str(), -1, group->gr_gid) == -1){
				throw errno;
			}
		}
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

void cli_usage(){
	Logging::log.message(
		"autotier Copyright (C) 2019-2021  Josh Boudreau <jboudreau@45drives.com>\n"
		"This program is released under the GNU General Public License v3.\n"
		"See <https://www.gnu.org/licenses/> for more details.\n"
		"\n"
		"Usage:\n"
		"  autotier [<flags>] <command> [<arg1 arg2 ...>]\n"
		"Commands:\n"
		"  config      - display current configuration values\n"
		"  help        - display this message\n"
		"  list-pins   - show all pinned files\n"
		"  list-popularity\n"
		"              - print list of all tier files sorted by frequency of use\n"
		"  oneshot [<fake tier period>]\n"
		"              - execute tiering only once\n"
		"              - a fake period (integer, seconds) can be passed when using\n"
		"                with cron so file popularity can still be calculated\n"
		"  pin <\"tier name\"> <\"path/to/file\" \"path/to/file\" ...>\n"
		"              - pin file(s) to tier using tier name in config file\n"
		"  status      - list info about defined tiers\n"
		"  unpin <\"path/to/file\" \"path/to/file\" ...>\n"
		"              - remove pin from file(s)\n"
		"  which-tier <\"path/to/file\" \"path/to/file\" ...>\n"
		"              - list which tier each argument is in\n"
		"Flags:\n"
		"  -c, --config <path/to/config>\n"
		"              - override configuration file path (default /etc/autotier.conf)\n"
		"  -h, --help  - display this message and cancel current command\n"
		"  -q, --quiet - set log level to 0 (no output)\n"
		"  -v, --verbose\n"
		"              - set log level to 2 (debug output)\n"
		"  -V, --version\n"
		"              - print version and exit\n"
		"              - if log level >= 1, logo will also print\n"
		"              - combine with -q to mute logo output",
		0
	);
}

void fs_usage(){
	Logging::log.message(
		"autotierfs Copyright (C) 2019-2021  Josh Boudreau <jboudreau@45drives.com>\n"
		"This program is released under the GNU General Public License v3.\n"
		"See <https://www.gnu.org/licenses/> for more details.\n"
		"\n"
		"Usage:\n"
		"  autotierfs [<flags>] <mountpoint> [-o <fuse,options,...>]\n"
		"Flags:\n"
		"  -c, --config <path/to/config>\n"
		"              - override configuration file path (default /etc/autotier.conf)\n"
		"  -h, --help  - display this message and cancel current command\n"
		"  -o, --fuse-options <comma,separated,list>\n"
		"              - mount options to pass to fuse (see man mount.fuse)\n"
		"  -q, --quiet - set log level to 0 (no output)\n"
		"  -v, --verbose\n"
		"              - set log level to 2 (debug output)\n"
		"  -V, --version\n"
		"              - print version and exit\n"
		"              - if log level >= 1, logo will also print\n"
		"              - combine with -q to mute logo output",
		0
	);
}
