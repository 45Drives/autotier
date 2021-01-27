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

#pragma once

#include <string>
#include <vector>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#define NUM_COMMANDS 8
enum command_enum {ONESHOT, PIN, UNPIN, STATUS, CONFIG, HELP, LPIN, LPOP, MOUNTPOINT};

int get_command_index(const char *cmd);
/* return switchable index determined by regex match of cmd against
 * each entry in command_list
 */

class AdHoc{
public:
	int cmd_;
	std::vector<std::string> args_;
	AdHoc(int cmd, const std::vector<std::string> &args){
		cmd_ = cmd;
		for(const std::string &s : args){
			args_.emplace_back(s);
		}
	}
	AdHoc(const std::vector<std::string> &work_req){
		cmd_ = get_command_index(work_req.front().c_str());
		for(std::vector<std::string>::const_iterator itr = std::next(work_req.begin()); itr != work_req.end(); ++itr){
			args_.emplace_back(*itr);
		}
	}
	~AdHoc(void) = default;
};

class WorkPipe{
private:
	int fd_;
public:
	WorkPipe(fs::path run_path);
	~WorkPipe(void);
	int get(std::vector<std::string> &payload) const;
	int put(const std::vector<std::string> &payload) const;
	int non_block(void) const;
	int block(void) const;
};

void usage(void);
/* print usage message to std::cout
 */
