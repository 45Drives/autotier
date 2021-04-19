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

void TierEngine::execute_queued_work(void){
	while(!adhoc_work_.empty()){
		AdHoc work = adhoc_work_.pop();
		switch(work.cmd_){
			case ONESHOT:
				if(work.args_.empty())
					tier(std::chrono::seconds(-1)); // don't affect period
				else
					tier(std::chrono::seconds(stoi(work.args_[0]))); // won't throw, args_[0] is already validated
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
		Metadata f(relative_path.c_str(), db_);
		if(f.not_found()){
			Logging::log.warning("File to be pinned was not in database: " + mounted_path.string());
			continue;
		}
		Tier *old_tptr = tier_lookup(fs::path(f.tier_path()));
		File file(old_tptr->path() / relative_path, db_, old_tptr);
		file.pin();
		tptr->enqueue_file_ptr(&file);
	}
	tptr->transfer_files(config_.copy_buff_sz());
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
