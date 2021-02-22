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
#include "alert.hpp"
#include "tier.hpp"
#include <sstream>

extern "C" {
	#include <sys/time.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <sys/wait.h>
}

Metadata::Metadata(const char *path, rocksdb::DB *db, Tier *tptr){
	std::string str;
	if(path[0] == '/') path++;
	rocksdb::Status s = db->Get(rocksdb::ReadOptions(), path, &str);
	if(s.ok()){
		std::stringstream ss(str);
		boost::archive::text_iarchive ia(ss);
		this->serialize(ia, 0);
	}else if(tptr){
		tier_path_ = tptr->path().string();
		update(path, db);
	}else{
		not_found_ = true;
	}
}

void Metadata::update(const char *relative_path, rocksdb::DB *db){
	if(relative_path[0] == '/') relative_path++;
	std::stringstream ss;
	{
		boost::archive::text_oarchive oa(ss);
		this->serialize(oa, 0);
	}
	rocksdb::Status s = db->Put(rocksdb::WriteOptions(), relative_path, ss.str());
}

void Metadata::touch(void){
	access_count_++;
}

std::string Metadata::tier_path(void) const{
	return tier_path_;
}

void Metadata::tier_path(const std::string &path){
	tier_path_ = path;
}

bool Metadata::pinned(void) const{
	return pinned_;
}

void Metadata::pinned(bool val){
	pinned_ = val;
}

bool Metadata::not_found(void) const{
	return not_found_;
}

std::string Metadata::dump_stats(void) const{
	std::stringstream ss;
	ss << "tier_path_: " << tier_path_ << std::endl;
	ss << "access_count_: " << access_count_ << std::endl;
	ss << "popularity_: " << popularity_ << std::endl;
	ss << "pinned: " << std::boolalpha << pinned_ << std::noboolalpha << std::endl;
	return ss.str();
}

File::File(fs::path full_path, rocksdb::DB *db, Tier *tptr)
		: relative_path_(fs::relative(full_path, tptr->path())), metadata_(relative_path_.c_str(), db, tptr){
	tier_ptr_ = tptr;
	struct stat info;
	lstat(full_path.c_str(), &info);
	size_ = (uintmax_t)info.st_size;
	times_[0].tv_sec = info.st_atim.tv_sec;
	times_[0].tv_usec = info.st_atim.tv_nsec / 1000;
	times_[1].tv_sec = info.st_mtim.tv_sec;
	times_[1].tv_usec = info.st_mtim.tv_nsec / 1000;
	atime_ = times_[0].tv_sec;
	ctime_ = info.st_ctim.tv_sec;
	db_ = db;
}

File::~File(){
	metadata_.update(relative_path_.c_str(), db_);
}

void File::calc_popularity(double period_seconds){
	if(period_seconds == 0.0)
		return;
	double usage_frequency = metadata_.access_count_ ? double(metadata_.access_count_) / period_seconds : 0.0;
// 	if(metadata_.access_count_ == 1 && metadata_.last_popularity_calc_){
// 		// access period slower than tier period
// 		usage_frequency = double(metadata_.access_count_) / double(now - metadata_.last_popularity_calc_);
// 		metadata_.last_popularity_calc_ = now;
// 	}else if(metadata_.access_count_){
// 		// access period faster than tier period
// 		usage_frequency = double(metadata_.access_count_) / period_seconds;
// 		metadata_.last_popularity_calc_ = now;
// 	}else if(metadata_.last_popularity_calc_){
// 		// access period slower than two tier periods
// 		usage_frequency = 1.0 / double(now - metadata_.last_popularity_calc_);
// 	}else{
// 		// access period slower than two tier periods
// 		double diff = now - atime_;
// 		if(diff < 1.0)
// 			diff = 1.0;
// 		usage_frequency = 1.0 / diff;
// 		metadata_.last_popularity_calc_ = atime_;
// 	}
	double damping = std::min((double)(time(NULL) - ctime_) / 100.0 + START_DAMPING, DAMPING); // dynamically change damping as file ages
	metadata_.popularity_ = MULTIPLIER * usage_frequency / damping + (1.0 - 1.0 / damping) * metadata_.popularity_;
	metadata_.access_count_ = 0;
}

bool File::is_open(void){
	pid_t pid;
	int status;
	int fd;
	int stdout_copy;
	int stderr_copy;
	
	pid = fork();
	switch(pid){
	case -1:
		Logging::log.warning("Error forking while checking if file is open!");
		return false;
	case 0:
		// child
		pid = getpid();
		
		// redirect output to /dev/null
		stdout_copy = dup(STDOUT_FILENO);
		stderr_copy = dup(STDERR_FILENO);
		fd = open("/dev/null", O_WRONLY);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		close(fd);
		
		execlp("lsof", "lsof", full_path().c_str(), (char *)NULL);
		
		// undo redirect
		dup2(stdout_copy, STDOUT_FILENO);
		close(stdout_copy);
		dup2(stderr_copy, STDERR_FILENO);
		close(stderr_copy);
		
		Logging::log.error("Error executing lsof! Is it installed?");
		// error exits, ignore following:
		// fall through
	default:
		// parent
		if((waitpid(pid, &status, 0)) < 0)
			Logging::log.warning("Error waiting for lsof to exit.");
		break;
	}
	
	if(WIFEXITED(status)){
		switch(WEXITSTATUS(status)){
		case 0:
			return true;
		case 1:
			return false;
		default:
			Logging::log.warning("Unexpected lsof exit status! Exit status: " + std::to_string(WEXITSTATUS(status)));
			return false;
		}
	}else{
		Logging::log.warning("Error reading lsof exit status!");
		return false;
	}
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

uintmax_t File::size(void) const{
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
	metadata_.update(relative_path_.c_str(), db_);
}

void File::overwrite_times(void) const{
	utimes(full_path().c_str(), times_);
}
