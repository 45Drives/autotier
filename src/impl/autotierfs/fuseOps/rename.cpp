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
	int rename(const char *from, const char *to, unsigned int flags) {
		int res;

		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if (!priv)
			return -ECHILD;

#ifdef LOG_METHODS
		{
			std::stringstream ss;
			ss << "rename " << from << "->" << to;
			Logging::log.message(ss.str(), Logger::log_level_t::NONE);
		}
#endif

		if (flags)
			return -EINVAL;

		if (l::is_directory(from)) {
			res = -ENOENT;
			for (const Tier *tptr : priv->tiers_) {
				res = ::rename((tptr->path() / from).c_str(), (tptr->path() / to).c_str());
				if (res == -1)
					return -errno;
			}
			l::update_keys_in_directory(from + 1, to + 1, priv->db_);
		} else {
			Metadata f(from, priv->db_);
			if (f.not_found())
				return -ENOENT;
			fs::path tier_path = f.tier_path();

			res = ::rename((tier_path / from).c_str(), (tier_path / to).c_str());
			if (res == -1)
				return -errno;

			std::string key_to_delete(from);
			f.update(to, priv->db_, &key_to_delete);
		}

		return res;
	}
} // namespace fuse_ops
