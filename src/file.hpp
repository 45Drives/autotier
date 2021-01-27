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
#define DAMPING 1000000.0
#define MULTIPLIER 1000.0
/* DAMPING is how slowly popularity changes
 * TIME_DIFF is time since last file access
 * MULTIPLIER is to scale values
 */

#define AVG_USAGE 0.238 // 40hr/(7days * 24hr/day)
/* for calculating initial popularity for new files
 */

#include <rocksdb/db.h>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class File;
class Tier;

class Metadata{
	friend class boost::serialization::access;
	friend class File;
private:
	uintmax_t access_count_ = 0;
	double popularity_ = MULTIPLIER*AVG_USAGE;
	bool not_found_ = false;
	bool pinned_ = false;
	std::string tier_path_;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version){
		(void) version;
		ar & tier_path_;
		ar & access_count_;
		ar & popularity_;
		ar & pinned_;
	}
	void put_info(const char *key, rocksdb::DB *db);
	/* put info into db
	 */
public:
	Metadata(const char *path, rocksdb::DB *db, Tier *tptr = nullptr);
	void update(const char *relative_path, rocksdb::DB *db);
	~Metadata(void) = default;
	void touch(void);
	std::string tier_path(void) const;
	bool not_found(void) const;
	std::string dump_stats(void) const;
};

class File{
private:
	// not in database
	uintmax_t size_ = 0;
	Tier *tier_ptr_;
	struct timeval times_[2];
	time_t atime_;
	fs::path relative_path_;
	rocksdb::DB *db_;
	
	// in database
	Metadata metadata;
public:
	File(fs::path full_path, rocksdb::DB *db, Tier *tptr);
	/* file constructor used while tiering
	 * overwrites current tier path based on tptr if tptr != NULL,
	 * else does not modify current tier path and
	 * grabs current tier path from db
	 */
	File(fs::path rel_path, rocksdb::DB *db);
	~File();
	/* destructor - calls put_info(db)
	 */
	void calc_popularity(double period_seconds);
	/* calculate new popularity value of file
	 */
	bool is_open(void);
	/* fork and exec lsof, use return value to determine if the file is
	 * currently open. There is probably a less expensive way of doing this.
	 */
	fs::path full_path(void) const;
	fs::path relative_path(void) const;
	Tier *tier_ptr(void) const;
	double popularity(void) const;
	struct timeval atime(void) const;
	bool is_pinned(void) const;
	void pin(void);
	void unpin(void);
	uintmax_t size(void) const;
	void transfer_to_tier(Tier *tptr);
	void overwrite_times(void) const;
};
