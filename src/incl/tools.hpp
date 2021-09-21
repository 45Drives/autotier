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
#include <sstream>
#include <vector>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

/**
 * @brief enum for table lookup and to switch on for which ad hoc command to run
 * 
 */
enum command_enum {ONESHOT, PIN, UNPIN, STATUS, CONFIG, HELP, LPIN, LPOP, WHICHTIER, NUM_COMMANDS};

/**
 * @brief return switchable index determined by regex match of cmd against
 * each entry in command_list
 * 
 * @param cmd String to check
 * @return int command_enum or -1 if error
 */
int get_command_index(const std::string &cmd);

/**
 * @brief Make sure file exists and is in the autotier filesystem.
 * 
 * @param paths Paths to check
 */
void sanitize_paths(std::list<std::string> &paths);

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
		cmd_ = get_command_index(work_req.front());
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
 * @brief Print usage help message for autotierfs to stdout
 * 
 */
void fs_usage(void);

/**
 * @brief Print usage help message for autotier to stdout
 * 
 */
void cli_usage(void);
