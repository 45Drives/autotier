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
#include "openFiles.hpp"
#include "tier.hpp"

extern "C" {
#include <sys/fsuid.h>
}

#ifdef LOG_METHODS
#	include "alert.hpp"

#	include <bitset>
#	include <sstream>
#endif

namespace fuse_ops {
	int create(const char *path, mode_t mode, struct fuse_file_info *fi) {
#ifdef LOG_METHODS
		{
			std::stringstream ss;
			ss << "create " << path << " mode: " << std::oct << mode
			   << " flags: " << std::bitset<8>(fi->flags);
			Logging::log.message(ss.str(), Logger::log_level_t::NONE);
		}
#endif
		int res;
		char *fullpath = nullptr;

		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if (!priv)
			return -ECHILD;

		Tier *top_tier = priv->tiers_.front();

		if (::setfsuid(ctx->uid) == -1)
			goto error_out;
		if (::setfsgid(ctx->gid) == -1)
			goto error_out;

		fullpath = strdup((top_tier->path() / path).c_str());
		if (fullpath == nullptr)
			goto error_out;

		OpenFiles::register_open_file(fullpath); // must register before open to avoid tiering race

		res = ::creat(fullpath, mode);
		if (res == -1)
			goto registered_error_out;

		fi->fh = res;

		Metadata(path, priv->db_, top_tier).update(path, priv->db_);

		priv->insert_fd_to_path(fi->fh, fullpath);
		priv->insert_size_at_open(fi->fh, 0);

		if (::setfsuid(getuid()) == -1)
			goto registered_error_out;
		if (::setfsgid(getgid()) == -1)
			goto registered_error_out;

		free(fullpath);

		return 0;
	registered_error_out:
		OpenFiles::release_open_file(fullpath);
	error_out:
		res = -errno;
		::setfsuid(getuid());
		::setfsgid(getgid());
		free(fullpath);
		return res;
	}
} // namespace fuse_ops
