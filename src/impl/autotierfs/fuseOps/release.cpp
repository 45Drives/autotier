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
#include "alert.hpp"
#include "tier.hpp"
#include "openFiles.hpp"
#include "TierEngine/TierEngine.hpp"

#ifdef LOG_METHODS
#include <sstream>
#endif

namespace fuse_ops{
	int release(const char *path, struct fuse_file_info *fi){
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
				ss << "size from LUT: " << priv->size_at_open(fi->fh);
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
		if(old_size != -1 && new_size != -1 && tptr){
			tptr->size_delta(old_size, new_size);
			if(!priv->autotier_->strict_period() && tptr->usage_bytes() > tptr->quota())
				priv->autotier_->tier();
		}
		res = ::close(fi->fh);
		return res;
	}
}
