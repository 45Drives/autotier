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
#include "TierEngine/TierEngine.hpp"
#include "alert.hpp"

extern "C" {
	#include <signal.h>
}

namespace fuse_ops{
	void destroy(void *private_data){
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
}
