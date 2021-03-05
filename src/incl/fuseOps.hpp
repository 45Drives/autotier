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

#define FUSE_USE_VERSION 30

#include <rocksdb/db.h>
#include <thread>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

extern "C" {
	#include <fuse.h>
}

class Tier;
class TierEngine;

class FusePriv{
private:
	std::mutex fd_to_path_mt_;
	std::unordered_map<int, char *> fd_to_path_;
	std::mutex size_at_open_mt_;
	std::unordered_map<int, uintmax_t> size_at_open_;
public:
	fs::path config_path_;
	fs::path mount_point_;
	TierEngine *autotier_;
	rocksdb::DB *db_;
	std::vector<Tier *> tiers_;
	std::thread tier_worker_;
	std::thread adhoc_server_;
	~FusePriv(){
		for(std::unordered_map<int, char *>::iterator itr = fd_to_path_.begin(); itr != fd_to_path_.end(); ++itr)
			free(itr->second);
	}
	void insert_fd_to_path(int fd, char *path){
		std::lock_guard<std::mutex> lk(fd_to_path_mt_);
		try{
			char *val = fd_to_path_.at(fd);
			// already defined
			free(val);
			val = strdup(path);
		}catch(const std::out_of_range &){
			// insert new
			fd_to_path_[fd] = strdup(path);
		}
	}
	void remove_fd_to_path(int fd){
		std::lock_guard<std::mutex> lk(fd_to_path_mt_);
		fd_to_path_.erase(fd);
	}
	char *fd_to_path(int fd) const{
		return fd_to_path_.at(fd);
	}
	void insert_size_at_open(int fd, uintmax_t size){
		std::lock_guard<std::mutex> lk(size_at_open_mt_);
		size_at_open_[fd] = size;
	}
	void remove_size_at_open(int fd){
		std::lock_guard<std::mutex> lk(size_at_open_mt_);
		size_at_open_.erase(fd);
	}
	uintmax_t size_at_open(int fd) const{
		return size_at_open_.at(fd);
	}
};

// definitions in helpers.cpp:
namespace l{
	int is_directory(const fs::path &relative_path);
	
	Tier *fullpath_to_tier(fs::path fullpath);
	
	intmax_t file_size(int fd);
	
	intmax_t file_size(const fs::path &path);
	
	void update_keys_in_directory(
		std::string old_directory,
		std::string new_directory,
		::rocksdb::DB *db
	);
}

namespace fuse_ops{
	int getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
	
	int readlink(const char *path, char *buf, size_t size);
	
	int mknod(const char *path, mode_t mode, dev_t rdev);
	
	int mkdir(const char *path, mode_t mode);
	
	int unlink(const char *path);
	
	int rmdir(const char *path);
	
	int symlink(const char *from, const char *to);
	
	int rename(const char *from, const char *to, unsigned int flags);
	
	int link(const char *from, const char *to);
	
	int chmod(const char *path, mode_t mode, struct fuse_file_info *fi);
	
	int chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi);
	
	int truncate(const char *path, off_t size, struct fuse_file_info *fi);
	
	int open(const char *path, struct fuse_file_info *fi);
	
	int read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
	
	int write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
	
	int statfs(const char *path, struct statvfs *stbuf);
	
	int flush(const char *path, struct fuse_file_info *fi);
	
	int release(const char *path, struct fuse_file_info *fi);
	
	int fsync(const char *path, int isdatasync, struct fuse_file_info *fi);
	
	int setxattr(const char *path, const char *name, const char *value, size_t size, int flags);

	int getxattr(const char *path, const char *name, char *value, size_t size);

	int listxattr(const char *path, char *list, size_t size);

	int removexattr(const char *path, const char *name);
	
	int opendir(const char *path, struct fuse_file_info *fi);
	
	int readdir(
		const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct ::fuse_file_info *fi,
		enum fuse_readdir_flags flags
	);
	
	int releasedir(const char *path, struct fuse_file_info *fi);
	
#ifdef USE_FSYNCDIR
	int fsyncdir(const char *path, int isdatasync, struct fuse_file_info *fi);
#endif
	
	void *init(struct fuse_conn_info *conn, struct fuse_config *cfg);
	
	void destroy(void *private_data);
	
	int access(const char *path, int mask);
	
	int create(const char *path, mode_t mode, struct fuse_file_info *fi);
	
#ifdef HAVE_LIBULOCKMGR
	int lock(const char *path, struct ::fuse_file_info *fi, int cmd, struct flock *lock);
#endif
	
	int utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi);
	
	int read_buf(const char *path, struct fuse_bufvec **bufp, size_t size, off_t offset, struct fuse_file_info *fi);
	
	int write_buf(const char *path, struct fuse_bufvec *buf, off_t offset, struct fuse_file_info *fi);
	
	int flock(const char *path, struct fuse_file_info *fi, int op);
	
	int fallocate(const char *path, int mode, off_t offset, off_t length, struct fuse_file_info *fi);
	
	ssize_t copy_file_range(
		const char *path_in, struct fuse_file_info *fi_in, off_t offset_in, const char *path_out,
		struct fuse_file_info *fi_out, off_t offset_out, size_t len, int flags
	);
	
	off_t lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi);
}
