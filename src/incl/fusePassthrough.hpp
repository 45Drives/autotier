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

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

struct ConfigOverrides;

namespace Global{
	extern fs::path config_path_;
	extern fs::path mount_point_;
	extern ConfigOverrides config_overrides_;
}

/**
 * @brief Class to mount filesystem
 * 
 */
class FusePassthrough{
public:
	/**
	 * @brief Construct a new Fuse Passthrough object.
	 * Calls open_db and saves pointers to each tier of tiers_.
	 * 
	 * @param config_path Path to config file for constructing TierEngine/Config
	 * @param config_overrides Config overrides from cli args
	 */
	FusePassthrough(const fs::path &config_path, const ConfigOverrides &config_overrides);
	/**
	 * @brief Destroy the Fuse Passthrough object
	 * 
	 */
	~FusePassthrough(void) = default;
	/**
	 * @brief Mount the fuse filesystem.
	 * Creates struct of FUSE function pointers and calls fuse_main().
	 * 
	 * @param mountpoint Path to mountpoint
	 * @param fuse_opts Comma separated options to pass to fuse_main()
	 * @return int Return value of fuse_main()
	 */
	int mount_fs(fs::path mountpoint, char *fuse_opts);
};
