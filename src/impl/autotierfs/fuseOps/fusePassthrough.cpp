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

#include "fusePassthrough.hpp"
#include "fuseOps.hpp"
#include "config.hpp"
#include "alert.hpp"
#include "TierEngine/TierEngine.hpp"

FusePassthrough::FusePassthrough(const fs::path &config_path, const ConfigOverrides &config_overrides){
	fuse_ops::autotier_ptr = new TierEngine(config_path, config_overrides);
}

// methods
int FusePassthrough::mount_fs(fs::path mountpoint, char *fuse_opts){
	fuse_ops::autotier_ptr->mount_point(mountpoint);
	Logging::log.message("Mounting filesystem", 2);
	static const struct fuse_operations at_oper = {
		.getattr					= fuse_ops::getattr,
		.readlink					= fuse_ops::readlink,
		.mknod						= fuse_ops::mknod,
		.mkdir						= fuse_ops::mkdir,
		.unlink						= fuse_ops::unlink,
		.rmdir						= fuse_ops::rmdir,
		.symlink					= fuse_ops::symlink,
		.rename						= fuse_ops::rename,
		.link						= fuse_ops::link,
		.chmod						= fuse_ops::chmod,
		.chown						= fuse_ops::chown,
		.truncate					= fuse_ops::truncate,
		.open						= fuse_ops::open,
		.read						= fuse_ops::read,
		.write						= fuse_ops::write,
		.statfs						= fuse_ops::statfs,
		.flush						= fuse_ops::flush,
		.release					= fuse_ops::release,
		.fsync						= fuse_ops::fsync,
		.setxattr					= fuse_ops::setxattr,
		.getxattr					= fuse_ops::getxattr,
		.listxattr					= fuse_ops::listxattr,
		.removexattr				= fuse_ops::removexattr,
		.opendir					= fuse_ops::opendir,
		.readdir					= fuse_ops::readdir,
		.releasedir					= fuse_ops::releasedir,
#ifdef USE_FSYNCDIR
		.fsyncdir					= fuse_ops::fsyncdir,
#endif
		.init		 				= fuse_ops::init,
		.destroy					= fuse_ops::destroy,
		.access						= fuse_ops::access,
		.create 					= fuse_ops::create,
#ifdef HAVE_LIBULOCKMGR
		.lock						= fuse_ops::lock,
#endif
		.utimens					= fuse_ops::utimens,
		.write_buf					= fuse_ops::write_buf,
		.read_buf					= fuse_ops::read_buf,
		.flock						= fuse_ops::flock,
		.fallocate					= fuse_ops::fallocate,
#ifndef EL8
		.copy_file_range 			= fuse_ops::copy_file_range,
		.lseek						= fuse_ops::lseek,
#endif
	};
	std::vector<char *>argv = {strdup("autotier"), strdup(mountpoint.c_str())};
	if(fuse_opts){
		argv.push_back(strdup("-o"));
		argv.push_back(fuse_opts);
	}
	return fuse_main(argv.size(), argv.data(), &at_oper, NULL);
}
