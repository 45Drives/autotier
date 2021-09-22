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
	int access(const char *path, int mask) {
		int res;

		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if (!priv)
			return -ECHILD;

#ifdef LOG_METHODS
		{
			std::stringstream ss;
			ss << "access " << path;
			Logging::log.message(ss.str(), Logger::log_level_t::NONE);
		}
#endif

		int is_directory = l::is_directory(path);
		if (is_directory == -1)
			return -errno;
		if (is_directory) {
			for (Tier *tptr : priv->tiers_) {
				res = ::access((tptr->path() / path).c_str(), mask);
				if (res != 0)
					return res;
			}
		} else {
			Metadata f(path, priv->db_);
			if (f.not_found())
				return -ENOENT;
			fs::path tier_path = f.tier_path();
			res = ::access((tier_path / path).c_str(), mask);
		}

		if (res != 0)
			return res;
		return res;
	}
} // namespace fuse_ops
