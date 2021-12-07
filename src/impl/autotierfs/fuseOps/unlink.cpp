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

#include "TierEngine/TierEngine.hpp"
#include "fuseOps.hpp"
#include "metadata.hpp"
#include "rocksDbHelpers.hpp"
#include "tier.hpp"

#ifdef LOG_METHODS
#	include "alert.hpp"

#	include <sstream>
#endif

namespace fuse_ops {
	int unlink(const char *path) {
		int res;

		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if (!priv)
			return -ECHILD;

#ifdef LOG_METHODS
		{
			std::stringstream ss;
			ss << "unlink " << path;
			Logging::log.message(ss.str(), Logger::log_level_t::NONE);
		}
#endif

		Metadata f(path, priv->db_);
		if (f.not_found())
			return -ENOENT;

		fs::path tier_path = f.tier_path();
		fs::path full_path = tier_path / path;

		Tier *tptr = priv->autotier_->tier_lookup(fs::path(tier_path));

		intmax_t size = l::file_size(full_path);
		if (size == -1) {
			return -errno;
		}
		tptr->subtract_file_size(size);

		res = ::unlink(full_path.c_str());

		if (res == -1)
			return -errno;

		{
			std::lock_guard<std::mutex> lk(l::rocksdb::global_lock_);
			if (!priv->db_->Delete(rocksdb::WriteOptions(), path + 1).ok())
				return -1;
		}

		return res;
	}
} // namespace fuse_ops
