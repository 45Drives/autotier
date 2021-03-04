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

#include "popularityCalc.hpp"
#include <rocksdb/db.h>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

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
// 		ar & last_popularity_calc_;
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
	void update(std::string relative_path, rocksdb::DB *db, std::string *old_key = nullptr);
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
