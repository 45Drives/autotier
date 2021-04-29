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

#include "fuseOps.hpp"
#include "tier.hpp"
#include <regex>

#ifdef LOG_METHODS
#include "alert.hpp"
#endif

extern "C" {
	#include <dirent.h>
	#include <sys/stat.h>
}

namespace fuse_ops{
	class dirp{
	public:
		struct dirent *entry;
		off_t offset;
		std::vector<DIR *> dps;
		std::vector<char *> backends;
		dirp(){
			fuse_context *ctx = fuse_get_context();
			FusePriv *priv = (FusePriv *)ctx->private_data;
			dps = std::vector<DIR *>();
			backends = std::vector<char *>();
		}
		~dirp(){
			for(DIR *dp : dps)
				closedir(dp);
			for(char *str : backends)
				free(str);
		}
	};

	int opendir(const char *path, struct fuse_file_info *fi){
		int res;
		
		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if(!priv)
			return -ECHILD;
		
		class dirp *d = new dirp();
		if(d == NULL)
			return -ENOMEM;
		
		for(Tier *t : priv->tiers_){
			fs::path backend_path = t->path() / path;
			DIR *dir = ::opendir(backend_path.c_str());
			if(dir != NULL){
				d->dps.push_back(dir);
				d->backends.push_back(strdup(backend_path.c_str()));
			} // else ignore
		}
		
		d->offset = 0;
		d->entry = NULL;

		fi->fh = (unsigned long) d;
		return 0;
	}

	static inline class dirp *get_dirp(struct fuse_file_info *fi){
		return (class dirp *) (uintptr_t) fi->fh;
	}

	int readdir(
		const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi,
		enum fuse_readdir_flags flags
	){
		class dirp *d = get_dirp(fi);
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
					d->entry = ::readdir(*cur_dir);
					if(!d->entry)
						break;
					skip_dup_dir = (d->entry->d_type == DT_DIR && cur_dir != d->dps.begin());
				}
		#ifdef HAVE_FSTATAT
				if(flags & FUSE_READDIR_PLUS) {
					int res;
					
					res = ::fstatat(dirfd(*cur_dir), d->entry->d_name, &st,
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
				if(!skip_dup_dir && !std::regex_match(d->entry->d_name, temp_file_re)){
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

	int releasedir(const char *path, struct fuse_file_info *fi){
		class dirp *d = get_dirp(fi);
		(void) path;
		delete d;
		return 0;
	}
	
#ifdef USE_FSYNCDIR
	int fsyncdir(const char *path, int isdatasync, struct fuse_file_info *fi){
		int res;
		class dirp *d = get_dirp(fi);
		
		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if(!priv)
			return -ECHILD;
		
		(void) path;
#ifdef LOG_METHODS
		Logging::log.message("fsyncdir fh", 0);
#endif
		for(int i = 0; i < priv->tiers_.size(); i++){
			int fd = ::open(d->backends[i], O_DIRECTORY);
			if(isdatasync)
				res = ::fdatasync(fd);
			else
				res = ::fsync(fd);
			
			if(res == -1)
				return -errno;
		}
		
		return 0;
	}
#endif
}
