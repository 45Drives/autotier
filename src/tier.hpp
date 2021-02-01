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

#pragma once

#include <queue>
#include <rocksdb/db.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class File;

class Tier{
	/* Class to represent each tier in the filesystem.
	 */
private:
	int watermark_ = -1;
	/* Percent fullness at which to stop
	 * filling tier and move to next one.
	 */
	uintmax_t watermark_bytes_;
	/* Number of bytes to stop filling at,
	 * calculated from watermark_ and capacity_.
	 */
	uintmax_t capacity_;
	/* Number of bytes available in the tier,
	 * calculated from the results of a statvfs() call.
	 */
	uintmax_t usage_;
	/* Real current bytes used in underlying filesystem.
	 */
	uintmax_t sim_usage_;
	/* Number of bytes used, used while simulating
	 * the tiering of files to determine where to place each file.
	 */
	std::string id_;
	/* User-defined friendly name of tier,
	 * in square bracket header of tier definition
	 * in config file.
	 */
	fs::path path_;
	/* Backend path to tier.
	 */
	std::vector<File *> incoming_files_;
	/* Queue of files to be placed into the tier, filled
	 * during the simulation of tiering and used while actually
	 * tiering.
	 */
	void get_capacity_and_usage(void);
	/* Find capacity_ from statvfs() call and set usage to 0.
	 */
	void copy_ownership_and_perms(const fs::path &old_path, const fs::path &new_path) const;
	/* Copy ownership and permissions from old_path to new_path,
	 * called after copying a file to a different tier.
	 */
public:
	Tier(std::string id);
	/* Constructor assigns user-defined ID.
	 */
	~Tier() = default;
	/* Default destructor.
	 */
	void add_file_size(uintmax_t size);
	/* Add size bytes to usage_.
	 */
	void subtract_file_size(uintmax_t size);
	/* Subtract size bytes from usage.
	 */
	void watermark(int watermark);
	/* Set watermark_.
	 */
	int watermark(void) const;
	/* Get watermark_.
	 */
	void calc_watermark_bytes(void);
	/* Determine watermark_bytes_ from watermark_ and capacity_.
	 */
	uintmax_t watermark_bytes(void) const;
	/* Get watermark_bytes_.
	 */
	bool full_test(const File &file) const;
	/* returns usage_ + file.size() > watermark_bytes_.
	 */
	void path(const fs::path &path);
	/* Set path_ and call get_capacity_and_usage().
	 */
	const fs::path &path(void) const;
	/* Get path_.
	 */
	const std::string &id(void) const;
	/* Get id_.
	 */
	void enqueue_file_ptr(File *fptr);
	/* Push file pointer into incoming_files_.
	 */
	void transfer_files(void);
	/* Iterate through incoming_files_ and move each file into
	 * the tier.
	 */
	bool move_file(const fs::path &old_path, const fs::path &new_path) const;
	/* Called in transfer_files() to actually copy the file and
	 * remove the old one.
	 */
	double usage_percent(void) const;
	/* Return real current usage as percent.
	 */
	uintmax_t usage_bytes(void) const;
	/* Return real current usage in bytes.
	 */
};
