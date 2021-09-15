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

#ifdef LOG_METHODS
#include "alert.hpp"
#endif

namespace fuse_ops{
	int flush(const char *path, struct fuse_file_info *fi){
		int res;
		(void) path;
		
#ifdef LOG_METHODS
		Logging::log.message("flush fh", Logger::log_level_t::NONE);
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
		
		res = ::close(fh_dup);
		
		if(res == -1)
			return -errno;
		return res;
	}
}
