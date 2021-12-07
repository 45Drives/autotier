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

extern "C" {
#include <sys/xattr.h>
}

namespace fuse_ops {
	int setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
		int res;

		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if (!priv)
			return -ECHILD;

#ifdef LOG_METHODS
		{
			std::stringstream ss;
			ss << "setxattr";
			Logging::log.message(ss.str(), Logger::log_level_t::NONE);
		}
#endif

		fs::path fullpath;

		int is_directory = l::is_directory(path);
		if (is_directory == -1)
			return -errno;
		if (is_directory) {
			for (Tier *const &tier : priv->tiers_) {
				fullpath = tier->path() / path;
				res = ::lsetxattr(fullpath.c_str(), name, value, size, flags);
				if (res == -1)
					return -errno;
			}
		} else {
			Metadata f(path, priv->db_);
			if (f.not_found())
				return -ENOENT;
			fullpath = f.tier_path() + std::string(path);
			res = ::lsetxattr(fullpath.c_str(), name, value, size, flags);
			if (res == -1)
				return -errno;
		}

		return 0;
	}

	int getxattr(const char *path, const char *name, char *value, size_t size) {
		int res;

		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if (!priv)
			return -ECHILD;

#ifdef LOG_METHODS
		{
			std::stringstream ss;
			ss << "getxattr " << path;
			Logging::log.message(ss.str(), Logger::log_level_t::NONE);
		}
#endif

		fs::path fullpath;

		int is_directory = l::is_directory(path);
		if (is_directory == -1)
			return -errno;
		if (is_directory) {
			fullpath = priv->tiers_.front()->path() / path;
		} else {
			Metadata f(path, priv->db_);
			if (f.not_found())
				return -ENOENT;
			fullpath = f.tier_path() + std::string(path);
		}

		res = ::lgetxattr(fullpath.c_str(), name, value, size);

		if (res == -1)
			return -errno;

		return res;
	}

	int listxattr(const char *path, char *list, size_t size) {
		int res;

		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if (!priv)
			return -ECHILD;

#ifdef LOG_METHODS
		{
			std::stringstream ss;
			ss << "listxattr";
			Logging::log.message(ss.str(), Logger::log_level_t::NONE);
		}
#endif

		fs::path fullpath;

		int is_directory = l::is_directory(path);
		if (is_directory == -1)
			return -errno;
		if (is_directory) {
			fullpath = priv->tiers_.front()->path() / path;
		} else {
			Metadata f(path, priv->db_);
			if (f.not_found())
				return -ENOENT;
			fullpath = f.tier_path() + std::string(path);
		}

		res = ::llistxattr(fullpath.c_str(), list, size);
		if (res == -1)
			return -errno;

		return res;
	}

	int removexattr(const char *path, const char *name) {
		int res;

		fuse_context *ctx = fuse_get_context();
		FusePriv *priv = (FusePriv *)ctx->private_data;
		if (!priv)
			return -ECHILD;

#ifdef LOG_METHODS
		{
			std::stringstream ss;
			ss << "removexattr";
			Logging::log.message(ss.str(), Logger::log_level_t::NONE);
		}
#endif

		int is_directory = l::is_directory(path);
		if (is_directory == -1)
			return -errno;
		if (is_directory) {
			for (Tier *const &tier : priv->tiers_) {
				fs::path fullpath = tier->path() / path;
				res = ::lremovexattr(fullpath.c_str(), name);
				if (res == -1)
					return -errno;
			}
		} else {
			Metadata f(path, priv->db_);
			if (f.not_found())
				return -ENOENT;
			fs::path fullpath = f.tier_path() + std::string(path);
			res = ::lremovexattr(fullpath.c_str(), name);
			if (res == -1)
				return -errno;
		}

		return 0;
	}
} // namespace fuse_ops
