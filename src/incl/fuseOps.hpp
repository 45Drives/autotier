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
#include <mutex>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

extern "C" {
	#include <fuse.h>
}

class Tier;
class TierEngine;

/**
 * @brief Fuse Private data class grabbed from fuse_get_context()->private_data (void*)
 * in fuse filesystem functions
 * 
 */
class FusePriv{
public:
	fs::path config_path_; ///< Path to configuration file TODO: see if this is used anywhere
	fs::path mount_point_; ///< Path to autotierfs mount point
	TierEngine *autotier_; ///< Pointer to TierEngine
	rocksdb::DB *db_; ///< RocksDB database holding file metadata
	std::vector<Tier *> tiers_; ///< List of pointers to tiers from TierEngine
	std::thread tier_worker_; ///< Thread running TierEngineTiering::begin()
	std::thread adhoc_server_; ///< Thread running TierEngineAdhoc::process_adhoc_requests()
	/**
	 * @brief Destroy the Fuse Priv object,
	 * freeing all cstrings in fd_to_path_
	 * 
	 */
	~FusePriv(){
		for(std::unordered_map<int, char *>::iterator itr = fd_to_path_.begin(); itr != fd_to_path_.end(); ++itr)
			free(itr->second);
	}
	/**
	 * @brief Insert fd : path pair into fd_to_path_ map
	 * 
	 * @param fd File descriptor
	 * @param path Path to file
	 */
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
	/**
	 * @brief Remove fd : path pair from fd_to_path_ map
	 * 
	 * @param fd File descriptor to remove
	 */
	void remove_fd_to_path(int fd){
		std::lock_guard<std::mutex> lk(fd_to_path_mt_);
		try {
			char *val = fd_to_path_.at(fd);
			free(val);
		} catch (const std::out_of_range &) { /* do nothing */ }
		fd_to_path_.erase(fd);
	}
	/**
	 * @brief Return path corresponding to file descriptor
	 * 
	 * @param fd File descriptor
	 * @return char* Path to file
	 */
	char *fd_to_path(int fd) const{
		return fd_to_path_.at(fd);
	}
	/**
	 * @brief Insert fd : size at open pair into size_at_open_ map
	 * 
	 * @param fd File descriptor
	 * @param size Size of file at open
	 */
	void insert_size_at_open(int fd, uintmax_t size){
		std::lock_guard<std::mutex> lk(size_at_open_mt_);
		size_at_open_[fd] = size;
	}
	/**
	 * @brief Remove fd : size at open pair from size_at_open_ map
	 * 
	 * @param fd File descriptor to remove
	 */
	void remove_size_at_open(int fd){
		std::lock_guard<std::mutex> lk(size_at_open_mt_);
		size_at_open_.erase(fd);
	}
	/**
	 * @brief Return file size at open corresponding to fd
	 * to determine size delta at close
	 * 
	 * @param fd File descriptor
	 * @return uintmax_t Size of file at open
	 */
	uintmax_t size_at_open(int fd) const{
		return size_at_open_.at(fd);
	}
private:
	/**
	 * @brief Mutex to synchronize insertion and deletion from fd_to_path_ map
	 * 
	 */
	std::mutex fd_to_path_mt_;
	/**
	 * @brief Map relating file descriptor to file path
	 * 
	 */
	std::unordered_map<int, char *> fd_to_path_;
	/**
	 * @brief Mutex to synchronize insertion and deletion from size_at_open_ map
	 * 
	 */
	std::mutex size_at_open_mt_;
	/**
	 * @brief Map relating file descriptor to file size at open
	 * 
	 */
	std::unordered_map<int, uintmax_t> size_at_open_;
};

// definitions in helpers.cpp:
/**
 * @brief Local namespace
 * 
 */
namespace l{
	/**
	 * @brief Test if path is a directory
	 * 
	 * @param relative_path Relative path to test
	 * @return int 0 (false) if not a directory, 1 (true) if a directory, -1 if error
	 */
	int is_directory(const fs::path &relative_path);
	/**
	 * @brief Find tier containing full path
	 * 
	 * @param fullpath Full path to test
	 * @return Tier* Pointer to tier containing path or nullptr if not found
	 */
	Tier *fullpath_to_tier(fs::path fullpath);
	/**
	 * @brief Get size of file from file descriptor
	 * 
	 * @param fd File descriptor to check
	 * @return intmax_t Size of file or -1 if error
	 */
	intmax_t file_size(int fd);
	/**
	 * @brief Get size of file from path
	 * 
	 * @param path File path to check
	 * @return intmax_t Size of file or -1 if error
	 */
	intmax_t file_size(const fs::path &path);
	/**
	 * @brief Iterate through RocksDB database, updating all
	 * paths after old_directory to new_directory for when a directory
	 * is moved
	 * 
	 * @param old_directory Old path name
	 * @param new_directory New path name
	 * @param db RocksDB database to modify
	 */
	void update_keys_in_directory(
		std::string old_directory,
		std::string new_directory,
		::rocksdb::DB *db
	);
}

/**
 * @brief Namespace to hold fuse operations.
 * See fuse3/fuse.h documentation for descriptions of each function.
 * 
 */
namespace fuse_ops{
	extern TierEngine *autotier_ptr;
	
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
