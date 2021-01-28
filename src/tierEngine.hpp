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

class TierEngine{
private:
	bool stop_flag_;
	std::mutex sleep_mt_;
	std::condition_variable sleep_cv_;
	rocksdb::DB *db_;
	/* database holding file metadata:
	 * table Files:
	 * ID | relative path | current tier path | pin path | popularity | last atime
	 */
	int mutex_;
	/* file handle for mutexing tiering of files
	 */
	fs::path run_path_;
	/* path to mutex lock file
	 */
	fs::path get_mutex_name(const fs::path &config_path);
	/* returns RUN_PATH/<hash of config file path>.autotier.lock
	 */
	int lock_mutex(void);
	/* opens file at mutex_path such that if the file already exists, opening fails.
	 * Uses this as a mutex lock - if the file exists, the critical section is locked.
	 * To unlock, delete the file
	 */
	void unlock_mutex(void);
	/* deletes mutex lock file
	 */
	void open_db(bool read_only);
	/* creates connection with sqlite database db
	 */
	std::string print_bytes(unsigned long long bytes);
	/* formats number of bytes as either bytes, SI units (base 1000),
	 * or binary units (base 1024). Accepts number of bytes, returns formatted 
	 * string
	 */
	std::list<Tier> tiers;
	/* list of tiers build from configuration file
	 */
	std::list<File> files;
	/* list to contain every file across all tiers for sorting
	 */
	Config config_;
	/* configuration goes here, read in from file in TierEngine::TierEngine()
	 */
	ConcurrentQueue<AdHoc> adhoc_work_;
public:
	TierEngine(const fs::path &config_path, bool read_only = false);
	/* attaches signal handlers, loads config, picks run path for mutex lock and
	 * db with pick_run_path(), constructs mutex_path,
	 * opens database with open_db()
	 */
	~TierEngine(void);
	/* closes database
	 */
	std::list<Tier> &get_tiers(void);
	/* returns reference to list of tiers
	 */
	rocksdb::DB *get_db(void);
	Tier *tier_lookup(fs::path p);
	Tier *tier_lookup(std::string id);
	/* returns pointer to tier either with t->dir == p
	 * or with t->id == id
	 * useful for file pinning
	 */
	Config *get_config(void);
	/* returns pointer to config for modification
	 */
	void begin(bool daemon_mode);
	/* IF daemon_mode THEN
	 * starts tiering daemon
	 *    WHILE running DO
	 *      launch_crawlers(), calc_popularity() once per second
	 *      sort(), simulate_tier(), move_files() once per tier period
	 *    END DO
	 * mounts filesystem
	 * ELSE
	 * tiers files once
	 * END IF
	 */
	void sleep(std::chrono::steady_clock::duration t);
	void launch_crawlers(void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr));
	/* crawl() for each tier in tiers
	 */
	void crawl(fs::path dir, Tier *tptr, void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr));
	/* recurse into tier directory, executing function on each file.
	 * Function can only be emplace_file(), so this should be changed to always emplace files
	 */
	void emplace_file(fs::directory_entry &file, Tier *tptr);
	/* place file into std::list<File> files, constructing with fs::path, Tier*, and db
	 */
	void print_file_pins(void);
	/* iterate through list of files, printing old_path and pinnned_to to std::cout
	 */
	void print_file_popularity(void);
	/* iterate through list of files, printing popularity
	 */
	void sort(void);
	/* sorts list of files based on popularity, if pop1 == pop2, sort by atime
	 */
	void simulate_tier(void);
	/* find out which tier each file belongs in
	 * LET file = first file of sorted files
	 * LET tier = first tier
	 * LET tier_usage = size of pinned files
	 * WHILE file != last file of sorted files DO
	 *    IF tier_usage + size of file > tier watermark AND tier != last tier THEN
	 *        tier = next tier
	 *        tier_usage = 0
	 *    END IF
	 *    file belongs to tier (push &file into Tier::incoming_files)
	 *    tier_usage += size of file
	 *    file = next file
	 * END DO
	 */
	void move_files(void);
	/* start at last tier, move files into tier based on outcome of simulate_tier()
	 */
	void print_tier_info(void);
	/* iterate through list of tiers, printing ID, path, current usage, and watermark
	 * uses format_bytes() for printing current usage and watermark
	 */
	void pin_files(std::string tier_name, std::vector<fs::path> &files_);
	/* pin each file in files_ to tier referenced by tier_name
	 * TODO: remove non-existant files from db
	 * possibly just use SQL directly to accomplish this
	 */
	void unpin(int optind, int argc, char *argv[]);
	/* remove pinned_to path from each file passed
	 * TODO: remove non-existant files from db
	 * possibly just use SQL directly to accomplish this
	 */
	void calc_popularity(void);
	/* call File::calc_popularity() for each file in files
	 */
	void stop(void);
	void process_adhoc_requests(void);
};
