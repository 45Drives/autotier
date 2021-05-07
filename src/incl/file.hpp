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

#include "metadata.hpp"
#include <rocksdb/db.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class Tier;

class File{
	/* File object to represent a file in the autotier filesystem.
	 */
private:
	uintmax_t size_ = 0;
	/* Size of file in bytes.
	 */
	Tier *tier_ptr_;
	/* Pointer to Tier object representing the tier containing this file.
	 */
	struct timeval times_[2];
	/* atime and mtime of the file. Used to overwrite changes from copying file.
	 */
	time_t atime_;
	/* Just the atime of the file.
	 */
	time_t ctime_;
	/* Just the ctime of the file.
	 */
	fs::path relative_path_;
	/* Location of file relative to the tier and the filesystem mountpoint.
	 */
	rocksdb::DB *db_;
	/* Database storing metadata.
	 */
	Metadata metadata_;
	/* Metadata of object retrieved from database.
	 */
public:
	File(void);
	/* Empty constructor.
	 */
	File(fs::path full_path, rocksdb::DB *db, Tier *tptr);
	/* File constructor used while tiering.
	 * Grabs metadata from db and other info from stat().
	 */
	File(const File &other);
	/* Copy constructor.
	 */
	~File();
	/* Destructor - calls metadata.update(relative_path_.c_str(), db_)
	 * to store newly updated metadata in database after tiering.
	 */
	void calc_popularity(double period_seconds);
	/* Calculate new popularity value of file.
	 * y[n] = MULTIPLIER * x / DAMPING + (1.0 - 1.0 / DAMPING) * y[n-1]
	 * where x is file usage frequency
	 */
	fs::path full_path(void) const;
	/* Return full backend path to file via tier.
	 */
	fs::path relative_path(void) const;
	/* Return path to file relative to the tier path and/or the filesystem mountpoint.
	 */
	Tier *tier_ptr(void) const;
	/* Get pointer to current tier.
	 */
	double popularity(void) const;
	/* Get popularity in accesses per hour.
	 */
	struct timeval atime(void) const;
	/* Get atime.
	 */
	uintmax_t size(void) const;
	/* Get size of file in bytes.
	 */
	void pin(void);
	/* set metadata_.pinned_.
	 */
	bool is_pinned(void) const;
	/* Get metadata_.pinned_.
	 */
	void transfer_to_tier(Tier *tptr);
	/* Update tier_ptr_ and metadata_.tier_path_ then call metadata_.update().
	 */
	void overwrite_times(void) const;
	/* Call utimes() on file path with the saved atime and mtime so
	 * they stay the same as before tiering.
	 */
	void change_path(const fs::path &new_path);
	/* Update database with new path.
	 */
};
