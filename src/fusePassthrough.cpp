/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2020			 Josh Boudreau <jboudreau@45drives.com>
	Copyright (C) 2001-2007	Miklos Szeredi <miklos@szeredi.hu>
	Copyright (C) 2011			 Sebastian Pipping <sebastian@pipping.org>

	This program can be distributed under the terms of the GNU GPLv2.
	See the file COPYING.
*/

/** @file
 *
 * Adapted from example code from libfuse, handles filsystem calls
 *
 * Compile with:
 *
 *		 gcc -Wall passthrough.c `pkg-config fuse3 --cflags --libs` -lulockmgr -o passthrough_fh
 */

#define HAVE_UTIMENSAT

#include "fusePassthrough.hpp"
#include "tierEngine.hpp"
#include "file.hpp"
#include "alert.hpp"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#ifdef __FreeBSD__
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <list>
#include <fuse.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

static sqlite3 *db;

static std::list<Tier *> tiers;

fs::path _mountpoint_;

FusePassthrough::FusePassthrough(std::list<Tier> &tiers_){
	open_db();
	for(std::list<Tier>::iterator tptr = tiers_.begin(); tptr != tiers_.end(); ++tptr){
		tiers.push_back(&(*tptr));
	}
}

FusePassthrough::~FusePassthrough(){
	sqlite3_close(db);
}

void FusePassthrough::open_db(){
	int res = sqlite3_open(RUN_PATH "/db.sqlite", &db);
	char *errMsg = 0;
	if(res){
		std::cerr << "Error opening database: " << sqlite3_errmsg(db);
		exit(res);
	}else{
		Log("Opened database successfully", 2);
	}
	
	const char *sql =
	"CREATE TABLE IF NOT EXISTS Files("
	"	ID INT PRIMARY KEY NOT NULL,"
	"	RELATIVE_PATH TEXT NOT NULL,"
	"	CURRENT_TIER TEXT,"
	"	PIN TEXT,"
	"	POPULARITY REAL,"
	"	LAST_ACCESS INT"
	");";
		
	res = sqlite3_exec(db, sql, NULL, 0, &errMsg);
	
	if(res){
		std::cerr << "Error creating table: " << sqlite3_errmsg(db);
		exit(res);
	}
}

// helpers

static int is_file(fs::path p){
	for(Tier *tptr : tiers){
		if(fs::is_symlink(tptr->dir / p)){
			p = fs::relative(fs::canonical(tptr->dir / p), tptr->dir);
		}
		if(fs::is_regular_file(tptr->dir / p))
			return true;
	}
	return false;
}

static int mknod_wrapper(int dirfd, const char *path, const char *link,
	int mode, dev_t rdev)
{
	int res;

	if (S_ISREG(mode)) {
		res = openat(dirfd, path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISDIR(mode)) {
		res = mkdirat(dirfd, path, mode);
	} else if (S_ISLNK(mode) && link != NULL) {
		res = symlinkat(link, dirfd, path);
	} else if (S_ISFIFO(mode)) {
		res = mkfifoat(dirfd, path, mode);
#ifdef __FreeBSD__
	} else if (S_ISSOCK(mode)) {
		struct sockaddr_un su;
		int fd;

		if (strlen(path) >= sizeof(su.sun_path)) {
			errno = ENAMETOOLONG;
			return -1;
		}
		fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd >= 0) {
			/*
			 * We must bind the socket to the underlying file
			 * system to create the socket file, even though
			 * we'll never listen on this socket.
			 */
			su.sun_family = AF_UNIX;
			strncpy(su.sun_path, path, sizeof(su.sun_path));
			res = bindat(dirfd, fd, (struct sockaddr*)&su,
				sizeof(su));
			if (res == 0)
				close(fd);
		} else {
			res = -1;
		}
#endif
	} else {
		res = mknodat(dirfd, path, mode, rdev);
	}

	return res;
}

// methods

static int at_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi){
	(void) fi;
	fs::path p(path);
	if(is_file(p)){
		File f(p, db);
		int res;

		res = lstat(f.old_path.c_str(), stbuf);
		if (res == -1)
			return -errno;
	}else{
		int res;
		
		res = lstat((tiers.front()->dir / p).c_str(), stbuf);
		if (res == -1)
			return -errno;
	}
	return 0;
}

static int at_readlink(const char *path, char *buf, size_t size){
	File f(path, db);
	int res;
	
	res = readlink(f.old_path.c_str(), buf, size - 1);
	if (res == -1)
		return -errno;
	
	buf[res] = '\0';
	return 0;
}

static int at_mknod(const char *path, mode_t mode, dev_t rdev){
	File f(path, db);
	int res;

		res = mknod_wrapper(AT_FDCWD, f.old_path.c_str(), NULL, mode, rdev);
		if (res == -1)
			return -errno;

		return 0;
}

static int at_mkdir(const char *path, mode_t mode){
	int res;
	
	for(Tier *tptr : tiers){
		res = mkdir((tptr->dir / fs::path(path)).c_str(), mode);
		if (res == -1)
			return -errno;
	}
	
	return 0;
}

static int at_unlink(const char *path){
	File f(path, db);
	int res;

	res = unlink(f.old_path.c_str());
	if (res == -1)
		return -errno;

	return 0;
}

static int at_rmdir(const char *path){
	int res;
	for(Tier *tptr : tiers){
		res = rmdir((tptr->dir / fs::path(path)).c_str());
		if (res == -1)
			return -errno;
	}

	return 0;
}

static int at_symlink(const char *from, const char *to){
	
}

static int at_rename(const char *from, const char *to, unsigned int flags){
	int res;

	if (flags)
		return -EINVAL;
	
	File f(from, db);
	fs::path from_abs(f.old_path);
	fs::path to_rel = (fs::path(to).is_absolute())? fs::relative(fs::path(to), fs::path("/")) : fs::path(to);
	fs::path to_abs = f.current_tier / to_rel;
	
	f.rel_path = to_rel;
	f.ID = std::hash<std::string>{}(to_rel.string());
	
	res = rename(from_abs.c_str(), to_abs.c_str());
	if (res == -1)
		return -errno;

	return 0;
}

static int at_link(const char *from, const char *to){
	int res;
	
	File f(from, db);
	fs::path from_abs(f.old_path);
	fs::path to_rel = (fs::path(to).is_absolute())? fs::relative(fs::path(to), fs::path("/")) : fs::path(to);
	fs::path to_abs = f.current_tier / to_rel;
	
	f.rel_path = to_rel;
	f.ID = std::hash<std::string>{}(to_rel.string());
	
	res = link(from_abs.c_str(), to_abs.c_str());
		return -errno;

	return 0;
}

static int at_chmod(const char *path, mode_t mode, struct fuse_file_info *fi){
	(void) fi;
	fs::path p(path);
	if(is_file(p)){
		File f(p, db);
		int res;

		res = chmod(f.old_path.c_str(), mode);
		if (res == -1)
			return -errno;
	}else{
		int res;
		
		res = chmod((tiers.front()->dir / p).c_str(), mode);
		if (res == -1)
			return -errno;
	}
	
	return 0;
}

static int at_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi){
	
}

static int at_truncate(const char *path, off_t size, struct fuse_file_info *fi){
	int res;

	if (fi == NULL){
		File f(path, db);
		res = truncate(f.old_path.c_str(), size);
	}else
		res = ftruncate(fi->fh, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int at_open(const char *path, struct fuse_file_info *fi){
	fs::path p(path);
	if(is_file(p)){
		File f(path, db);
		int res;
		
		res = open(f.old_path.c_str(), fi->flags);
		if (res == -1)
			return -errno;
		
		fi->fh = res;
	}else{
		int res;
		
		res = open((tiers.front()->dir / p).c_str(), fi->flags);
		if (res == -1)
			return -errno;
		
		fi->fh = res;
	}
	return 0;
}

static int at_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	int fd;
	int res;

	if(fi == NULL){
		File f(path, db);
		fd = open(f.old_path.c_str(), O_RDONLY);
	}else
		fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi == NULL)
		close(fd);
	return res;
}

static int at_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	int fd;
	int res;

	(void) fi;
	if(fi == NULL){
		File f(path, db);
		fd = open(f.old_path.c_str(), O_WRONLY);
	}else
		fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi == NULL)
		close(fd);
	return res;
}

static int at_statfs(const char *path, struct statvfs *stbuf){
	int res;
	struct statvfs fs_stats_total, fs_stats_temp;
	memset(&fs_stats_total, 0, sizeof(struct statvfs));
	
	for(Tier *tptr : tiers){
		res = statvfs((tptr->dir / fs::path(path)).c_str(), &fs_stats_temp);
		if (res == -1)
			return -errno;
		if(fs_stats_total.f_bsize == 0) fs_stats_total.f_bsize = fs_stats_temp.f_bsize;
		if(fs_stats_total.f_frsize == 0) fs_stats_total.f_frsize = fs_stats_temp.f_frsize;
		fs_stats_total.f_blocks += fs_stats_temp.f_blocks;
		fs_stats_total.f_bfree += fs_stats_temp.f_bfree;
		fs_stats_total.f_bavail += fs_stats_temp.f_bavail;
		fs_stats_total.f_files += fs_stats_temp.f_files;
		if(fs_stats_total.f_ffree == 0) fs_stats_total.f_ffree = fs_stats_temp.f_ffree;
		if(fs_stats_total.f_favail == 0) fs_stats_total.f_favail = fs_stats_temp.f_favail;
	}

	return 0;
}

static int at_release(const char *path, struct fuse_file_info *fi){
	(void) path;
	close(fi->fh);
	return 0;
}

static int at_fsync(const char *path, int isdatasync, struct fuse_file_info *fi){
	/* Just a stub.	 This method is optional and can safely be left
		 unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_SETXATTR
static int at_setxattr(const char *path, const char *name, const char *value, size_t size, int flags){
	
}

static int at_getxattr(const char *path, const char *name, char *value, size_t size){
	
}

static int at_listxattr(const char *path, char *list, size_t size){
	
}

static int at_removexattr(const char *path, const char *name){
	
}

#endif
static int at_readdir(
	const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi,
	enum fuse_readdir_flags flags
){
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;
	(void) flags;
	for(Tier *tptr : tiers){
		dp = opendir((tptr->dir / fs::path(path)).c_str());
		if (dp == NULL)
			return -errno;

		while ((de = readdir(dp)) != NULL) {
			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = de->d_ino;
			st.st_mode = de->d_type << 12;
			if (filler(buf, de->d_name, &st, 0, (fuse_fill_dir_flags)0))
				break;
		}

		closedir(dp);
	}
	return 0;
}

void *at_init(struct fuse_conn_info *conn, struct fuse_config *cfg){
	(void) conn;
	cfg->use_ino = 1;

	/* Pick up changes from lower filesystem right away. This is
		 also necessary for better hardlink support. When the kernel
		 calls the unlink() handler, it does not know the inode of
		 the to-be-removed entry and can therefore not invalidate
		 the cache of the associated inode - resulting in an
		 incorrect st_nlink value being reported for any remaining
		 hardlinks to this inode. */
	cfg->entry_timeout = 0;
	cfg->attr_timeout = 0;
	cfg->negative_timeout = 0;

	return NULL;
}

static int at_access(const char *path, int mask){
	fs::path p(path);
	if(is_file(p)){
		File f(p, db);
		int res;
		
		res = access(f.old_path.c_str(), mask);
		if (res == -1)
			return -errno;
	}else{
		int res;
		
		res = access((tiers.front()->dir / p).c_str(), mask);
		if (res == -1)
			return -errno;
	}
	
	return 0;
}

static int at_create(const char *path, mode_t mode, struct fuse_file_info *fi){
	int res;
	fs::path fullpath(tiers.front()->dir / path);
	res = open(fullpath.c_str(), fi->flags, mode);
	if (res == -1)
		return -errno;
	File(path, tiers.front(), db);

	fi->fh = res;
	return 0;
}

#ifdef HAVE_UTIMENSAT
static int at_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi){
	(void) fi;
	File f(path, db);
	int res;
	
	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, f.old_path.c_str(), ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

	return 0;
}

#endif
#ifdef HAVE_POSIX_FALLOCATE
static int at_fallocate(const char *path, int mode, off_t offset, off_t length, struct fuse_file_info *fi){
	
}

#endif
#ifdef HAVE_COPY_FILE_RANGE
static ssize_t at_copy_file_range(
	const char *path_in, struct fuse_file_info *fi_in, off_t offset_in, const char *path_out,
	struct fuse_file_info *fi_out, off_t offset_out, size_t len, int flags
){
	
}

#endif
static off_t at_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi){
	int fd;
	off_t res;

	if (fi == NULL){
		File f(path, db);
		fd = open(f.old_path.c_str(), O_RDONLY);
	}else
		fd = fi->fh;

	if (fd == -1)
		return -errno;

	res = lseek(fd, off, whence);
	if (res == -1)
		res = -errno;

	if (fi == NULL)
		close(fd);
	return res;
}


int FusePassthrough::mount_fs(fs::path mountpoint){
	_mountpoint_ = mountpoint;
	Log("Mounting filesystem", 2);
	static const struct fuse_operations at_oper = {
		.getattr					= at_getattr,
		.readlink					= at_readlink,
		.mknod						= at_mknod,
		.mkdir						= at_mkdir,
		.unlink						= at_unlink,
		.rmdir						= at_rmdir,
		.symlink					= at_symlink,
		.rename						= at_rename,
		.link							= at_link,
		.chmod						= at_chmod,
		.chown						= at_chown,
		.truncate					= at_truncate,
		.open							= at_open,
		.read							= at_read,
		.write						= at_write,
		.statfs						= at_statfs,
		.release					= at_release,
		.fsync						= at_fsync,
#ifdef HAVE_SETXATTR
		.setxattr					= at_setxattr,
		.getxattr					= at_getxattr,
		.listxattr				= at_listxattr,
		.removexattr			= at_removexattr,
#endif
		.readdir					= at_readdir,
		.init			 				= at_init,
		.access						= at_access,
		.create 					= at_create,
#ifdef HAVE_UTIMENSAT
		.utimens					= at_utimens,
#endif
#ifdef HAVE_POSIX_FALLOCATE
		.fallocate				= at_fallocate,
#endif
#ifdef HAVE_COPY_FILE_RANGE
		.copy_file_range 	= at_copy_file_range,
#endif
		.lseek						= at_lseek,
	};
	char mountpoint_[4096];
	strncpy(mountpoint_, mountpoint.c_str(), strlen(mountpoint.c_str())+1);
	//std::cerr << std::string("\"") + std::string(mountpoint_) + std::string("\"") << std::endl;
	char *argv[2] = {"autotier", mountpoint_};
	return fuse_main(2, argv, &at_oper, NULL);
}



















#ifdef lksjdlkfjds

	#define FUSE_USE_VERSION 30

	#ifdef HAVE_CONFIG_H
	#include <config.h>
	#endif

	#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
	#endif

	#ifdef linux
	/* For pread()/pwrite()/utimensat() */
	#define _XOPEN_SOURCE 700
	#endif

	#include <fuse.h>
	#include <stdlib.h>
	#include <stdio.h>
	#include <string.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <dirent.h>
	#include <errno.h>
	#ifdef __FreeBSD__
	#include <sys/socket.h>
	#include <sys/un.h>
	#endif
	#include <sys/time.h>
	#ifdef HAVE_SETXATTR
	#include <sys/xattr.h>
	#endif

	#include "passthrough_helpers.h"

	static void *at_init(struct fuse_conn_info *conn,
					struct fuse_config *cfg)
	{
		(void) conn;
		cfg->use_ino = 1;

		/* Pick up changes from lower filesystem right away. This is
			 also necessary for better hardlink support. When the kernel
			 calls the unlink() handler, it does not know the inode of
			 the to-be-removed entry and can therefore not invalidate
			 the cache of the associated inode - resulting in an
			 incorrect st_nlink value being reported for any remaining
			 hardlinks to this inode. */
		cfg->entry_timeout = 0;
		cfg->attr_timeout = 0;
		cfg->negative_timeout = 0;

		return NULL;
	}

	static int at_getattr(const char *path, struct stat *stbuf,
					 struct fuse_file_info *fi)
	{
		(void) fi;
		int res;

		res = lstat((path), stbuf);
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_access(const char *path, int mask)
	{
		int res;

		res = access((path), mask);
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_readlink(const char *path, char *buf, size_t size)
	{
		int res;

		res = readlink((path), buf, size - 1);
		if (res == -1)
			return -errno;

		buf[res] = '\0';
		return 0;
	}


	static int at_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
					 off_t offset, struct fuse_file_info *fi,
					 enum fuse_readdir_flags flags)
	{
		DIR *dp;
		struct dirent *de;

		(void) offset;
		(void) fi;
		(void) flags;

		dp = opendir((path));
		if (dp == NULL)
			return -errno;

		while ((de = readdir(dp)) != NULL) {
			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = de->d_ino;
			st.st_mode = de->d_type << 12;
			if (filler(buf, de->d_name, &st, 0, (fuse_fill_dir_flags)0))
				break;
		}

		closedir(dp);
		return 0;
	}

	static int at_mknod(const char *path, mode_t mode, dev_t rdev)
	{
		int res;

		res = mknod_wrapper(AT_FDCWD, (path), NULL, mode, rdev);
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_mkdir(const char *path, mode_t mode)
	{
		int res;

		res = mkdir((path), mode);
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_unlink(const char *path)
	{
		int res;

		res = unlink((path));
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_rmdir(const char *path)
	{
		int res;

		res = rmdir((path));
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_symlink(const char *from, const char *to)
	{
		int res;

		res = symlink((from), (to));
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_rename(const char *from, const char *to, unsigned int flags)
	{
		int res;

		if (flags)
			return -EINVAL;

		res = rename((from), (to));
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_link(const char *from, const char *to)
	{
		int res;

		res = link((from), (to));
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_chmod(const char *path, mode_t mode,
				 struct fuse_file_info *fi)
	{
		(void) fi;
		int res;

		res = chmod((path), mode);
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_chown(const char *path, uid_t uid, gid_t gid,
				 struct fuse_file_info *fi)
	{
		(void) fi;
		int res;

		res = lchown((path), uid, gid);
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_truncate(const char *path, off_t size,
				struct fuse_file_info *fi)
	{
		int res;

		if (fi != NULL)
			res = ftruncate(fi->fh, size);
		else
			res = truncate((path), size);
		if (res == -1)
			return -errno;

		return 0;
	}

	#ifdef HAVE_UTIMENSAT
	static int at_utimens(const char *path, const struct timespec ts[2],
					 struct fuse_file_info *fi)
	{
		(void) fi;
		int res;

		/* don't use utime/utimes since they follow symlinks */
		res = utimensat(0, (path), ts, AT_SYMLINK_NOFOLLOW);
		if (res == -1)
			return -errno;

		return 0;
	}
	#endif

	static int at_create(const char *path, mode_t mode,
					struct fuse_file_info *fi)
	{
		int res;

		res = open((path), fi->flags, mode);
		if (res == -1)
			return -errno;

		fi->fh = res;
		return 0;
	}

	static int at_open(const char *path, struct fuse_file_info *fi)
	{
		int res;

		res = open((path), fi->flags);
		if (res == -1)
			return -errno;

		fi->fh = res;
		return 0;
	}

	static int at_read(const char *path, char *buf, size_t size, off_t offset,
				struct fuse_file_info *fi)
	{
		int fd;
		int res;

		if(fi == NULL)
			fd = open((path), O_RDONLY);
		else
			fd = fi->fh;
		
		if (fd == -1)
			return -errno;

		res = pread(fd, buf, size, offset);
		if (res == -1)
			res = -errno;

		if(fi == NULL)
			close(fd);
		return res;
	}

	static int at_write(const char *path, const char *buf, size_t size,
				 off_t offset, struct fuse_file_info *fi)
	{
		int fd;
		int res;

		(void) fi;
		if(fi == NULL)
			fd = open((path), O_WRONLY);
		else
			fd = fi->fh;
		
		if (fd == -1)
			return -errno;

		res = pwrite(fd, buf, size, offset);
		if (res == -1)
			res = -errno;

		if(fi == NULL)
			close(fd);
		return res;
	}

	static int at_statfs(const char *path, struct statvfs *stbuf)
	{
		int res;

		res = statvfs((path), stbuf);
		if (res == -1)
			return -errno;

		return 0;
	}

	static int at_release(const char *path, struct fuse_file_info *fi)
	{
		(void) path;
		close(fi->fh);
		return 0;
	}

	static int at_fsync(const char *path, int isdatasync,
				 struct fuse_file_info *fi)
	{
		/* Just a stub.	 This method is optional and can safely be left
			 unimplemented */

		(void) path;
		(void) isdatasync;
		(void) fi;
		return 0;
	}

	#ifdef HAVE_POSIX_FALLOCATE
	static int at_fallocate(const char *path, int mode,
				off_t offset, off_t length, struct fuse_file_info *fi)
	{
		int fd;
		int res;

		(void) fi;

		if (mode)
			return -EOPNOTSUPP;

		if(fi == NULL)
			fd = open((path), O_WRONLY);
		else
			fd = fi->fh;
		
		if (fd == -1)
			return -errno;

		res = -posix_fallocate(fd, offset, length);

		if(fi == NULL)
			close(fd);
		return res;
	}
	#endif

	#ifdef HAVE_SETXATTR
	/* xattr operations are optional and can safely be left unimplemented */
	static int at_setxattr(const char *path, const char *name, const char *value,
				size_t size, int flags)
	{
		int res = lsetxattr((path), name, value, size, flags);
		if (res == -1)
			return -errno;
		return 0;
	}

	static int at_getxattr(const char *path, const char *name, char *value,
				size_t size)
	{
		int res = lgetxattr((path), name, value, size);
		if (res == -1)
			return -errno;
		return res;
	}

	static int at_listxattr(const char *path, char *list, size_t size)
	{
		int res = llistxattr((path), list, size);
		if (res == -1)
			return -errno;
		return res;
	}

	static int at_removexattr(const char *path, const char *name)
	{
		int res = lremovexattr((path), name);
		if (res == -1)
			return -errno;
		return 0;
	}
	#endif /* HAVE_SETXATTR */

	#ifdef HAVE_COPY_FILE_RANGE
	static ssize_t at_copy_file_range(const char *path_in,
						 struct fuse_file_info *fi_in,
						 off_t offset_in, const char *path_out,
						 struct fuse_file_info *fi_out,
						 off_t offset_out, size_t len, int flags)
	{
		int fd_in, fd_out;
		ssize_t res;

		if(fi_in == NULL)
			fd_in = open((path_in), O_RDONLY);
		else
			fd_in = fi_in->fh;

		if (fd_in == -1)
			return -errno;

		if(fi_out == NULL)
			fd_out = open((path_out), O_WRONLY);
		else
			fd_out = fi_out->fh;

		if (fd_out == -1) {
			close(fd_in);
			return -errno;
		}

		res = copy_file_range(fd_in, &offset_in, fd_out, &offset_out, len,
						flags);
		if (res == -1)
			res = -errno;

		close(fd_in);
		close(fd_out);

		return res;
	}
	#endif

	static off_t at_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi)
	{
		int fd;
		off_t res;

		if (fi == NULL)
			fd = open((path), O_RDONLY);
		else
			fd = fi->fh;

		if (fd == -1)
			return -errno;

		res = lseek(fd, off, whence);
		if (res == -1)
			res = -errno;

		if (fi == NULL)
			close(fd);
		return res;
	}

	static const struct fuse_operations at_oper = {
		.getattr	= at_getattr,
		.readlink	= at_readlink,
		.mknod		= at_mknod,
		.mkdir		= at_mkdir,
		.unlink		= at_unlink,
		.rmdir		= at_rmdir,
		.symlink	= at_symlink,
		.rename		= at_rename,
		.link		= at_link,
		.chmod		= at_chmod,
		.chown		= at_chown,
		.truncate	= at_truncate,
		.open		= at_open,
		.read		= at_read,
		.write		= at_write,
		.statfs		= at_statfs,
		.release	= at_release,
		.fsync		= at_fsync,
	#ifdef HAVE_SETXATTR
		.setxattr	= at_setxattr,
		.getxattr	= at_getxattr,
		.listxattr	= at_listxattr,
		.removexattr	= at_removexattr,
	#endif
		.readdir	= at_readdir,
		.init			 = at_init,
		.access		= at_access,
		.create 	= at_create,
	#ifdef HAVE_UTIMENSAT
		.utimens	= at_utimens,
	#endif
	#ifdef HAVE_POSIX_FALLOCATE
		.fallocate	= at_fallocate,
	#endif
	#ifdef HAVE_COPY_FILE_RANGE
		.copy_file_range = at_copy_file_range,
	#endif
		.lseek		= at_lseek,
	};

	int mount_autotier(int argc, char **argv) {
		return fuse_main(argc, argv, &at_oper, NULL);
	}
	
}
#endif
