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
#include "tierEngine.hpp"
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

WorkPipe::WorkPipe(fs::path run_path){
	fs::path pipe_path = run_path / "work.pipe";
	int res = mkfifo(pipe_path.c_str(), 0755);
	if(res == -1){
		if(errno != EEXIST) throw errno; // allow fail if exists
	}
	fd_ = open(pipe_path.c_str(), O_RDWR);
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
		payload.push_back(token);
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

int WorkPipe::non_block(void) const{
	int flags;
	if((flags = fcntl(fd_, F_GETFL, 0)) == -1)
		flags = 0;
	return fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
}

int WorkPipe::block(void) const{
	int flags;
	if((flags = fcntl(fd_, F_GETFL, 0)) == -1)
		flags = 0;
	return fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);
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
