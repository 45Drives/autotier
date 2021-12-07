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

namespace fuse_ops {
	ssize_t copy_file_range(const char *path_in,
							struct fuse_file_info *fi_in,
							off_t offset_in,
							const char *path_out,
							struct fuse_file_info *fi_out,
							off_t offset_out,
							size_t len,
							int flags) {
		ssize_t res;
		(void)path_in;
		(void)path_out;

		res = ::copy_file_range(fi_in->fh, &offset_in, fi_out->fh, &offset_out, len, flags);
		if (res == -1)
			return -errno;

		return res;
	}
} // namespace fuse_ops
