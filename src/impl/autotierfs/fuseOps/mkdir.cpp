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
#	include "alert.hpp"

#	include <sstream>
#endif

namespace fuse_ops {
	int mkdir(const char *path, mode_t mode) {
		int res;

		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if (!priv)
			return -ECHILD;

#ifdef LOG_METHODS
		{
			std::stringstream ss;
			ss << "mkdir " << path;
			Logging::log.message(ss.str(), Logger::log_level_t::NONE);
		}
#endif

		for (Tier *tptr : priv->tiers_) {
			res = ::mkdir((tptr->path() / fs::path(path)).c_str(), mode);
			if (res == -1)
				return -errno;
			res = ::lchown((tptr->path() / fs::path(path)).c_str(), ctx->uid, ctx->gid);
			if (res == -1)
				return -errno;
		}

		return res;
	}
} // namespace fuse_ops
