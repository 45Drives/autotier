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

#include "adhoc.hpp"
#include "database.hpp"
#include "mutex.hpp"
#include "sleep.hpp"

#include <chrono>

/**
 * @brief TierEngine component to deal with tiering, inherits all other components
 * so this is essentially the entirety of TierEngine.
 *
 */
class TierEngineTiering
	: public TierEngineDatabase
	, public TierEngineSleep
	, public TierEngineAdhoc
	, public TierEngineMutex {
public:
	/**
	 * @brief Construct a new Tier Engine Tiering object
	 *
	 * @param config_path Path to config file
	 * @param config_overrides Config overrides from main()
	 */
	TierEngineTiering(const fs::path &config_path, const ConfigOverrides &config_overrides);
	/**
	 * @brief Destroy the Tier Engine Tiering object
	 *
	 */
	~TierEngineTiering(void);
	/**
	 * @brief Tier files with tier(), do ad hoc work, and
	 * sleep until next period or woken by more work.
	 *
	 * @param daemon_mode Whether or not to daemonise
	 */
	void begin(bool daemon_mode);
	/**
	 * @brief Find files, update their popularities, sort the files
	 * by popularity, and finally move the files to their
	 * respective tiers. Returns true if tiering happened,
	 * false if failed to lock mutex.
	 *
	 * @return true Tiered successfully
	 * @return false Failed to lock mutex
	 */
	bool tier(void);
	/**
	 * @brief Call crawl() for each tier in tiers.
	 *
	 * @param function Function to execute for each tier
	 */
	void launch_crawlers(void (TierEngineTiering::*function)(
		fs::directory_entry &itr, Tier *tptr, std::atomic<ffd::Bytes::bytes_type> &usage));
	/**
	 * @brief Recurse into tier directory, executing function on each file.
	 * Function can be emplace_file(), print_file_pins(), or print_file_popularity().
	 *
	 * @param dir Current directory path during recursion
	 * @param tptr Pointer to tier it is running for
	 * @param function Function to execute on each file
	 * @param usage Keeps track of recursive file size for reporting tier usage
	 */
	void crawl(fs::path dir,
			   Tier *tptr,
			   void (TierEngineTiering::*function)(fs::directory_entry &itr,
												   Tier *tptr,
												   std::atomic<ffd::Bytes::bytes_type> &usage),
			   std::atomic<ffd::Bytes::bytes_type> &usage);
	/**
	 * @brief Place file into files_, constructing with fs::path, Tier*, and db_.
	 *
	 * @param file Directory entry for file, containing path
	 * @param tptr Tier the file was found in
	 * @param usage Keeps track of recursive file size for reporting tier usage
	 */
	void
	emplace_file(fs::directory_entry &file, Tier *tptr, std::atomic<ffd::Bytes::bytes_type> &usage);
	/**
	 * @brief Call File::calc_popularity() for each file in files_.
	 *
	 */
	void calc_popularity(void);
	/**
	 * @brief Sorts list of files based on popularity, if pop1 == pop2, sort by atime.
	 *
	 */
	void sort(void);
	/**
	 * @brief Find out which tier each file belongs in
	 *
	 * Pseudocode:
	 * LET file = first file of sorted files
	 * LET tier = first tier
	 * LET tier_usage = size of pinned files
	 * WHILE file != last file of sorted files DO
	 *    IF tier_usage + size of file > tier watermark AND tier != last tier THEN
	 *        tier = next tier
	 *        tier_usage = 0
	 *    END IF
	 *    file belongs to tier (push file into tier.incoming_files_)
	 *    tier_usage += size of file
	 *    file = next file
	 * END DO
	 *
	 */
	void simulate_tier(void);
	/**
	 * @brief Launch one thread for each tier to move incoming files into their new
	 * backend paths based on results of simulate_tier().
	 *
	 */
	void move_files(void);
	/**
	 * @brief Iterate over file list and call File::update_db().
	 * 
	 */
	void update_db(void);
	/**
	 * @brief Obtain sleep_mt_, set stop_flag_ to true,
	 * wake sleeping tier thread with sleep_cv_.notify_one().
	 * Causes sleeping tiering thread to join.
	 *
	 */
	void stop(void);
	/**
	 * @brief Check if currently tiering.
	 *
	 * @return true Tiering
	 * @return false Not tiering
	 */
	bool currently_tiering(void) const;
	/**
	 * @brief Return config_.strict_period().
	 *
	 * @return true Tier period is strict (no automatic tiers)
	 * @return false Tiering can happen whenever needed
	 */
	bool strict_period(void) const;
	/**
	 * @brief Unlock mutex and call stop() before calling ::exit()
	 *
	 * @param status Exit status of process
	 */
	void exit(int status);
private:
	bool currently_tiering_; ///< Whether or not tiering is happening. Set and cleared in tier()
	std::chrono::steady_clock::time_point last_tier_time_; ///< For determining tier period.
	std::vector<File> files_; ///< Vector to contain every file across all tiers for sorting.
};
