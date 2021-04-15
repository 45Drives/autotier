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

#include "tier.hpp"
#include "alert.hpp"
#include "openFiles.hpp"
#include "file.hpp"
#include <thread>

extern "C" {
	#include <sys/stat.h>
	#include <sys/statvfs.h>
}

void Tier::copy_ownership_and_perms(const fs::path &old_path, const fs::path &new_path) const{
	struct stat info;
	stat(old_path.c_str(), &info);
	chown(new_path.c_str(), info.st_uid, info.st_gid);
	chmod(new_path.c_str(), info.st_mode);
}

Tier::Tier(std::string id){
	id_ = id;
	usage_ = 0;
}

void Tier::add_file_size(uintmax_t size){
	std::lock_guard<std::mutex> lk(usage_mt_);
	usage_ += size;
}

void Tier::subtract_file_size(uintmax_t size){
	std::lock_guard<std::mutex> lk(usage_mt_);
	usage_ -= size;
}

void Tier::size_delta(intmax_t old_size, intmax_t new_size){
	std::lock_guard<std::mutex> lk(usage_mt_);
	usage_ += new_size - old_size;
}

void Tier::add_file_size_sim(uintmax_t size){
	sim_usage_ += size;
}

void Tier::subtract_file_size_sim(uintmax_t size){
	sim_usage_ -= size;
}

void Tier::quota_percent(double quota_percent){
	quota_percent_ = quota_percent;
}

double Tier::quota_percent(void) const{
	return quota_percent_;
}

void Tier::get_capacity_and_usage(void){
	struct statvfs fs_stats;
	if((statvfs(path_.c_str(), &fs_stats) == -1)){
		Logging::log.error("statvfs() failed on " + path_.string());
		exit(EXIT_FAILURE);
	}
	capacity_ = (fs_stats.f_blocks * fs_stats.f_frsize);
	sim_usage_ = 0;
}

void Tier::calc_quota_bytes(void){
	if(quota_bytes_ == (uintmax_t)-1)
		quota_bytes_ = (double)capacity_ * quota_percent_ / 100.0;
	else if(quota_percent_ == -1.0)
		quota_percent_ = (double)quota_bytes_ * 100.0 / (double)capacity_;
}

void Tier::quota_bytes(uintmax_t quota_bytes){
	quota_bytes_ = quota_bytes;
}

uintmax_t Tier::quota_bytes(void) const{
	return quota_bytes_;
}

bool Tier::full_test(const File &file) const{
	return sim_usage_ + file.size() > quota_bytes_;
}

void Tier::path(const fs::path &path){
	path_ = path;
}

const fs::path &Tier::path(void) const{
	return path_;
}

const std::string &Tier::id(void) const{
	return id_;
}

void Tier::enqueue_file_ptr(File *fptr){
	incoming_files_.push_back(fptr);
}

void Tier::transfer_files(void){
	for(File * fptr : incoming_files_){
		fs::path old_path = fptr->full_path();
		if(OpenFiles::is_open(old_path.string())){
			Logging::log.warning("File is open by another process: " + old_path.string());
			continue;
		}
		fs::path new_path = path_ / fptr->relative_path();
		bool copy_success = Tier::move_file(old_path, new_path);
		if(copy_success){
			fptr->transfer_to_tier(this);
			fptr->overwrite_times();
		}
	}
	incoming_files_.clear();
	sim_usage_ = 0;
}

bool Tier::move_file(const fs::path &old_path, const fs::path &new_path) const{
	fs::path new_tmp_path = new_path.parent_path() / ("." + new_path.filename().string() + ".autotier.hide");
	if(!is_directory(new_path.parent_path()))
		create_directories(new_path.parent_path());
	Logging::log.message("Copying " + old_path.string() + " to " + new_path.string(), 2);
	bool copy_success = true;
	bool out_of_space = false;
	do{
		try{
			fs::copy_file(old_path, new_tmp_path); // move item to slow tier
			copy_success = true;
			out_of_space = false;
		}catch(const boost::filesystem::filesystem_error &e){
			copy_success = false;
			out_of_space = false;
			if(e.code() == boost::system::errc::file_exists){
				Logging::log.error("Copy failed: " + std::string(e.what()));
				Logging::log.error("User intervention required to delete duplicate file: " + new_tmp_path.string());
			}else if(e.code() == boost::system::errc::no_such_file_or_directory){
				Logging::log.error("Copy failed: " + std::string(e.what()));
				Logging::log.error("No action required, file was deleted by another process.");
			}else if(e.code() == boost::system::errc::no_space_on_device){
				// TODO: implement writing in chunks so that on failure it doesn't have to start from the beginning
				out_of_space = true;
				Logging::log.warning("Tier ran out of space while moving files, trying again. ");
				fs::remove(new_tmp_path);
				std::this_thread::yield(); // let another thread run
			}
		}
	}while(out_of_space);
	if(copy_success){
		copy_ownership_and_perms(old_path, new_tmp_path);
		fs::remove(old_path);
		fs::rename(new_tmp_path, new_path);
		Logging::log.message("Copy succeeded.\n", 2);
	}
	return copy_success;
}

void Tier::usage(uintmax_t usage){
	std::lock_guard<std::mutex> lk(usage_mt_);
	usage_ = usage;
}

double Tier::usage_percent(void) const{
	return double(usage_) / double(capacity_) * 100.0;
}

uintmax_t Tier::usage_bytes(void) const{
	return usage_;
}

uintmax_t Tier::capacity(void) const{
	return capacity_;
}
