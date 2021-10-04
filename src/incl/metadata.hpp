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

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <rocksdb/db.h>

class Tier;

/**
 * @brief File metadata to be stored in and
 * retrieved from the RocksDB database.
 *
 */
class Metadata {
	friend class boost::serialization::access;
	friend class File;
	friend class MetadataViewer;
public:
	/**
	 * @brief Construct a new empty Metadata object
	 *
	 */
	Metadata(void);
	/**
	 * @brief Construct a new Metadata object from serialized string
	 *
	 * @param serialized Serialized string representing Metadata object
	 */
	Metadata(const std::string &serialized);
	/**
	 * @brief Copy construct a new Metadata object
	 *
	 * @param other
	 */
	Metadata(const Metadata &other);
	/**
	 * @brief Copy assign a new Metadata object
	 * 
	 * @param other 
	 * @return Metadata& 
	 */
	Metadata &operator=(const Metadata &other);
	/**
	 * @brief Move construct a new Metadata object
	 * 
	 * @param other 
	 */
	Metadata(Metadata &&other);
	/**
	 * @brief Move assign a new Metadata object
	 * 
	 * @param other 
	 * @return Metadata& 
	 */
	Metadata &operator=(Metadata &&other);
	/**
	 * @brief Destroy the Metadata object
	 *
	 */
	~Metadata(void) = default;
#ifndef BAREBONES_METADATA
	/**
	 * @brief Construct a new Metadata object.
	 * Try to retrieve data from db. If not found and tptr != nullptr,
	 * new metadata object is initialized and put into the database.
	 * If not found and tptr == nullptr, metadata is left undefined and
	 * not_found_ is set to true.
	 *
	 * @param path
	 * @param db
	 * @param tptr
	 */
	Metadata(std::string path, std::shared_ptr<rocksdb::DB> db, Tier *tptr = nullptr);
	/**
	 * @brief Put metadata into database with relative_path as the key.
	 *
	 * @param relative_path Database key
	 * @param db Pointer to RocksDB database
	 * @param old_key string pointer to old key to remove if not nullptr
	 */
	void update(std::string relative_path, std::shared_ptr<rocksdb::DB> db, std::string *old_key = nullptr);
	/**
	 * @brief Increment access_count_.
	 *
	 */
	void touch(void);
	/**
	 * @brief Get path to tier root.
	 *
	 * @return std::string
	 */
	std::string tier_path(void) const;
	/**
	 * @brief Set path to tier root.
	 *
	 * @param path
	 */
	void tier_path(const std::string &path);
	/**
	 * @brief Test if metadata has pinned flag set.
	 *
	 * @return true File corresponding to metadata is pinned
	 * @return false File corresponding to metadata is not pinned
	 */
	bool pinned(void) const;
	/**
	 * @brief Set pinned flag.
	 *
	 * @param val
	 */
	void pinned(bool val);
	/**
	 * @brief Get popularity.
	 *
	 * @return double Popularity of file
	 */
	double popularity(void) const;
	/**
	 * @brief Test if metadata was found in database.
	 *
	 * @return true metadata was not found
	 * @return false metadata was found
	 */
	bool not_found(void) const;
	/**
	 * @brief Return metadata as formatted string.
	 *
	 * @return std::string
	 */
	std::string dump_stats(void) const;
#endif
private:
	/**
	 * @brief Number of times the file was accessed since last tiering.
	 * Resets to 0 after each popularity calculation.
	 *
	 */
	uintmax_t access_count_ = 0;
	/**
	 * @brief Moving average of file usage frequency in accesses per hour.
	 * Used to sort list of files to determine which tiers
	 * they belong in.
	 *
	 */
	double popularity_ = MULTIPLIER * AVG_USAGE;
	/**
	 * @brief Set to true when the file metadata could not be
	 * retrieved from the database.
	 *
	 */
	bool not_found_ = false;
	/**
	 * @brief Flag to determine whether to ignore file while tiering.
	 * When true, the file stays in the tier it is in.
	 *
	 */
	bool pinned_ = false;
	/**
	 * @brief The backend path of the tier. This + relative path
	 * is the real location of the file.
	 *
	 */
	std::string tier_path_;
	/**
	 * @brief Serialize method for boost::serialize
	 *
	 * @tparam Archive Template type
	 * @param ar Internal boost::serialize object
	 * @param version boost::serialize version (unused)
	 */
	template<class Archive>
	void serialize(Archive &ar, const unsigned int version) {
		(void)version;
		ar &tier_path_;
		ar &access_count_;
		ar &popularity_;
		ar &pinned_;
	}
};
