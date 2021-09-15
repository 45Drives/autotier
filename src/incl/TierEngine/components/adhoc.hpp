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

#include "base.hpp"
#include "concurrentQueue.hpp"
#include "tools.hpp"

/**
 * @brief TierEngine component to handle ad hoc commands.
 * 
 */
class TierEngineAdhoc : virtual public TierEngineBase {
public:
	/**
	 * @brief Construct a new Tier Engine Adhoc object
	 * 
	 * @param config_path Path to config file
	 * @param config_overrides Config options overridden from main()
	 */
    TierEngineAdhoc(const fs::path &config_path, const ConfigOverrides &config_overrides);
	/**
	 * @brief Destroy the Tier Engine Adhoc object
	 * 
	 */
    ~TierEngineAdhoc();
	/**
	 * @brief Function for ad hoc server thread. Listens to named FIFO
	 * in run_path_ to receive ad hoc commands from running
	 * autotier while mounted. Grabs command and queues work as
	 * an AdHoc object in TierEngineBase::adhoc_work_.
	 * 
	 */
    void process_adhoc_requests(void);
	/**
	 * @brief Enqueue oneshot AdHoc command into adhoc_work_.
	 * 
	 * @param work The command to be processed, containing needed args_.
	 */
	void process_oneshot(const AdHoc &work);
	/**
	 * @brief Enqueue pin or unpin AdHoc command into adhoc_work_.
	 * 
	 * @param work The command to be processed, containing needed args_.
	 */
	void process_pin_unpin(const AdHoc &work);
	/**
	 * @brief Iterate through list of tiers, printing ID, path, current usage, and watermark.
	 * 
	 * @param work The command to be processed, containing needed args_.
	 */
	void process_status(const AdHoc &work);
	/**
	 * @brief Dump current configuration settings from memory.
	 * 
	 */
	void process_config(void);
	/**
	 * @brief Send all pinned files with the corresponding tier they are pinned to.
	 * 
	 */
	void process_list_pins(void);
	/**
	 * @brief Send all file paths in filesystem along with popularity.
	 * 
	 */
	void process_list_popularity(void);
	/**
	 * @brief Send table of each argument file along with its corresponding tier name
	 * and full backend path.
	 * 
	 * @param work The command to be processed, containing needed args_.
	 */
	void process_which_tier(AdHoc &work);
	/**
	 * @brief Process each Adhoc job enqueued in adhoc_queue_, popping them.
	 * Called by the tiering thread as part of the main tier loop in begin().
	 */
	void execute_queued_work(void);
	/**
	 * @brief Iterate through each string in args[1:] to tier name in args[0].
	 * Sets Metadata::pinned_ flag of each file then moves the file.
	 * 
	 * @param args Tier name and list of files to pin.
	 */
	void pin_files(const std::vector<std::string> &args);
	/**
	 * @brief Iterate through args, clearing the Metadata::pinned_ flag of each file.
	 * 
	 * @param args List of file paths to unpin.
	 */
	void unpin_files(const std::vector<std::string> &args);
private:
};
