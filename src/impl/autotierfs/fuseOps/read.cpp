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
#	include "alert.hpp"

#	include <sstream>
#endif

static struct fuse_bufvec fuse_bufvec_init(size_t sz) {
	return FUSE_BUFVEC_INIT(sz);
}

namespace fuse_ops {
	int read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
		int res;
		(void)path;

#ifdef LOG_METHODS
		{
			std::stringstream ss;
			ss << "read";
			Logging::log.message(ss.str(), Logger::log_level_t::NONE);
		}
#endif

		res = ::pread(fi->fh, buf, size, offset);

		if (res == -1)
			return -errno;
		return res;
	}

	int read_buf(const char *path,
				 struct fuse_bufvec **bufp,
				 size_t size,
				 off_t offset,
				 struct fuse_file_info *fi) {
		struct fuse_bufvec *src;

		(void)path;

		src = (struct fuse_bufvec *)malloc(sizeof(struct fuse_bufvec));
		if (src == NULL)
			return -ENOMEM;

		*src = fuse_bufvec_init(size);

		src->buf[0].flags = (fuse_buf_flags)(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
		src->buf[0].fd = fi->fh;
		src->buf[0].pos = offset;

		*bufp = src;

		return 0;
	}
} // namespace fuse_ops
