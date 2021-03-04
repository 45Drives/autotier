/*
 *    Copyright (C) 2019-2021 Joshua Boudreau <jboudreau@45drives.com>
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

/* fusePassthrough.cpp contains all the fuse filesystem calls needed to
 * implement the filesystem. It creates a TierEngine object pointer and runs
 * begin() and process_adhoc_requests() as threads in the at_init() method, and
 * joins the threads in the at_destroy() method.
 */

#pragma once

#include <list>
#include <rocksdb/db.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

struct ConfigOverrides;

class FusePassthrough{
private:
public:
	FusePassthrough(const fs::path &config_path, const ConfigOverrides &config_overrides);
	/* calls open_db and saves pointers to each tier of tiers_
	 */
	~FusePassthrough(void) = default;
	int mount_fs(fs::path mountpoint, char *fuse_opts);
	/* creates struct of FUSE function pointers and calls fuse_main()
	 */
};

