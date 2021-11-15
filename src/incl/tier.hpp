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

#include <45d/Quota.hpp>
#include <boost/filesystem.hpp>
#include <mutex>
#include <queue>
#include <rocksdb/db.h>
namespace fs = boost::filesystem;

class File;

/**
 * @brief Class to represent each tier in the filesystem.
 *
 */
class Tier {
private:
	ffd::Quota quota_; ///< Holds maximum size and percent allotted
	ffd::Bytes usage_; ///< Real current bytes used in underlying filesystem.
	/**
	 * @brief Number of bytes used, used while simulating
	 * the tiering of files to determine where to place each file.
	 */
	ffd::Bytes sim_usage_;
	/**
	 * @brief User-defined friendly name of tier,
	 * in square bracket header of tier definition
	 * in config file.
	 */
	std::string id_;
	fs::path path_; ///< Backend path to tier.
	/**
	 * @brief Queue of files to be placed into the tier, filled
	 * during the simulation of tiering and used while actually
	 * tiering.
	 */
	std::vector<File *> incoming_files_;
	/**
	 * @brief Copy ownership and permissions from old_path to new_path,
	 * called after copying a file to a different tier.
	 *
	 * @param old_path Path to file before moving
	 * @param new_path Path to file after moving
	 */
	void copy_ownership_and_perms(const fs::path &old_path, const fs::path &new_path) const;
	std::mutex usage_mt_; ///< Mutex to be used in {add,subtract}_file_size() for FUSE threads.
public:
	/**
	 * @brief Construct a new Tier object
	 *
	 * @param id User-defined ID of tier from config subsection header
	 * @param path Path to tier in filesystem
	 */
	Tier(std::string id, const fs::path &path, const ffd::Quota &quota);
	/**
	 * @brief Copy construct a new Tier object
	 *
	 * @param other Tier to be copied
	 */
	Tier(Tier &&other)
		: quota_(std::move(other.quota_))
		, usage_(std::move(other.usage_))
		, sim_usage_(std::move(other.sim_usage_))
		, id_(std::move(other.id_))
		, path_(std::move(other.path_))
		, incoming_files_(std::move(other.incoming_files_))
		, usage_mt_() {}
	/**
	 * @brief Destroy the Tier object
	 *
	 */
	~Tier() = default;
	/**
	 * @brief Add size bytes to usage_.
	 *
	 * @param size
	 */
	void add_file_size(ffd::Bytes size);
	/**
	 * @brief Subtract size bytes from usage.
	 *
	 * @param size
	 */
	void subtract_file_size(ffd::Bytes size);
	/**
	 * @brief Subtract old_size then add new_size, locking mutex for
	 * thread safety.
	 *
	 * @param old_size Subtract this
	 * @param new_size Add this
	 */
	void size_delta(ffd::Bytes old_size, ffd::Bytes new_size);
	/**
	 * @brief Add size bytes to sim_usage_.
	 *
	 * @param size
	 */
	void add_file_size_sim(ffd::Bytes size);
	/**
	 * @brief Subtract size bytes from sim_usage.
	 *
	 * @param size
	 */
	void subtract_file_size_sim(ffd::Bytes size);
	/**
	 * @brief Set quota percentage
	 *
	 * @param quota_percent
	 */
	void quota_percent(double quota_percent);
	/**
	 * @brief Get quota percentage
	 *
	 * @return double
	 */
	double quota_percent(void) const;
	/**
	 * @brief Get quota_
	 *
	 * @return ffd::Quota
	 */
	ffd::Quota quota(void) const;
	/**
	 * @brief returns true if file would make tier overfilled.
	 * (usage_ + file.size() > quota_bytes_).
	 *
	 * @param file File to test
	 * @return true file would make tier overfilled
	 * @return false file would fit in tier
	 */
	bool full_test(const ffd::Bytes &file_size) const;
	/**
	 * @brief Get path to root of tier
	 *
	 * @return const std::string&
	 */
	const fs::path &path(void) const;
	/**
	 * @brief Get user-defined ID of tier
	 *
	 * @return const fs::path&
	 */
	const std::string &id(void) const;
	/**
	 * @brief Push file pointer into incoming_files_.
	 *
	 * @param fptr
	 */
	void enqueue_file_ptr(File *fptr);
	/**
	 * @brief Iterate through incoming_files_ and move each file into
	 * the tier.
	 *
	 * @param buff_sz
	 * @param run_path
	 * @param db
	 */
	void transfer_files(int buff_sz, const fs::path &run_path, std::shared_ptr<rocksdb::DB> &db);
	/**
	 * @brief Called in transfer_files() to actually copy the file and
	 * remove the old one.
	 *
	 * @param old_path
	 * @param new_path
	 * @param buff_sz
	 * @param conflicted
	 * @param orig_tier
	 * @return true
	 * @return false
	 */
	bool move_file(const fs::path &old_path,
				   const fs::path &new_path,
				   int buff_sz,
				   bool *conflicted = nullptr,
				   std::string orig_tier = "") const;
	/**
	 * @brief Set tier usage_ in bytes.
	 *
	 * @param usage
	 */
	void usage(ffd::Bytes usage);
	/**
	 * @brief Return real current usage as percent.
	 *
	 * @return double
	 */
	double usage_percent(void) const;
	/**
	 * @brief Return real current usage in bytes.
	 *
	 * @return ffd::Bytes
	 */
	ffd::Bytes usage_bytes(void) const;
	/**
	 * @brief Set sim_usage_ to zero
	 *
	 */
	void reset_sim(void);
	/**
	 * @brief Return capacity in bytes.
	 *
	 * @return ffd::Bytes
	 */
	ffd::Bytes capacity(void) const;
};
