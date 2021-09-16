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

/**
 * @brief enum for table lookup and to switch on for which ad hoc command to run
 * 
 */
enum command_enum {ONESHOT, PIN, UNPIN, STATUS, CONFIG, HELP, LPIN, LPOP, WHICHTIER, NUM_COMMANDS};

/**
 * @brief exception to throw when there is a FIFO issue
 * 
 */
class fifo_exception : public std::exception{
private:
	std::string what_;
public:
	fifo_exception(const char *what) : what_(what) {}
	fifo_exception(const std::string &what) : what_(what.c_str()) {}
	const char *what(void) const noexcept{
		return &what_[0];
	}
};

/**
 * @brief return switchable index determined by regex match of cmd against
 * each entry in command_list
 * 
 * @param cmd String to check
 * @return int command_enum or -1 if error
 */
int get_command_index(const char *cmd);

/**
 * @brief Make sure file exists and is in the autotier filesystem.
 * 
 * @param paths Paths to check
 */
void sanitize_paths(std::list<std::string> &paths);

/**
 * @brief Open FIFO non-blocking and try to send a payload to the ad hoc server.
 * 
 * @param payload Command and arguments to send
 * @param run_path Path to pipe
 */
void send_fifo_payload(const std::vector<std::string> &payload, const fs::path &run_path);


/**
 * @brief Open FIFO with blocking IO and wait for a command to be sent.
 * 
 * @param payload Command and arguments returned by reference
 * @param run_path Path to pipe
 */
void get_fifo_payload(std::vector<std::string> &payload, const fs::path &run_path);

/**
 * @brief Representation of an ad hoc command with the command index and
 * arguments.
 * 
 */
class AdHoc{
public:
	int cmd_; ///< Index of command in comman_enum.
	std::vector<std::string> args_; ///< Arguments passed to command
	/**
	 * @brief Construct a new Ad Hoc object from command_enum and list of args
	 * 
	 * @param cmd command_enum from get_command_index()
	 * @param args list of args to command
	 */
	AdHoc(int cmd, const std::vector<std::string> &args){
		cmd_ = cmd;
		for(const std::string &s : args){
			args_.emplace_back(s);
		}
	}
	/**
	 * @brief Construct a new Ad Hoc object from list of strings
	 * 
	 * @param work_req command (work_req[0]) and args (work_req[1:])
	 */
	AdHoc(const std::vector<std::string> &work_req){
		cmd_ = get_command_index(work_req.front().c_str());
		for(std::vector<std::string>::const_iterator itr = std::next(work_req.begin()); itr != work_req.end(); ++itr){
			args_.emplace_back(*itr);
		}
	}
	/**
	 * @brief Destroy the Ad Hoc object
	 * 
	 */
	~AdHoc(void) = default;
};

/**
 * @brief Object to represent the named FIFO through which
	 * ad hoc work is passed.
 * 
 */
class WorkPipe{
private:
	int fd_; ///< File descriptor of the FIFO.
public:
	/**
	 * @brief Construct a new Work Pipe object.
	 * Create the FIFO with mkfifo(), then open it
	 * with the provided flags.
	 * 
	 * @param pipe_path Path to create FIFO in
	 * @param flags Flags to open FIFO with
	 */
	WorkPipe(fs::path pipe_path, int flags);
	/**
	 * @brief Destroy the Work Pipe object, closing the fd_
	 * 
	 */
	~WorkPipe(void);
	/**
	 * @brief Get work payload.
	 * 
	 * @param payload Payload returned by reference
	 * @return int 0 if okay, -1 if error
	 */
	int get(std::vector<std::string> &payload) const;
	/**
	 * @brief Put work payload.
	 * 
	 * @param payload Payload to send
	 * @return int 0 if okay, -1 if error
	 */
	int put(const std::vector<std::string> &payload) const;
	/**
	 * @brief Use fcntl to set flags to curr_flags | flags.
	 * 
	 * @param flags Flags to set
	 * @return int Result of fcntl()
	 */
	int set_flags(int flags) const;
	/**
	 * @brief Use fcntl to set flags to curr_flags & ~flags.
	 * 
	 * @param flags Flags to clear
	 * @return int Result of fcntl()
	 */
	int clear_flags(int flags) const;
};

/**
 * @brief Print usage help message for autotierfs to stdout
 * 
 */
void fs_usage(void);

/**
 * @brief Print usage help message for autotier to stdout
 * 
 */
void cli_usage(void);
