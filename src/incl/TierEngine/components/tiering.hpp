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

#include "database.hpp"
#include "sleep.hpp"
#include "adhoc.hpp"
#include "mutex.hpp"

#include <chrono>

class TierEngineTiering : public TierEngineDatabase, public TierEngineSleep, public TierEngineAdhoc, public TierEngineMutex {
public:
    TierEngineTiering(const fs::path &config_path, const ConfigOverrides &config_overrides);
    ~TierEngineTiering(void);
	void begin(bool daemon_mode);
	/* Tier files with tier(), do ad hoc work, and
	 * sleep until next period or woken by more work.
	 */
	bool tier(void);
	/* Find files, update their popularities, sort the files
	 * by popularity, and finally move the files to their
	 * respective tiers. Returns true if tiering happened,
	 * false if failed to lock mutex.
	 */
	void launch_crawlers(void (TierEngineTiering::*function)(fs::directory_entry &itr, Tier *tptr, std::atomic<ffd::Bytes::bytes_type> &usage));
	/* Call crawl() for each tier in tiers.
	 */
	void crawl(fs::path dir, Tier *tptr, void (TierEngineTiering::*function)(fs::directory_entry &itr, Tier *tptr, std::atomic<ffd::Bytes::bytes_type> &usage), std::atomic<ffd::Bytes::bytes_type> &usage);
	/* Recurse into tier directory, executing function on each file.
	 * Function can be emplace_file(), print_file_pins(), or print_file_popularity().
	 */
	void emplace_file(fs::directory_entry &file, Tier *tptr, std::atomic<ffd::Bytes::bytes_type> &usage);
	/* Place file into files_, constructing with fs::path, Tier*, and db_.
	 */
	void calc_popularity(void);
	/* Call File::calc_popularity() for each file in files_.
	 */
	void sort(void);
	/* Sorts list of files based on popularity, if pop1 == pop2, sort by atime.
	 */
	void simulate_tier(void);
	/* Find out which tier each file belongs in:
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
	 */
	void move_files(void);
	/* Start at last tier, move files into tier based on outcome of simulate_tier().
	 */
	void stop(void);
	/* Obtain sleep_mt_, set stop_flag_ to true,
	 * wake sleeping tier thread with sleep_cv_.notify_one().
	 * Causes sleeping tiering thread to join.
	 */
    bool currently_tiering(void) const;
	/* Check if currently tiering.
	 */
	bool strict_period(void) const;
	/* Return config_.strict_period().
	 */
private:
    bool currently_tiering_;
	/* Set and cleared in tier()
	 */
	std::chrono::steady_clock::time_point last_tier_time_;
	/* For determining tier period.
	 */
	std::vector<File> files_;
	/* Vector to contain every file across all tiers for sorting.
	 */
};
