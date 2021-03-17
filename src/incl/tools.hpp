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

#define NUM_COMMANDS 9
enum command_enum {ONESHOT, PIN, UNPIN, STATUS, CONFIG, HELP, LPIN, LPOP, WHICHTIER};

//enum FIFO_WHO {SERVER, CLIENT};
class fifo_exception : public std::exception{
private:
	const char *what_;
public:
	fifo_exception(const char *what) : what_(what) {}
	fifo_exception(const std::string &what) : what_(what.c_str()) {}
	const char *what(void) const noexcept{
		return what_;
	}
};

int get_command_index(const char *cmd);
/* return switchable index determined by regex match of cmd against
 * each entry in command_list
 */

void sanitize_paths(std::list<std::string> &paths);
/* Make sure file exists and is in the autotier filesystem.
 */

void send_fifo_payload(const std::vector<std::string> &payload, const fs::path &run_path);
/* Open FIFO non-blocking and try to send a payload to the ad hoc server.
 */

void get_fifo_payload(std::vector<std::string> &payload, const fs::path &run_path);
/* Open FIFO with blocking IO and wait for a command to be sent.
 */

class AdHoc{
	/* Representation of an ad hoc command with the command index and
	 * arguments.
	 */
public:
	int cmd_;
	/* Index of command in comman_enum.
	 */
	std::vector<std::string> args_;
	/* Arguments passed to command
	 */
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
	/* Object to represent the named FIFO through which
	 * ad hoc work is passed.
	 */
private:
	int fd_;
	/* File descriptor of the FIFO.
	 */
public:
	WorkPipe(fs::path pipe_path, int flags);
	/* Create the FIFO with mkfifo(), then open it
	 * with the provided flags.
	 */
	~WorkPipe(void);
	/* Close fd_.
	 */
	int get(std::vector<std::string> &payload) const;
	/* Get work payload.
	 */
	int put(const std::vector<std::string> &payload) const;
	/* Put work payload.
	 */
	int set_flags(int flags) const;
	/* Use fcntl to set flags to curr_flags | flags.
	 */
	int clear_flags(int flags) const;
	/* Use fcntl to set flags to curr_flags & ~flags.
	 */
};

void fs_usage(void);
void cli_usage(void);
/* print usage message to std::cout
 */
