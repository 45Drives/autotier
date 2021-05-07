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
#include <fstream>
#include <mutex>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#define CONFLICT_LOG_FILE "conflicts.log"

static std::mutex conflict_lock;

static inline void read_conflicts(std::vector<std::string> &conflicts, const std::string &path){
	std::ifstream f(path);
	if(!f)
		return;
	std::string entry;
	while(std::getline(f, entry))
		if(entry.size() > 0 && fs::exists(entry + ".autotier_conflict") && fs::exists(entry + ".autotier_conflict_orig"))
			conflicts.push_back(entry);
	f.close();
}

static inline void write_conflicts(const std::vector<std::string> &conflicts, const std::string &path){
	std::ofstream f(path, std::ofstream::out | std::ofstream::trunc);
	if(!f){
		Logging::log.error("Unable to open conflict log file for writing.");
		return;
	}
	for(const std::string &entry : conflicts){
		f << entry << std::endl;
	}
	f.close();
}

bool check_conflicts(std::vector<std::string> &conflicts, const fs::path &run_path){
	fs::path log_file = run_path / CONFLICT_LOG_FILE;
	{
		std::lock_guard<std::mutex> lk(conflict_lock);
		read_conflicts(conflicts, log_file.string());
		write_conflicts(conflicts, log_file.string());
	}
	return conflicts.size() > 0;
}

void add_conflict(const std::string &path, const fs::path &run_path){
	fs::path log_file = run_path / CONFLICT_LOG_FILE;
	std::vector<std::string> conflicts;
	{
		std::lock_guard<std::mutex> lk(conflict_lock);
		read_conflicts(conflicts, log_file.string());
		conflicts.push_back(path);
		write_conflicts(conflicts, log_file.string());
	}
}
