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
#include "fusePassthrough.hpp"
#include "tierEngine.hpp"

extern "C" {
	#include <pthread.h>
}

namespace fuse_ops{
	void *init(struct fuse_conn_info *conn, struct fuse_config *cfg){
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
}
