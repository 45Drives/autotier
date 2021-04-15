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
#include "tierEngine.hpp"

#ifdef LOG_METHODS
#include "alert.hpp"
#include <sstream>
#endif

static struct fuse_bufvec fuse_bufvec_init(size_t sz){
	return FUSE_BUFVEC_INIT(sz);
}

namespace fuse_ops{
	int write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
		int res;
		(void) path;
		
#ifdef LOG_METHODS
		{
		std::stringstream ss;
		ss << "write";
		Logging::log.message(ss.str(), 0);
		}
#endif
		bool out_of_space = false;
		FusePriv *priv = nullptr;
		
		do{
			res = ::pwrite(fi->fh, buf, size, offset);
			out_of_space = false;
			if(res == -1){
				int error = errno;
				if(error == ENOSPC){
					out_of_space = true;
					if(!priv){
						fuse_context *ctx = fuse_get_context();
						priv = (FusePriv *)ctx->private_data;
						if(!priv)
							return -error;
					}
					if(priv->autotier_->strict_period())
						return -error;
					else{
						bool tier_res = priv->autotier_->tier(std::chrono::seconds(-1));
						while(!tier_res && priv->autotier_->currently_tiering()){
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						}
					}
				}else
					return -error;
			}
		}while(out_of_space);
		
		return res;
	}
	
	int write_buf(const char *path, struct ::fuse_bufvec *buf, off_t offset, struct ::fuse_file_info *fi){
		struct ::fuse_bufvec dst = fuse_bufvec_init(fuse_buf_size(buf));
		
		(void) path;
		
		dst.buf[0].flags = (fuse_buf_flags)(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
		dst.buf[0].fd = fi->fh;
		dst.buf[0].pos = offset;
		
		bool out_of_space = false;
		FusePriv *priv = nullptr;
		
		ssize_t bytes_copied;
		
		do{
			bytes_copied = fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
			out_of_space = false;
			if(bytes_copied == -ENOSPC){
				out_of_space = true;
				if(!priv){
					fuse_context *ctx = fuse_get_context();
					priv = (FusePriv *)ctx->private_data;
					if(!priv)
						return -ENOSPC;
				}
				if(priv->autotier_->strict_period())
					return -ENOSPC;
				else{
					bool tier_res = priv->autotier_->tier(std::chrono::seconds(-1));
					while(!tier_res && priv->autotier_->currently_tiering()){
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
					}
				}
			}
		}while(out_of_space);
		
		return bytes_copied;
	}
}
