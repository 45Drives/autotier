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

#include "concurrentQueue.hpp"
#include "config.hpp"
#include "tier.hpp"
#include "tools.hpp"

#include <boost/filesystem.hpp>
#include <condition_variable>
#include <list>
namespace fs = boost::filesystem;

/**
 * @brief Base class of TierEngine. Deals with calling config_ constructor and holds
 * onto some members used in multiple other components.
 *
 */
class TierEngineBase {
public:
	/**
	 * @brief Construct a new Tier Engine Base object
	 *
	 * @param config_path
	 * @param config_overrides
	 */
	TierEngineBase(const fs::path &config_path, const ConfigOverrides &config_overrides);
	/**
	 * @brief Destroy the Tier Engine Base object
	 *
	 */
	~TierEngineBase(void);
	/**
	 * @brief Creates path for FIFOs and database, chowning to root:autotier
	 *
	 * @return int 0 if okay, -1 if error.
	 */
	int create_run_path(void) const;
	/**
	 * @brief Get reference to the list of tiers.Used in fusePassthrough.cpp to
	 * get references to each tier for finding full backend paths.
	 *
	 * @return std::list<Tier>& Reference to list of tiers.
	 */
	std::list<Tier> &get_tiers(void);
	/**
	 * @brief Find Tier from path
	 *
	 * @param p Backend path to tier
	 * @return Tier* Tier matching path if found, else nullptr
	 */
	Tier *tier_lookup(fs::path p);
	/**
	 * @brief Find tier from ID
	 *
	 * @param id ID of tier from config
	 * @return Tier* Tier matching ID if found, else nullptr
	 */
	Tier *tier_lookup(std::string id);
	/**
	 * @brief Set the mount_point_ variable
	 *
	 * @param mount_point Path to filesystem mount point
	 */
	void mount_point(const fs::path &mount_point);
	/**
	 * @brief Virtual tier function to allow other components to call TierEngineTiering::tier().
	 *
	 * @return true Tiering finished successfully
	 * @return false Tiering failed
	 */
	virtual bool tier(void);
	virtual bool currently_tiering(void) const;
protected:
	/**
	 * @brief Set to false to make thread exit. Used to continue
	 * or cancel sleeping after being woken to do ad hoc
	 * command work.
	 *
	 */
	bool stop_flag_;
	std::list<Tier> tiers_; ///< List of tiers built from configuration file.
	Config config_;         ///< Configuration read from config_file path
	/**
	 * @brief Path to mutex lock file, ad hoc FIFO, and database.
	 * Defaults to /var/lib/autotier/<hash of config path>, can be overridden in config.
	 */
	fs::path run_path_;
	fs::path mount_point_; ///< Where autotier filesystem is mounted, set by mount_point().
	ConcurrentQueue<AdHoc> adhoc_work_; ///< Single-consumer concurrentQueue for the ad hoc command
										///< server to queue work.
	/**
	 * @brief Condition variable to use wait_until to sleep between tiering,
	 * used for the ad hoc server thread or the main thread to wake the tiering
	 * thread from sleep.
	 *
	 */
	std::condition_variable sleep_cv_;
	std::shared_ptr<rocksdb::DB> db_; ///< Nosql database holding file metadata.
	/**
	 * @brief Virtual exit function that can be overridden by other components for cleanup
	 *
	 * @param status Exit code of process
	 */
	virtual void exit(int status) const;
};
