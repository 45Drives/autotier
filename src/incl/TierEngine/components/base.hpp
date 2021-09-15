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
#include "concurrentQueue.hpp"

#include <list>
#include <condition_variable>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class TierEngineBase {
public:
    TierEngineBase(const fs::path &config_path, const ConfigOverrides &config_overrides);
	~TierEngineBase(void);
    int create_run_path(void) const;
	/* Creates path for FIFOs and database, chowning to root:autotier
	 */
    std::list<Tier> &get_tiers(void);
	/* Returns reference to list of tiers. Used in fusePassthrough.cpp to
	 * get references to each tier for finding full backend paths.
	 */
    Tier *tier_lookup(fs::path p);
	/* Return pointer to tier with path_ == p,
	 * if not found, return nullptr.
	 */
	Tier *tier_lookup(std::string id);
	/* Return pointer to tier with id_ == id,
	 * if not found, return nullptr.
	 */
	void mount_point(const fs::path &mount_point);
	/* Set the mount_point_ variable.
	 */
	virtual bool tier(void);
protected:
	bool stop_flag_;
	/* Set to false to make thread exit. Used to continue
	 * or cancel sleeping after being woken to do ad hoc
	 * command work.
	 */
	std::list<Tier> tiers_;
	/* List of tiers build from configuration file.
	 */
	Config config_;
	/* Cconfiguration goes here, read in from file in TierEngine::TierEngine()
	 */
	fs::path run_path_;
	/* Path to mutex lock file, ad hoc FIFO, and database.
	 */
	fs::path mount_point_;
	/* Where autotier filesystem is mounted.
	 */
	ConcurrentQueue<AdHoc> adhoc_work_;
	/* Single-consumer concurrentQueue for the ad hoc command server to queue work.
	 */
	std::condition_variable sleep_cv_;
	/* Condition variable to use wait_until to sleep between tiering,
	 * used for the ad hoc server thread or the main thread to wake the tiering
	 * thread from sleep.
	 */
	rocksdb::DB *db_;
	/* Nosql database holding file metadata.
	 */
};
