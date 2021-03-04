/*
 *    Copyright (C) 2019-2021 Joshua Boudreau <jboudreau@45drives.com>
 * 
 *    Original FUSE passthrough_fh.c authors:
 *    Copyright (C) 2001-2007 Miklos Szeredi <miklos@szeredi.hu>
 *    Copyright (C) 2011      Sebastian Pipping <sebastian@pipping.org>
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

#include "fusePassthrough.hpp"
#include "tier.hpp"
#include "tierEngine.hpp"
#include "metadata.hpp"
#include "alert.hpp"
#include "config.hpp"
#include "openFiles.hpp"
#include "rocksDbHelpers.hpp"
#include <thread>
#include <regex>
#include <unordered_map>

//#define LOG_METHODS // uncomment to enable debug output to journalctl

#define USE_XATTR

#define FUSE_USE_VERSION 30
extern "C"{
	#include <signal.h>
	#include <syslog.h>
	#include <string.h>
	#include <fuse.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <dirent.h>
	#include <errno.h>
	#include <sys/time.h>
	#include <sys/xattr.h>
#ifdef HAVE_LIBULOCKMGR
	#include <ulockmgr.h>
#endif
	#include <sys/file.h>
}

namespace Global{
	fs::path config_path_;
	fs::path mount_point_;
	ConfigOverrides config_overrides_;
}

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

FusePassthrough::FusePassthrough(const fs::path &config_path, const ConfigOverrides &config_overrides){
	Global::config_path_ = config_path;
	Global::config_overrides_ = config_overrides;
}

// helpers

namespace l{
	static int is_directory(const fs::path &relative_path){
		FusePriv *priv = (FusePriv *)fuse_get_context()->private_data;
		fs::file_status status;
		try{
			status = fs::symlink_status(priv->tiers_.front()->path() / relative_path);
		}catch(const boost::system::error_code &ec){
			errno = ec.value();
			return -1;
		}
		return fs::is_directory(status);
	}
	
// 	static void insert_key(int key, char *value, std::map<int, char *> &map){
// 		try{
// 			char *val = map.at(key);
// 			// already defined
// 			free(val);
// 			val = value;
// 		}catch(const std::out_of_range &){
// 			// insert new
// 			map[key] = value;
// 		}
// 	}
// 	
// 	template<class key_t, class value_t>
// 	static void clear_map(std::map<key_t, value_t> &map){
// 		if(typeid(value_t) == typeid(char *))
// 			for(typename std::map<key_t, value_t>::iterator itr = map.begin(); itr != map.end(); ++itr)
// 				free(itr->second);
// 		map.clear();
// 	}
	
	static Tier *fullpath_to_tier(fs::path fullpath){
		FusePriv *priv = (FusePriv *)fuse_get_context()->private_data;
		for(Tier *tptr : priv->tiers_){
			if(std::equal(tptr->path().string().begin(), tptr->path().string().end(), fullpath.string().begin())){
				return tptr;
			}
		}
		return nullptr;
	}
	
	static intmax_t file_size(int fd){
		struct stat st = {};
		int res = fstat(fd, &st);
		if(res == -1)
			return res;
		return st.st_size;
	}
	
	static intmax_t file_size(const fs::path &path){
		struct stat st = {};
		int res = lstat(path.c_str(), &st);
		if(res == -1)
			return res;
		return st.st_size;
	}
	
	static void update_keys_in_directory(
		std::string old_directory,
		std::string new_directory,
		::rocksdb::DB *db
	){
		// Remove leading /.
		if(old_directory.front() == '/'){
			old_directory = old_directory.substr(1, std::string::npos);
		}
		if(new_directory.front() == '/'){
			new_directory = new_directory.substr(1, std::string::npos);
		}
		
		/* Ensure that only paths containing exclusively the changed directory are updated.
		 * EX: subdir and subdir2 exist. If subdir is updated, test should check for "subdir/"
		 * to avoid changing subdir2 aswell.
		 */
		if(old_directory.back() != '/'){
			old_directory += '/';
		}
		if(new_directory.back() != '/'){
			new_directory += '/';
		}
		
		// Batch changes to atomically update keys
		::rocksdb::WriteBatch batch;
		
		::rocksdb::ReadOptions read_options;
		::rocksdb::Iterator *itr = db->NewIterator(read_options);
		for(itr->Seek(old_directory); itr->Valid() && itr->key().starts_with(old_directory); itr->Next()){
			std::string old_path = itr->key().ToString();
			fs::path new_path = new_directory / fs::relative(old_path, old_directory);
			batch.Delete(itr->key());
			batch.Put(new_path.string(), itr->value());
		}
		delete itr;
		{
			std::lock_guard<std::mutex> lk(l::rocksdb::global_lock_);
			db->Write(::rocksdb::WriteOptions(), &batch);
		}
	}
}

// methods

static int at_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "getattr " << path;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	if(fi == NULL){
		int is_directory = l::is_directory(path);
		if(is_directory == -1)
			return -errno;
		if(is_directory){
			res = lstat((priv->tiers_.front()->path() / path).c_str(), stbuf);
		}else{
			Metadata f(path, priv->db_);
#ifdef LOG_METHODS
			{
			std::stringstream ss;
			ss << "getattr file " << path << " found? " << std::boolalpha << !f.not_found();
			Logging::log.message(ss.str(), 0);
			}
#endif
			if(f.not_found())
				return -ENOENT;
			
			fs::path tier_path = f.tier_path();
#ifdef LOG_METHODS
			{
			std::stringstream ss;
			ss << "path: " << (tier_path / path).c_str();
			Logging::log.message(ss.str(), 0);
			}
#endif
			res = lstat((tier_path / path).c_str(), stbuf);
#ifdef LOG_METHODS
			{
			std::stringstream ss;
			ss << "res: " << res;
			Logging::log.message(ss.str(), 0);
			}
#endif
		}
	}else{
#ifdef LOG_METHODS
		{
		std::stringstream ss;
		ss << "getattr fh " << priv->fd_to_path_.at(fi->fh);
		Logging::log.message(ss.str(), 0);
		}
#endif
		res = fstat(fi->fh, stbuf);
	}
	
	if(res == -1)
		return -errno;
	return res;
}

static int at_readlink(const char *path, char *buf, size_t size){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "readlink " << path;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	Metadata f(path, priv->db_);
	if(f.not_found())
		return -ENOENT;
	
	fs::path tier_path = f.tier_path();
	
	res = readlink((tier_path / path).c_str(), buf, size - 1);
	
	if(res == -1)
		return -errno;
	buf[res] = '\0';
	return 0;
}

static int at_mknod(const char *path, mode_t mode, dev_t rdev){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "mknod " << path;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	fs::path fullpath(priv->tiers_.front()->path() / path);
	
	if(S_ISFIFO(mode))
		res = mkfifo(fullpath.c_str(), mode);
	else
		res = mknod(fullpath.c_str(), mode, rdev);
	if(res == -1)
		return -errno;
	
	res = lchown(fullpath.c_str(), ctx->uid, ctx->gid);
	
	if(res == -1)
		return -errno;
	
	Metadata l(path, priv->db_, priv->tiers_.front());
	
	return res;
}

static int at_mkdir(const char *path, mode_t mode){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "mkdir " << path;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	for(Tier *tptr : priv->tiers_){
		res = mkdir((tptr->path() / fs::path(path)).c_str(), mode);
		if(res == -1)
			return -errno;
		res = lchown((tptr->path() / fs::path(path)).c_str(), ctx->uid, ctx->gid);
		if(res == -1)
			return -errno;
	}
	
	return res;
}

static int at_unlink(const char *path){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "unlink " << path;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	Metadata f(path, priv->db_);
	if(f.not_found())
		return -ENOENT;
	
	fs::path tier_path = f.tier_path();
	fs::path full_path = tier_path / path;
	
	Tier *tptr = priv->autotier_->tier_lookup(fs::path(tier_path));
	
	intmax_t size = l::file_size(full_path);
	if(size == -1){
		return -errno;
	}
	tptr->subtract_file_size(size);
	
	res = unlink(full_path.c_str());
	
	if(res == -1)
		return -errno;
	
	{
		std::lock_guard<std::mutex> lk(l::rocksdb::global_lock_);
		if(!priv->db_->Delete(rocksdb::WriteOptions(), path+1).ok())
			return -1;
	}
	
	return res;
}

static int at_rmdir(const char *path){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "rmdir " << path;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	for(Tier *tptr : priv->tiers_){
		res = rmdir((tptr->path() / path).c_str());
		if(res == -1)
			return -errno;
	}
	
	return res;
}

static int at_symlink(const char *from, const char *to){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "symlink " << from << "->" << to;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	res = symlink(from, (priv->tiers_.front()->path() / to).c_str());
	
	if(res == -1)
		return -errno;
	
	res = lchown((priv->tiers_.front()->path() / to).c_str(), ctx->uid, ctx->gid);
	
	if(res == -1)
		return -errno;
	
	Metadata l(to, priv->db_, priv->tiers_.front());
	
	return res;
}

static int at_rename(const char *from, const char *to, unsigned int flags){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "rename " << from << "->" << to;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	if(flags)
		return -EINVAL;
	
	if(l::is_directory(from)){
		for(const Tier *tptr : priv->tiers_){
			res = rename((tptr->path() / from).c_str(), (tptr->path() / to).c_str());
			if(res == -1)
				return -errno;
		}
		l::update_keys_in_directory(from+1, to+1, priv->db_);
	}else{
		Metadata f(from, priv->db_);
		if(f.not_found())
			return -ENOENT;
		fs::path tier_path = f.tier_path();
		
		res = rename((tier_path / from).c_str(), (tier_path / to).c_str());
		if(res == -1)
			return -errno;
		
		std::string key_to_delete(from);
		f.update(to, priv->db_, &key_to_delete);
	}
	
	return res;
}

static int at_link(const char *from, const char *to){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "link " << from << "->" << to;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	Metadata f(from, priv->db_);
	if(f.not_found())
		return -ENOENT;
	fs::path tier_path = f.tier_path();
	
	res = link((tier_path / from).c_str(), (tier_path / to).c_str());
	
	if(res == -1)
		return -errno;
	
	res = lchown(to, ctx->uid, ctx->gid);
	
	if(res == -1)
		return -errno;
	
	Metadata l(to, priv->db_);
	
	return res;
}

static int at_chmod(const char *path, mode_t mode, struct fuse_file_info *fi){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "chmod";
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	if(fi){
#ifdef LOG_METHODS
		Logging::log.message("chmod fh", 0);
#endif
		res = fchmod(fi->fh, mode);
	}else{
#ifdef LOG_METHODS
		Logging::log.message("chmod " + std::string(path), 0);
#endif
		int is_directory = l::is_directory(path);
		if(is_directory == -1)
			return -errno;
		if(is_directory){
			for(Tier *tptr: priv->tiers_){
				res = chmod((tptr->path() / path).c_str(), mode);
				if(res == -1)
					return -errno;
			}
		}else{
			Metadata f(path, priv->db_);
			if(f.not_found())
				return -ENOENT;
			fs::path tier_path = f.tier_path();
			res = chmod((tier_path / path).c_str(), mode);
		}
	}
	
	if(res == -1)
		return -errno;
	return res;
}

static int at_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "chown";
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	if(fi){
#ifdef LOG_METHODS
		Logging::log.message("chown fh", 0);
#endif
		res = fchown(fi->fh, uid, gid);
	}else{
#ifdef LOG_METHODS
		Logging::log.message("chown " + std::string(path), 0);
#endif
		int is_directory = l::is_directory(path);
		if(is_directory == -1)
			return -errno;
		if(is_directory){
			for(Tier *tptr: priv->tiers_){
				res = lchown((tptr->path() / path).c_str(), uid, gid);
				if(res == -1)
					return -errno;
			}
		}else{
			Metadata f(path, priv->db_);
			if(f.not_found())
				return -ENOENT;
			fs::path tier_path = f.tier_path();
			res = lchown((tier_path / path).c_str(), uid, gid);
		}
	}
	
	if(res == -1)
		return -errno;
	return res;
}

static int at_truncate(const char *path, off_t size, struct fuse_file_info *fi){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "truncate";
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	if(fi){
		res = ftruncate(fi->fh, size);
	}else{
		Metadata f(path, priv->db_);
		if(f.not_found())
			return -ENOENT;
		fs::path tier_path = f.tier_path();
		fs::path full_path = tier_path / path;
		res = truncate(full_path.c_str(), size);
	}
	
	if(res == -1)
		return -errno;
	return res;
}

static int at_open(const char *path, struct fuse_file_info *fi){
	int res;
	char *fullpath;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	std::stringstream ss;
	ss << "open " << path << " flags: " << std::bitset<8>(fi->flags);
	Logging::log.message(ss.str(), 0);
#endif
	
	int is_directory = l::is_directory(path);
	if(is_directory == -1)
		return -errno;
	if(is_directory){
		fullpath = strdup((priv->tiers_.front()->path() / path).c_str());
		res = open(fullpath, fi->flags);
		if(res == -1)
			return -errno;
	}else{
		Metadata f(path, priv->db_);
		if(f.not_found())
			return -ENOENT;
		fs::path tier_path = f.tier_path();
		fullpath = strdup((tier_path / path).c_str());
		// get size before open() in case called with truncate
		intmax_t file_size = l::file_size(fs::path(fullpath));
		if(file_size == -1)
			return -errno;
		OpenFiles::register_open_file(fullpath);
		res = open(fullpath, fi->flags);
		priv->insert_size_at_open(res, file_size);
		//priv->size_at_open_.insert(std::pair<int, uintmax_t>(res, file_size));
		if(fi->flags & O_CREAT){
			struct fuse_context *ctx = fuse_get_context();
			int chown_res = fchown(res, ctx->uid, ctx->gid);
			if(chown_res == -1)
				return -errno;
		}
		f.touch();
		f.update(path, priv->db_);
#ifdef LOG_METHODS
		{
		std::stringstream ss;
		ss << "size at open of " << fullpath << ": " << file_size << ", from LUT: " << priv->size_at_open_[res];
		Logging::log.message(ss.str(), 0);
		}
#endif
	}
	
	if(res == -1)
		return -errno;
	fi->fh = res;
	
	priv->insert_fd_to_path(res, fullpath);
// 	l::insert_key(res, fullpath, priv->fd_to_path_);
	
	return 0;
}

static int at_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	int res;
	(void) path;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "read";
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	res = pread(fi->fh, buf, size, offset);
	
	if(res == -1)
		return -errno;
	return res;
}

static int at_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	int res;
	(void) path;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "write";
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	res = pwrite(fi->fh, buf, size, offset);
	
	if(res == -1)
		return -errno;
	
	return res;
}

static int at_statfs(const char *path, struct statvfs *stbuf){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "statfs";
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	struct statvfs fs_stats_temp;
	memset(stbuf, 0, sizeof(struct statvfs));
	
	for(Tier *tptr : priv->tiers_){
		res = statvfs((tptr->path() / path).c_str(), &fs_stats_temp);
		if(res == -1)
			return -errno;
		if(stbuf->f_bsize == 0) stbuf->f_bsize = fs_stats_temp.f_bsize;
		if(stbuf->f_frsize == 0) stbuf->f_frsize = fs_stats_temp.f_frsize;
		stbuf->f_blocks += fs_stats_temp.f_blocks;
		stbuf->f_bfree += fs_stats_temp.f_bfree;
		stbuf->f_bavail += fs_stats_temp.f_bavail;
		stbuf->f_files += fs_stats_temp.f_files;
		if(stbuf->f_ffree == 0) stbuf->f_ffree = fs_stats_temp.f_ffree;
		if(stbuf->f_favail == 0) stbuf->f_favail = fs_stats_temp.f_favail;
	}
	
	return res;
}

static int at_flush(const char *path, struct fuse_file_info *fi){
	int res;
	(void) path;
	
#ifdef LOG_METHODS
	Logging::log.message("flush fh", 0);
#endif
	
	/* This is called from every close on an open file, so call the
	 *	   close on the underlying filesystem.	But since flush may be
	 *	   called multiple times for an open file, this must not really
	 *	   close the file.  This is important if used on a network
	 *	   filesystem like NFS which flush the data/metadata on close() */
	int fh_dup;
	
	fh_dup = dup(fi->fh);
	if(fh_dup == -1)
		return -errno;
	
	res = close(fh_dup);
	
	if(res == -1)
		return -errno;
	return res;
}

static int at_release(const char *path, struct fuse_file_info *fi){
	int res;
	(void) path;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "release";
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	intmax_t old_size = -1;
	intmax_t new_size = -1;
	
	Tier *tptr = nullptr;
	
	try{
		char *fullpath = priv->fd_to_path(fi->fh);
		tptr = l::fullpath_to_tier(fullpath);
		try{
			old_size = priv->size_at_open(fi->fh);
#ifdef LOG_METHODS
			{
			std::stringstream ss;
			ss << "size from LUT: " << priv->size_at_open_.at(fi->fh);
			Logging::log.message(ss.str(), 0);
			}
#endif
			new_size = l::file_size(fi->fh);
			if(new_size == -1)
				return -errno;
#ifdef LOG_METHODS
			{
			std::stringstream ss;
			ss << "size after close: " << fs::file_size(fullpath);
			Logging::log.message(ss.str(), 0);
			}
#endif
		}catch(const std::out_of_range &){
			Logging::log.warning("release: Could not find fd in size map.");
		}
		OpenFiles::release_open_file(fullpath);
		free(fullpath);
		priv->remove_fd_to_path(fi->fh);
		priv->remove_size_at_open(fi->fh);
	}catch(const std::out_of_range &){
		Logging::log.warning("release: Could not find fd in path map.");
	}
	if(old_size != -1 && new_size != -1 && tptr)
		tptr->size_delta(old_size, new_size);
	res = close(fi->fh);
	return res;
}

static int at_fsync(const char *path, int isdatasync, struct fuse_file_info *fi){
	int res;
	
#ifdef LOG_METHODS
		Logging::log.message("fsync fh", 0);
#endif
	
	(void) path;
	
	if(isdatasync)
		res = fdatasync(fi->fh);
	else
		res = fsync(fi->fh);
	
	if(res == -1)
		return -errno;
	
	return 0;
}

#ifdef USE_XATTR
static int at_setxattr(const char *path, const char *name, const char *value, size_t size, int flags){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "setxattr";
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	fs::path fullpath;
	
	int is_directory = l::is_directory(path);
	if(is_directory == -1)
		return -errno;
	if(is_directory){
		for(Tier * const &tier : priv->tiers_){
			fullpath = tier->path() / path;
			res = lsetxattr(fullpath.c_str(), name, value, size, flags);
			if(res == -1)
				return -errno;
		}
	}else{
		Metadata f(path, priv->db_);
		if(f.not_found())
			return -ENOENT;
		fullpath = f.tier_path() + std::string(path);
		res = lsetxattr(fullpath.c_str(), name, value, size, flags);
		if(res == -1)
			return -errno;
	}
	
	return 0;
}

static int at_getxattr(const char *path, const char *name, char *value, size_t size){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "getxattr " << path;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	fs::path fullpath;
	
	int is_directory = l::is_directory(path);
	if(is_directory == -1)
		return -errno;
	if(is_directory){
		fullpath = priv->tiers_.front()->path() / path;
	}else{
		Metadata f(path, priv->db_);
		if(f.not_found())
			return -ENOENT;
		fullpath = f.tier_path() + std::string(path);
	}
	
	res = lgetxattr(fullpath.c_str(), name, value, size);
	
	if(res == -1)
		return -errno;
	
	return res;
}

static int at_listxattr(const char *path, char *list, size_t size){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "listxattr";
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	fs::path fullpath;
	
	int is_directory = l::is_directory(path);
	if(is_directory == -1)
		return -errno;
	if(is_directory){
		fullpath = priv->tiers_.front()->path() / path;
	}else{
		Metadata f(path, priv->db_);
		if(f.not_found())
			return -ENOENT;
		fullpath = f.tier_path() + std::string(path);
	}
	
	res = llistxattr(fullpath.c_str(), list, size);
	if(res == -1)
		return -errno;
	
	return res;
}

static int at_removexattr(const char *path, const char *name){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "removexattr";
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	int is_directory = l::is_directory(path);
	if(is_directory == -1)
		return -errno;
	if(is_directory){
		for(Tier * const &tier : priv->tiers_){
			fs::path fullpath = tier->path() / path;
			res = lremovexattr(fullpath.c_str(), name);
			if(res == -1)
				return -errno;
		}
	}else{
		Metadata f(path, priv->db_);
		if(f.not_found())
			return -ENOENT;
		fs::path fullpath = f.tier_path() + std::string(path);
		res = lremovexattr(fullpath.c_str(), name);
		if(res == -1)
			return -errno;
	}
	
	return 0;
}
#endif

class at_dirp{
public:
	struct dirent *entry;
	off_t offset;
	std::vector<DIR *> dps;
	std::vector<char *> backends;
	at_dirp(){
		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		dps = std::vector<DIR *>(priv->tiers_.size());
		backends = std::vector<char *>(priv->tiers_.size());
	}
	~at_dirp(){
		for(DIR *dp : dps)
			closedir(dp);
		for(char *str : backends)
			free(str);
	}
};

static int at_opendir(const char *path, struct fuse_file_info *fi){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
	class at_dirp *d = new at_dirp();
	if(d == NULL)
		return -ENOMEM;
	
	for(std::vector<Tier *>::size_type i = 0; i < priv->tiers_.size(); i++){
		fs::path backend_path = priv->tiers_[i]->path() / path;
		d->dps[i] = opendir(backend_path.c_str());
		d->backends[i] = strdup(backend_path.c_str());
		if(d->dps[i] == NULL) {
			res = -errno;
			delete d;
			return res;
		}
	}
	
	d->offset = 0;
	d->entry = NULL;

	fi->fh = (unsigned long) d;
	return 0;
}

static inline class at_dirp *get_dirp(struct fuse_file_info *fi){
	return (class at_dirp *) (uintptr_t) fi->fh;
}

static int at_readdir(
	const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi,
	enum fuse_readdir_flags flags
){
	class at_dirp *d = get_dirp(fi);
	std::vector<DIR *>::iterator cur_dir = d->dps.begin();
	
	(void) path;
	
	std::regex temp_file_re("^\\..*\\.autotier\\.hide$");
	
	while(cur_dir != d->dps.end()){
		if(offset != d->offset) {
	#ifndef __FreeBSD__
			seekdir(*cur_dir, offset);
	#else
			/* Subtract the one that we add when calling
			telldir() below */
			seekdir(*cur_dir, offset-1);
	#endif
			d->entry = NULL;
			d->offset = offset;
		}
		while (1) {
			bool skip_dup_dir = false;
			struct stat st;
			off_t nextoff;
			enum fuse_fill_dir_flags fill_flags = (enum fuse_fill_dir_flags) 0;

			if(!d->entry) {
				d->entry = readdir(*cur_dir);
				if(!d->entry)
					break;
				skip_dup_dir = (d->entry->d_type == DT_DIR && cur_dir != d->dps.begin());
			}
	#ifdef HAVE_FSTATAT
			if(flags & FUSE_READDIR_PLUS) {
				int res;

				res = fstatat(dirfd(*cur_dir), d->entry->d_name, &st,
						AT_SYMLINK_NOFOLLOW);
				if(res != -1)
					fill_flags |= FUSE_FILL_DIR_PLUS;
			}
	#endif
			if(!(fill_flags & FUSE_FILL_DIR_PLUS) && !skip_dup_dir) {
				memset(&st, 0, sizeof(st));
				st.st_ino = d->entry->d_ino;
				st.st_mode = d->entry->d_type << 12;
			}
			nextoff = telldir(*cur_dir);
	#ifdef __FreeBSD__		
			/* Under FreeBSD, telldir() may return 0 the first time
			it is called. But for libfuse, an offset of zero
			means that offsets are not supported, so we shift
			everything by one. */
			nextoff++;
	#endif
			if(!skip_dup_dir){
				if(filler(buf, d->entry->d_name, &st, nextoff, fill_flags))
					break;
			}

			d->entry = NULL;
			d->offset = nextoff;
		}
		
		++cur_dir;
	}
	return 0;
}

static int at_releasedir(const char *path, struct fuse_file_info *fi){
	class at_dirp *d = get_dirp(fi);
	(void) path;
	delete d;
	return 0;
}

#ifdef USE_FSYNCDIR
static int at_fsyncdir(const char *path, int isdatasync, struct fuse_file_info *fi){
	int res;
	class at_dirp *d = get_dirp(fi);
	(void) path;
#ifdef LOG_METHODS
	Logging::log.message("fsyncdir fh", 0);
#endif
	for(int i = 0; i < priv->tiers_.size(); i++){
		int fd = open(d->backends[i], O_DIRECTORY);
		if(isdatasync)
			res = fdatasync(fd);
		else
			res = fsync(fd);
		
		if(res == -1)
			return -errno;
	}
	
	return 0;
}
#endif

void *at_init(struct fuse_conn_info *conn, struct fuse_config *cfg){
	(void) conn;
	cfg->use_ino = 1;
	
	/* Pick up changes from lower filesystem right away. This is
	 *		 also necessary for better hardlink support. When the kernel
	 *		 calls the unlink() handler, it does not know the inode of
	 *		 the to-be-removed entry and can therefore not invalidate
	 *		 the cache of the associated inode - resulting in an
	 *		 incorrect st_nlink value being reported for any remaining
	 *		 hardlinks to this inode. */
	
	cfg->entry_timeout = 0;
	cfg->attr_timeout = 0;
	cfg->negative_timeout = 0;
	cfg->nullpath_ok = 1;
	
	FusePriv *priv = new FusePriv;
	
	priv->autotier_ = new TierEngine(Global::config_path_, Global::config_overrides_);
	
	priv->mount_point_ = Global::mount_point_;
	
	priv->autotier_->mount_point(Global::mount_point_);
	
	for(std::list<Tier>::iterator tptr = priv->autotier_->get_tiers().begin(); tptr != priv->autotier_->get_tiers().end(); ++tptr)
		priv->tiers_.push_back(&(*tptr));
	
	priv->db_ = priv->autotier_->get_db();
	
	priv->tier_worker_ = std::thread(&TierEngine::begin, priv->autotier_, true);
	
	priv->adhoc_server_ = std::thread(&TierEngine::process_adhoc_requests, priv->autotier_);
	
#ifdef LOG_METHODS
	pthread_setname_np(priv->tier_worker_.native_handle(), "AT Tier Worker");
	pthread_setname_np(priv->adhoc_server_.native_handle(), "AT AdHoc Server");
	pthread_setname_np(pthread_self(), "AT Fuse Thread");
#endif
	
	return priv;
}

void at_destroy(void *private_data){
	FusePriv *priv = (FusePriv *)private_data;
	
	priv->autotier_->stop();
	priv->tier_worker_.join();
	
	if(pthread_kill(priv->adhoc_server_.native_handle(), SIGUSR1) == -1){
		switch(errno){
			case EINVAL:
				Logging::log.warning("Invalid signal sent to adhoc_server_ thread. `killall -9 autotier` before remounting.");
				break;
			case ESRCH:
				Logging::log.warning("No thread with given ID while trying to kill adhoc_server_");
				break;
			default:
				Logging::log.warning("Could not kill adhoc_server_ thread. `killall -9 autotier` before remounting.");
				break;
		}
		return;
	}
	
	priv->adhoc_server_.join();
	
	delete priv->autotier_;
	
	delete priv;
}

static int at_access(const char *path, int mask){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "access " << path;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	int is_directory = l::is_directory(path);
	if(is_directory == -1)
		return -errno;
	if(is_directory){
		for(Tier *tptr: priv->tiers_){
			res = access((tptr->path() / path).c_str(), mask);
			if(res != 0)
				return res;
		}
	}else{
		Metadata f(path, priv->db_);
		if(f.not_found())
			return -ENOENT;
		fs::path tier_path = f.tier_path();
		res = access((tier_path / path).c_str(), mask);
	}
	
	if(res != 0)
		return res;
	return res;
}

static int at_create(const char *path, mode_t mode, struct fuse_file_info *fi){
#ifdef LOG_METHODS
	{
		std::stringstream ss;
		ss << "create " << path << " mode: " << std::oct << mode << " flags: " << std::bitset<8>(fi->flags);
		Logging::log.message(ss.str(), 0);
	}
#endif
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
	char *fullpath = strdup((priv->tiers_.front()->path() / path).c_str());
	OpenFiles::register_open_file(fullpath);
	res = open(fullpath, fi->flags, mode);
	
#ifdef LOG_METHODS
	{
		std::stringstream ss;
		ss << "fullpath: " << fullpath << " res: " << res;
		Logging::log.message(std::string(ss.str()), 0);
	}
#endif
	
	if(res == -1)
		return -errno;
	
	fi->fh = res;
	
	res = fchown(res, ctx->uid, ctx->gid);
	
	if(res == -1)
		return -errno;
	
	Metadata(path, priv->db_, priv->tiers_.front()).update(path, priv->db_);
	
	priv->insert_fd_to_path(fi->fh, fullpath);
	priv->insert_size_at_open(fi->fh, 0);
	
	return 0;
}

#ifdef HAVE_LIBULOCKMGR
static int at_lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *lock){
	(void) path;

	return ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
			   sizeof(fi->lock_owner));
}
#endif

static int at_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi){
	int res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "utimens " << path;
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	if(fi){
		res = futimens(fi->fh, ts);
	}else{
		int is_directory = l::is_directory(path);
		if(is_directory == -1)
			return -errno;
		if(is_directory){
			for(Tier *tptr: priv->tiers_){
				res = utimensat(0, (tptr->path() / path).c_str(), ts, AT_SYMLINK_NOFOLLOW);
				if(res != 0)
					return res;
			}
		}else{
			Metadata f(path, priv->db_);
			if(f.not_found())
				return -ENOENT;
			fs::path tier_path = f.tier_path();
			/* don't use utime/utimes since they follow symlinks */
			res = utimensat(0, (tier_path / path).c_str(), ts, AT_SYMLINK_NOFOLLOW);
		}
	}
	
	if(res == -1)
		return -errno;
	return res;
}

static int at_write_buf(const char *path, struct fuse_bufvec *buf, off_t offset, struct fuse_file_info *fi){
	struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));
	
	(void) path;
	
	dst.buf[0].flags = (fuse_buf_flags)(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
	dst.buf[0].fd = fi->fh;
	dst.buf[0].pos = offset;
	
	ssize_t bytes_copied = fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
	
	return bytes_copied;
}

static int at_read_buf(const char *path, struct fuse_bufvec **bufp, size_t size, off_t offset, struct fuse_file_info *fi){
	struct fuse_bufvec *src;
	
	(void) path;
	
	src = (struct fuse_bufvec *)malloc(sizeof(struct fuse_bufvec));
	if(src == NULL)
		return -ENOMEM;
	
	*src = FUSE_BUFVEC_INIT(size);
	
	src->buf[0].flags = (fuse_buf_flags)(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
	src->buf[0].fd = fi->fh;
	src->buf[0].pos = offset;
	
	*bufp = src;
	
	return 0;
}

static int at_flock(const char *path, struct fuse_file_info *fi, int op){
	int res;
	(void) path;

	res = flock(fi->fh, op);
	if(res == -1)
		return -errno;

	return 0;
}


static int at_fallocate(const char *path, int mode, off_t offset, off_t length, struct fuse_file_info *fi){
	(void) path;
	
	if(mode)
		return -EOPNOTSUPP;
	
	return -posix_fallocate(fi->fh, offset, length);
}

static ssize_t at_copy_file_range(
	const char *path_in, struct fuse_file_info *fi_in, off_t offset_in, const char *path_out,
	struct fuse_file_info *fi_out, off_t offset_out, size_t len, int flags
){
	ssize_t res;
	(void) path_in;
	(void) path_out;

	res = copy_file_range(fi_in->fh, &offset_in, fi_out->fh, &offset_out, len,
			      flags);
	if(res == -1)
		return -errno;

	return res;
}

static off_t at_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi){
	int fd;
	off_t res;
	
	fuse_context *ctx = fuse_get_context();
	FusePriv *priv = (FusePriv *)ctx->private_data;
	if(!priv)
		return -ECHILD;
	
#ifdef LOG_METHODS
	{
	std::stringstream ss;
	ss << "lseek";
	Logging::log.message(ss.str(), 0);
	}
#endif
	
	if(fi == NULL){
		Metadata f(path, priv->db_);
		if(f.not_found())
			return -ENOENT;
		fs::path tier_path = f.tier_path();
		fd = open((tier_path / path).c_str(), O_RDONLY);
	}else
		fd = fi->fh;
	
	if(fd == -1)
		return -errno;
	
	res = lseek(fd, off, whence);
	if(res == -1)
		res = -errno;
	
	if(fi == NULL)
		close(fd);
	return res;
}


int FusePassthrough::mount_fs(fs::path mountpoint, char *fuse_opts){
	Global::mount_point_ = mountpoint; // global
	Logging::log.message("Mounting filesystem", 2);
	static const struct fuse_operations at_oper = {
		.getattr					= at_getattr,
		.readlink					= at_readlink,
		.mknod						= at_mknod,
		.mkdir						= at_mkdir,
		.unlink						= at_unlink,
		.rmdir						= at_rmdir,
		.symlink					= at_symlink,
		.rename						= at_rename,
		.link						= at_link,
		.chmod						= at_chmod,
		.chown						= at_chown,
		.truncate					= at_truncate,
		.open						= at_open,
		.read						= at_read,
		.write						= at_write,
		.statfs						= at_statfs,
		.flush						= at_flush,
		.release					= at_release,
		.fsync						= at_fsync,
#ifdef USE_XATTR
		.setxattr					= at_setxattr,
		.getxattr					= at_getxattr,
		.listxattr					= at_listxattr,
		.removexattr				= at_removexattr,
#endif
		.opendir					= at_opendir,
		.readdir					= at_readdir,
		.releasedir					= at_releasedir,
#ifdef USE_FSYNCDIR
		.fsyncdir					= at_fsyncdir,
#endif
		.init		 				= at_init,
		.destroy					= at_destroy,
		.access						= at_access,
		.create 					= at_create,
#ifdef HAVE_LIBULOCKMGR
		.lock						= at_lock,
#endif
		.utimens					= at_utimens,
		.write_buf					= at_write_buf,
		.read_buf					= at_read_buf,
		.flock						= at_flock,
		.fallocate					= at_fallocate,
		.copy_file_range 			= at_copy_file_range,
		.lseek						= at_lseek,
	};
	std::vector<char *>argv = {strdup("autotier"), strdup(mountpoint.c_str())};
	if(fuse_opts){
		argv.push_back(strdup("-o"));
		argv.push_back(fuse_opts);
	}
	return fuse_main(argv.size(), argv.data(), &at_oper, NULL);
}
