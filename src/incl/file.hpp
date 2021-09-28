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

#include <45d/Bytes.hpp>
#include <boost/filesystem.hpp>
#include <rocksdb/db.h>
namespace fs = boost::filesystem;

class Tier;

/**
 * @brief File object to represent a file in the autotier filesystem.
 *
 */
class File {
public:
	/**
	 * @brief Construct a new empty File object
	 *
	 */
	File(void);
	/**
	 * @brief Construct a new File object
	 *
	 * @param full_path Full backend path to file
	 * @param db Rocksdb database pointer
	 * @param tptr Pointer to tier in which file was found
	 */
	File(fs::path full_path, rocksdb::DB *db, Tier *tptr);
	/**
	 * @brief Copy construct a new File object
	 *
	 * @param other
	 */
	File(const File &other);
	/**
	 * @brief Destroy the File object
	 * Calls metadata.update(relative_path_.c_str(), db_)
	 * to store newly updated metadata in database after tiering.
	 *
	 */
	~File();
	/**
	 * @brief Call Metadata::update()
	 *
	 */
	void update_db(void);
	/**
	 * @brief Calculate new popularity value of file.
	 * y[n] = MULTIPLIER * x / DAMPING + (1.0 - 1.0 / DAMPING) * y[n-1]
	 * where x is file usage frequency
	 *
	 * @param period_seconds Period over which to calculate
	 */
	void calc_popularity(double period_seconds);
	/**
	 * @brief Return full backend path to file via tier.
	 *
	 * @return fs::path Full backend path
	 */
	fs::path full_path(void) const;
	/**
	 * @brief Return path to file relative to the tier path and the filesystem mountpoint.
	 *
	 * @return fs::path Relative path
	 */
	fs::path relative_path(void) const;
	/**
	 * @brief Get the pointer to the tier currently holding this file
	 *
	 * @return Tier* Tier holding this file
	 */
	Tier *tier_ptr(void) const;
	/**
	 * @brief Get popularity in accesses per hour.
	 *
	 * @return double EMA of accesses per hour.
	 */
	double popularity(void) const;
	/**
	 * @brief Get last access time of file
	 *
	 * @return struct timeval
	 */
	struct timeval atime(void) const;
	/**
	 * @brief Get size of file on disk in bytes
	 *
	 * @return ffd::Bytes Size of file
	 */
	ffd::Bytes size(void) const;
	/**
	 * @brief Set metadata_.pinned_.
	 * Keeps file in current tier.
	 */
	void pin(void);
	/**
	 * @brief Get metadata_.pinned_.
	 * Check if file is pinned.
	 *
	 * @return true File is pinned
	 * @return false File is not pinned
	 */
	bool is_pinned(void) const;
	/**
	 * @brief Update tier_ptr_ and metadata_.tier_path_ then call metadata_.update().
	 *
	 * Subtracts file size from old tier, updates tptr_ to new tier, adds file size to new
	 * tier, updates metadata.tier_path, then calls metadata.update() to update the db.
	 *
	 * @param tptr New tier.
	 */
	void transfer_to_tier(Tier *tptr);
	/**
	 * @brief Call utimes() on file path with the saved atime and mtime so
	 * they stay the same as before tiering.
	 *
	 */
	void overwrite_times(void) const;
	/**
	 * @brief Update database with new path.
	 *
	 * @param new_path
	 */
	void change_path(const fs::path &new_path);
private:
	ffd::Bytes size_; ///< Size of file on disk
	Tier *tier_ptr_;  ///< Pointer to Tier object representing the tier containing this file.
	struct timeval
		times_[2]; ///< atime and mtime of the file. Used to overwrite changes from copying file.
	time_t atime_; ///< Just the atime of the file.
	time_t ctime_; ///< Just the ctime of the file.
	fs::path
		relative_path_; ///< Location of file relative to the tier and the filesystem mountpoint.
	rocksdb::DB *db_;   ///< Database storing metadata.
	Metadata metadata_; ///< Metadata of object retrieved from database.
};
