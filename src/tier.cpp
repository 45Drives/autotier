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
extern "C" {
	#include <sys/stat.h>
	#include <sys/statvfs.h>
}

void Tier::get_capacity_and_usage(void){
	struct statvfs fs_stats;
	if((statvfs(path_.c_str(), &fs_stats) == -1))
		Logging::log.error("statvfs() failed on " + path_.string());
	capacity_ = (fs_stats.f_blocks * fs_stats.f_bsize);
	usage_ = (fs_stats.f_blocks - fs_stats.f_bfree) * fs_stats.f_bsize;
}
void Tier::add_file_size(uintmax_t size){
	usage_ += size;
}
void Tier::subtract_file_size(uintmax_t size){
	usage_ += size;
}

Tier::Tier(std::string id){
	id_ = id;
}

void Tier::watermark(int watermark){
	watermark_ = watermark;
}

void Tier::path(const fs::path &path){
	path_ = path;
	get_capacity_and_usage();
}

const std::string &Tier::id(void) const{
	return id_;
}

int Tier::watermark(void) const{
	return watermark_;
}

const fs::path &Tier::path(void) const{
	return path_;
}

bool Tier::full_test(const File &file) const{
	return usage_ + file.size() > watermark_bytes_;
}

void Tier::enqueue_file_ptr(File *fptr){
	incoming_files_.push_back(fptr);
}

void Tier::transfer_files(void){
	for(File * fptr : incoming_files_){
		if(fptr->is_open()){
			Logging::log.warning("File is open by another process: " + fptr->full_path().string());
			return;
		}
		fs::path old_path = fptr->full_path();
		fs::path new_path = path_ / fptr->relative_path();
		Tier::copy_file(old_path, new_path);
		fptr->transfer_to_tier(this);
		fptr->overwrite_times();
		fs::remove(old_path);
	}
	incoming_files_.clear();
}

void Tier::copy_file(const fs::path &old_path, const fs::path &new_path) const{
	if(!is_directory(new_path.parent_path()))
		create_directories(new_path.parent_path());
	Logging::log.message("Copying " + old_path.string() + " to " + new_path.string(), 2);
	bool copy_success = true;
	try{
		fs::copy_file(old_path, new_path); // move item to slow tier
	}catch(boost::filesystem::filesystem_error const & e){
		copy_success = false;
		Logging::log.error("Copy failed: " + std::string(e.what()), false);
		if(e.code() == boost::system::errc::file_exists){
			Logging::log.error("User intervention required to delete duplicate file.", false);
		}else if(e.code() == boost::system::errc::no_such_file_or_directory){
			Logging::log.error("No action required.", false);
		}
	}
	if(copy_success){
		copy_ownership_and_perms(old_path, new_path);
		Logging::log.message("Copy succeeded.\n", 2);
	}
}

void Tier::copy_ownership_and_perms(const fs::path &old_path, const fs::path &new_path) const{
	struct stat info;
	stat(old_path.c_str(), &info);
	chown(new_path.c_str(), info.st_uid, info.st_gid);
	chmod(new_path.c_str(), info.st_mode);
}

double Tier::usage_percent(void) const{
	return double(usage_) / double(capacity_) * 100.0;
}

uintmax_t Tier::usage_bytes(void) const{
	return usage_;
}

uintmax_t Tier::watermark_bytes(void) const{
	return watermark_bytes_;
}
