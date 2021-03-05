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

#ifdef LOG_METHODS
#include "alert.hpp"
#include <sstream>
#endif

namespace fuse_ops{
	int statfs(const char *path, struct statvfs *stbuf){
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
			res = ::statvfs((tptr->path() / path).c_str(), &fs_stats_temp);
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
}
