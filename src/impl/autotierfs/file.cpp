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

#include "file.hpp"
#include "popularityCalc.hpp"
#include "alert.hpp"
#include "tier.hpp"
#include <sstream>

extern "C" {
	#include <sys/time.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <sys/wait.h>
}

File::File(void) :
	size_(0),
	tier_ptr_(nullptr),
	times_{{0}, {0}},
	atime_(0),
	ctime_(0),
	relative_path_(""),
	db_(nullptr),
	metadata_(){
		
}

File::File(fs::path full_path, rocksdb::DB *db, Tier *tptr)
		: relative_path_(fs::relative(full_path, tptr->path())), metadata_(relative_path_.c_str(), db, tptr){
	tier_ptr_ = tptr;
	struct stat info;
	lstat(full_path.c_str(), &info);
	size_ = {info.st_size};
	times_[0].tv_sec = info.st_atim.tv_sec;
	times_[0].tv_usec = info.st_atim.tv_nsec / 1000;
	times_[1].tv_sec = info.st_mtim.tv_sec;
	times_[1].tv_usec = info.st_mtim.tv_nsec / 1000;
	atime_ = times_[0].tv_sec;
	ctime_ = info.st_ctim.tv_sec;
	db_ = db;
}

File::File(const File &other) :
	size_(other.size_),
	tier_ptr_(other.tier_ptr_),
	times_{other.times_[0], other.times_[1]},
	atime_(other.atime_),
	ctime_(other.ctime_),
	relative_path_(other.relative_path_),
	db_(other.db_),
	metadata_(other.metadata_){
		
}

File::~File(){
	metadata_.update(relative_path_.string(), db_);
}

void File::calc_popularity(double period_seconds){
	if(period_seconds <= 0.0)
		return;
	double usage_frequency = metadata_.access_count_ ? double(metadata_.access_count_) / period_seconds : 0.0;
	double average_period_age = (double)(time(NULL) - ctime_) + period_seconds / 2.0;
	double damping = std::min(average_period_age * SLOPE + START_DAMPING, DAMPING) / period_seconds;
	metadata_.popularity_ = MULTIPLIER * usage_frequency / damping + (1.0 - 1.0 / damping) * metadata_.popularity_;
	metadata_.access_count_ = 0;
}

fs::path File::full_path(void) const{
	if(tier_ptr_)
		return tier_ptr_->path() / relative_path_;
	else
		return metadata_.tier_path_ / relative_path_;
}

fs::path File::relative_path(void) const{
	return relative_path_;
}

Tier *File::tier_ptr(void) const{
	return tier_ptr_;
}

double File::popularity(void) const{
	return metadata_.popularity_;
}

struct timeval File::atime(void) const{
	return times_[0];
}

ffd::Bytes File::size(void) const{
	return size_;
}

void File::pin(void){
	metadata_.pinned_ = true;
}

bool File::is_pinned(void) const{
	return metadata_.pinned_;
}

void File::transfer_to_tier(Tier *tptr){
	tier_ptr_->subtract_file_size(size_);
	tier_ptr_ = tptr;
	tier_ptr_->add_file_size(size_);
	metadata_.tier_path_ = tptr->path().string();
	metadata_.update(relative_path_.string(), db_);
}

void File::overwrite_times(void) const{
	utimes(full_path().c_str(), times_);
}

void File::change_path(const fs::path &new_path){
	std::string old_path = relative_path_.string();
	metadata_.update(new_path.string(), db_, &old_path);
	relative_path_ = new_path;
}
