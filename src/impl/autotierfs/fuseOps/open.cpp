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
	int open(const char *path, struct fuse_file_info *fi) {
		int res;
		char *fullpath = nullptr;

		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if (!priv)
			return -ECHILD;

#ifdef LOG_METHODS
		std::stringstream ss;
		ss << "open " << path << " flags: " << std::bitset<8>(fi->flags);
		Logging::log.message(ss.str(), Logger::log_level_t::NONE);
#endif

		int is_directory = l::is_directory(path);
		if (is_directory == -1)
			return -errno;
		if (::setfsuid(ctx->uid) == -1)
			goto error_out;
		if (::setfsgid(ctx->gid) == -1)
			goto error_out;
		if (is_directory) {
			fullpath = strdup((priv->tiers_.front()->path() / path).c_str());
			res = ::open(fullpath, fi->flags, 0777);
			if (res == -1)
				goto error_out;
		} else { // is file
			Metadata f(path, priv->db_);
			if (f.not_found()) {
				errno = ENOENT;
				goto error_out;
			}
			fs::path tier_path = f.tier_path();
			fullpath = strdup((tier_path / path).c_str());
			if (fullpath == nullptr)
				goto error_out;
			// get size before open() in case called with truncate
			intmax_t file_size = l::file_size(fs::path(fullpath));
			if (file_size == -1) {
				if (errno == ENOENT && fi->flags & O_CREAT)
					file_size = 0;
				else
					goto error_out;
			}
			OpenFiles::register_open_file(fullpath);
			res = ::open(fullpath, fi->flags, 0777);
			if (res == -1)
				goto registered_error_out;
			if (::setfsuid(getuid()) == -1)
				goto registered_error_out;
			if (::setfsgid(getgid()) == -1)
				goto registered_error_out;
			priv->insert_size_at_open(res, file_size);
			f.touch();
			f.update(path, priv->db_);
#ifdef LOG_METHODS
			{
				std::stringstream ss;
				ss << "size at open of " << fullpath << ": " << file_size
				   << ", from LUT: " << priv->size_at_open(res);
				Logging::log.message(ss.str(), Logger::log_level_t::NONE);
			}
#endif
		}

		fi->fh = res;

		priv->insert_fd_to_path(res, fullpath);

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
		free(fullpath);
		::setfsuid(getuid());
		::setfsgid(getgid());
		return res;
	}
} // namespace fuse_ops
