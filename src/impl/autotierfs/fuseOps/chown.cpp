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
#include "metadata.hpp"
#include "tier.hpp"

#ifdef LOG_METHODS
#	include "alert.hpp"

#	include <sstream>
#endif

namespace fuse_ops {
	int chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
		int res;

		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if (!priv)
			return -ECHILD;

#ifdef LOG_METHODS
		{
			std::stringstream ss;
			ss << "chown";
			Logging::log.message(ss.str(), Logger::log_level_t::NONE);
		}
#endif

		if (fi) {
#ifdef LOG_METHODS
			Logging::log.message("chown fh", Logger::log_level_t::NONE);
#endif
			res = ::fchown(fi->fh, uid, gid);
		} else {
#ifdef LOG_METHODS
			Logging::log.message("chown " + std::string(path), Logger::log_level_t::NONE);
#endif
			int is_directory = l::is_directory(path);
			if (is_directory == -1)
				return -errno;
			if (is_directory) {
				for (Tier *tptr : priv->tiers_) {
					res = ::lchown((tptr->path() / path).c_str(), uid, gid);
					if (res == -1)
						return -errno;
				}
			} else {
				Metadata f(path, priv->db_);
				if (f.not_found())
					return -ENOENT;
				fs::path tier_path = f.tier_path();
				res = ::lchown((tier_path / path).c_str(), uid, gid);
			}
		}

		if (res == -1)
			return -errno;
		return res;
	}
} // namespace fuse_ops
