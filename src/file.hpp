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

/* popularity calculation
 * y[n] = MULTIPLIER * x / DAMPING + (1.0 - 1.0 / DAMPING) * y[n-1]
 * where x is file usage frequency
 */
#define START_DAMPING  100.0
#define DAMPING    1000000.0
#define MULTIPLIER    3600.0
/* DAMPING is how slowly popularity changes.
 * MULTIPLIER is to scale values to accesses per hour.
 */

#define AVG_USAGE 0.238 // 40hr/(7days * 24hr/day)
/* for calculating initial popularity for new files
 * Unit: accesses per second.
 */

#include <rocksdb/db.h>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class File;
class Tier;

class Metadata{
	/* File metadata to be stored in and
	 * retrieved from the RocksDB database.
	 */
	friend class boost::serialization::access;
	friend class File;
private:
	uintmax_t access_count_ = 0;
	/* Number of times the file was accessed since last tiering.
	 * Resets to 0 after each popularity calculation.
	 */
	double popularity_ = MULTIPLIER*AVG_USAGE;
	/* Moving average of file usage frequency in accesses per hour.
	 * Used to sort list of files to determine which tiers
	 * they belong in.
	 */
	bool not_found_ = false;
	/* Set to true when the file metadata could not be
	 * retrieved from the database.
	 */
	bool pinned_ = false;
	/* Flag to determine whether to ignore file while tiering.
	 * When true, the file stays in the tier it is in.
	 */
	std::string tier_path_;
	/* The backend path of the tier. This + relative path
	 * is the real location of the file.
	 */
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version){
		(void) version;
		ar & tier_path_;
		ar & access_count_;
		ar & popularity_;
		ar & pinned_;
	}
public:
	Metadata(const char *path, rocksdb::DB *db, Tier *tptr = nullptr);
	/* Try to retrieve data from db. If not found and tptr != nullptr,
	 * new metadata object is initialized and put into the database.
	 * If not found and tptr == nullptr, metadata is left undefined and
	 * not_found_ is set to true.
	 */
	~Metadata(void) = default;
	/* Default destructor.
	 */
	void update(const char *relative_path, rocksdb::DB *db);
	/* Put metadata into database with relative_path as the key.
	 */
	void touch(void);
	/* Increment access_count_.
	 */
	std::string tier_path(void) const;
	/* Get path to tier root.
	 */
	void tier_path(const std::string &path);
	/* Set path to tier root.
	 */
	bool pinned(void) const;
	/* Test if metadata has pinned flag set.
	 */
	void pinned(bool val);
	/* Set pinned flag.
	 */
	bool not_found(void) const;
	/* Test if metadata was found in database.
	 */
	std::string dump_stats(void) const;
	/* Return metadata as formatted string.
	 */
};

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
	File(fs::path full_path, rocksdb::DB *db, Tier *tptr);
	/* File constructor used while tiering.
	 * Grabs metadata from db and other info from stat().
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
	bool is_open(void);
	/* Fork and exec lsof, use return value to determine if the file is
	 * currently open. There is probably a less expensive way of doing this.
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
};
