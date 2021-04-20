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

#include "tierEngine.hpp"
#include "alert.hpp"

extern "C" {
	#include <sys/stat.h>
	#include <sys/time.h>
}

void TierEngine::execute_queued_work(void){
	while(!adhoc_work_.empty()){
		AdHoc work = adhoc_work_.pop();
		switch(work.cmd_){
			case ONESHOT:
				if(work.args_.empty())
					tier();
				else
					tier();
				break;
			case PIN:
				pin_files(work.args_);
				break;
			case UNPIN:
				unpin_files(work.args_);
				break;
			default:
				Logging::log.warning("Trying to execute bad ad hoc command.");
				break;
		}
	}
}

void TierEngine::pin_files(const std::vector<std::string> &args){
	Tier *tptr;
	std::string tier_id = args.front();
	if((tptr = tier_lookup(tier_id)) == nullptr){
		Logging::log.warning("Tier does not exist, cannot pin files. Tier name given: " + tier_id);
		return;
	}
	for(std::vector<std::string>::const_iterator fptr = std::next(args.begin()); fptr != args.end(); ++fptr){
		fs::path mounted_path = *fptr;
		fs::path relative_path = fs::relative(mounted_path, mount_point_);
		Metadata f(relative_path.string(), db_);
		if(f.not_found()){
			Logging::log.warning("File to be pinned was not in database: " + mounted_path.string());
			continue;
		}
		fs::path old_path = f.tier_path()/relative_path;
		fs::path new_path = tptr->path()/relative_path;
		struct stat st;
		if(stat(old_path.c_str(), &st) == -1){
			Logging::log.warning("stat failed on " + old_path.string() + ": " + strerror(errno));
			continue;
		}
		struct timeval times[2];
		times[0].tv_sec = st.st_atim.tv_sec;
		times[0].tv_usec = st.st_atim.tv_nsec / 1000;
		times[1].tv_sec = st.st_mtim.tv_sec;
		times[1].tv_usec = st.st_mtim.tv_nsec / 1000;
		if(tptr->move_file(old_path, new_path, config_.copy_buff_sz())){
			utimes(new_path.c_str(), times);
			f.pinned(true);
			f.tier_path(tptr->path().string());
			f.update(relative_path.string(), db_);
		}
	}
	if(!config_.strict_period())
		tier();
}

void TierEngine::unpin_files(const std::vector<std::string> &args){
	for(const std::string &mounted_path : args){
		fs::path relative_path = fs::relative(mounted_path, mount_point_);
		Metadata f(relative_path.c_str(), db_);
		if(f.not_found()){
			Logging::log.warning("File to be unpinned was not in database: " + mounted_path);
			continue;
		}
		f.pinned(false);
		f.update(relative_path.c_str(), db_);
	}
}
