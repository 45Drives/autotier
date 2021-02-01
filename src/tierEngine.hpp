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

#include "config.hpp"
#include "tier.hpp"
#include "file.hpp"
#include "tools.hpp"
#include "concurrentQueue.hpp"
#include <rocksdb/db.h>
#include <string>
#include <mutex>
#include <condition_variable>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

fs::path pick_run_path(const fs::path &config_path);
/* Returns /run/autotier/<std::hash of config_path.string()>/
 * or ~/.local/run/autotier/<std::has of config_path.string()>/
 * if permission denied.
 */

class TierEngine{
private:
	bool stop_flag_;
	/* Set to false to make thread exit. Used to continue
	 * or cancel sleeping after being woken to do ad hoc
	 * command work.
	 */
	std::mutex sleep_mt_;
	/* Useless lock to release for the condition_variable
	 * to use wait_until.
	 */
	std::condition_variable sleep_cv_;
	/* Condition variable to use wait_until to sleep between tiering,
	 * used for the ad hoc server thread or the main thread to wake the tiering
	 * thread from sleep.
	 */
	rocksdb::DB *db_;
	/* Nosql database holding file metadata.
	 */
	int mutex_;
	/* File handle for mutexing tiering of files.
	 */
	fs::path run_path_;
	/* Path to mutex lock file, ad hoc FIFO, and database.
	 */
	fs::path mount_point_;
	/* Where autotier filesystem is mounted.
	 */
	std::list<Tier> tiers_;
	/* List of tiers build from configuration file.
	 */
	std::vector<File> files_;
	/* Vector to contain every file across all tiers for sorting.
	 */
	Config config_;
	/* Cconfiguration goes here, read in from file in TierEngine::TierEngine()
	 */
	ConcurrentQueue<AdHoc> adhoc_work_;
	/* Single-consumer concurrentQueue for the ad hoc command server to queue work.
	 */
	int lock_mutex(void);
	/* Opens file at mutex_path such that if the file already exists, opening fails.
	 * Uses this as a mutex lock - if the file exists, the critical section is locked.
	 * To unlock, delete the file.
	 */
	void unlock_mutex(void);
	/* Deletes mutex lock file.
	 */
	void open_db(bool read_only);
	/* Opens RocksDB database, if read_only is true, opened read-only.
	 */
public:
	TierEngine(const fs::path &config_path, bool read_only = false);
	/* Loads config, picks run
	 * path with pick_run_path(), constructs mutex_path,
	 * opens database with open_db()
	 */
	~TierEngine(void);
	/* Closes database.
	 */
	std::list<Tier> &get_tiers(void);
	/* Returns reference to list of tiers. Used in fusePassthrough.cpp to
	 * get references to each tier for finding full backend paths.
	 */
	rocksdb::DB *get_db(void);
	/* Return db_, used in fusePassthrough.cpp for getting file
	 * metadata.
	 */
	Tier *tier_lookup(fs::path p);
	/* Return pointer to tier with path_ == p,
	 * if not found, return nullptr.
	 */
	Tier *tier_lookup(std::string id);
	/* Return pointer to tier with id_ == id,
	 * if not found, return nullptr.
	 */
	void begin(bool daemon_mode);
	/* Tier files with tier(), do ad hoc work, and
	 * sleep until next period or woken by more work.
	 */
	void tier(std::chrono::steady_clock::duration period);
	/* Find files, update their popularities, sort the files
	 * by popularity, and finally move the files to their
	 * respective tiers.
	 */
	void launch_crawlers(void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr));
	/* Call crawl() for each tier in tiers.
	 */
	void crawl(fs::path dir, Tier *tptr, void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr));
	/* Recurse into tier directory, executing function on each file.
	 * Function can be emplace_file(), print_file_pins(), or print_file_popularity().
	 */
	void emplace_file(fs::directory_entry &file, Tier *tptr);
	/* Place file into files_, constructing with fs::path, Tier*, and db_.
	 */
	void print_file_pins(fs::directory_entry &file, Tier *tptr);
	/* Construct File from file, if pinned, print tier ID.
	 */
	void print_file_popularity(fs::directory_entry &file, Tier *tptr);
	/* Construct File from file, print popularity.
	 */
	void calc_popularity(std::chrono::steady_clock::duration period);
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
	void sleep_until(std::chrono::steady_clock::time_point t);
	/* call wait_until on the condition variable. Puts thread
	 * to sleep until time reaches t or woken by sleep_cv_.notify_one()
	 */
	void print_tier_info(void);
	/* Iterate through list of tiers, printing ID, path, current usage, and watermark
	 * uses Logger::format_bytes() for printing current usage and watermark.
	 */
	void stop(void);
	/* Obtain sleep_mt_, set stop_flag_ to true,
	 * wake sleeping tier thread with sleep_cv_.notify_one().
	 * Causes sleeping tiering thread to join.
	 */
	void process_adhoc_requests(void);
	/* Function for ad hoc server thread. Listens to named FIFO
	 * in run_path_ to receive ad hoc commands from running
	 * autotier while mounted. Grabs command and queues work as
	 * and AdHoc object.
	 */
	void execute_queued_work(void);
	/* Tiering thread calls this when woken to execute the queued work.
	 */
	void pin_files(const std::vector<std::string> &args);
	/* Pin each file in args[1:] to tier referenced by args[0].
	 * Sets pinned_ flag in file metadata and moves file into tier.
	 */
	void unpin_files(const std::vector<std::string> &args);
	/* Clear pinned_ flag in file metadata.
	 */
	void mount_point(const fs::path &mount_point);
	/* Set the mount_point_ variable.
	 */
};
