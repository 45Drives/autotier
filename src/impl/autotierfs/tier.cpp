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
#include <cstdlib>

extern "C" {
	#include <sys/stat.h>
	#include <sys/statvfs.h>
	#include <fcntl.h>
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

void Tier::transfer_files(int buff_sz){
	for(File * fptr : incoming_files_){
		fs::path old_path = fptr->full_path();
		if(OpenFiles::is_open(old_path.string())){
			Logging::log.warning("File is open by another process: " + old_path.string());
			continue;
		}
		fs::path new_path = path_ / fptr->relative_path();
		bool copy_success = Tier::move_file(old_path, new_path, buff_sz);
		if(copy_success){
			fptr->transfer_to_tier(this);
			fptr->overwrite_times();
		}
	}
	incoming_files_.clear();
	sim_usage_ = 0;
}

bool Tier::move_file(const fs::path &old_path, const fs::path &new_path, int buff_sz) const{
	fs::path new_tmp_path = new_path.parent_path() / ("." + new_path.filename().string() + ".autotier.hide");
	if(!is_directory(new_path.parent_path()))
		create_directories(new_path.parent_path());
	Logging::log.message("Copying " + old_path.string() + " to " + new_path.string(), 2);
	bool copy_success = true;
	bool out_of_space = false;
	char *buff = new char[buff_sz];
	off_t offset = 0;
	int res;
	int source_fd;
	int dest_fd;
	source_fd = open(old_path.c_str(), O_RDONLY);
	if(source_fd == -1)
		goto copy_error_out;
	dest_fd = open(new_tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if(dest_fd == -1)
		goto copy_error_out;
	off_t bytes_read;
	off_t bytes_written;
	do{
		while((bytes_read = read(source_fd, buff, buff_sz)) > 0){
			out_of_space = false;
			bytes_written = write(dest_fd, buff, bytes_read);
			if((bytes_written == (off_t)-1 && errno == ENOSPC) || bytes_written < bytes_read){
				if(bytes_written != (off_t)-1)
					offset += bytes_written; // seek to latest written byte
				out_of_space = true;
				res = lseek(source_fd, offset, SEEK_SET);
				if(res == (off_t)-1)
					goto copy_error_out;
				res = lseek(dest_fd, offset, SEEK_SET);
				if(res == (off_t)-1)
					goto copy_error_out;
				Logging::log.message("Tier ran out of space while moving files, trying again.", 2);
				std::this_thread::yield(); // let another thread run
			}else if(bytes_written != bytes_read)
				goto copy_error_out;
			else // copy okay
				offset += bytes_written;
		}
		if(bytes_read == -1)
			goto copy_error_out;
	}while(out_of_space);
	delete[] buff;
	if(close(source_fd) == -1)
		goto copy_error_out;
	if(close(dest_fd) == -1)
		goto copy_error_out;
	if(copy_success){
		copy_ownership_and_perms(old_path, new_tmp_path);
		fs::remove(old_path);
		fs::rename(new_tmp_path, new_path);
		Logging::log.message("Copy succeeded.\n", 2);
	}
	
	return copy_success;
	
copy_error_out:
	char *why = strerror(errno);
	Logging::log.error(std::string("Copy failed: ") + why);
	return false;
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
