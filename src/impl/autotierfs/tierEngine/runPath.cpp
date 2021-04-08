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
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <grp.h>
}

int TierEngine::create_run_path(void) const{
	if(access(run_path_.c_str(), F_OK) != 0){
		try{
			fs::create_directories(run_path_);
		}catch(const boost::system::error_code &ec){
			Logging::log.error("Error while creating run path: " + ec.message());
			return -1;
		}
	}
	struct stat st;
	if(stat(run_path_.c_str(), &st) == -1){
		Logging::log.error(std::string("Error while running stat on run path: ") + strerror(errno));
		return -1;
	}
	if(st.st_mode != 0775){
		mode_t new_mask = 002;
		mode_t old_mask = umask(new_mask);
		int res = chmod(run_path_.c_str(), 0775);
		umask(old_mask);
		if(res == -1){
			Logging::log.error(std::string("Error while running chmod on run path: ") + strerror(errno));
			return -1;
		}
	}
	struct group *group = getgrnam("autotier");
	if(group != nullptr && st.st_gid != group->gr_gid){
		if(chown(run_path_.c_str(), -1, group->gr_gid) == -1){
			Logging::log.error(std::string("Error while running chown on run path: ") + strerror(errno));
			return -1;
		}
	}
	return 0;
}
