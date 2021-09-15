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

class TierEngineAdhoc : virtual public TierEngineBase {
public:
    TierEngineAdhoc(const fs::path &config_path, const ConfigOverrides &config_overrides);
    ~TierEngineAdhoc();
    void process_adhoc_requests(void);
	/* Function for ad hoc server thread. Listens to named FIFO
	 * in run_path_ to receive ad hoc commands from running
	 * autotier while mounted. Grabs command and queues work as
	 * and AdHoc object.
	 */
	void process_oneshot(const AdHoc &work);
	/* Enqueue oneshot AdHoc command into adhoc_work_.
	 */
	void process_pin_unpin(const AdHoc &work);
	/* Enqueue pin or unpin AdHoc command into adhoc_work_.
	 */
	void process_status(const AdHoc &work);
	/* Iterate through list of tiers, printing ID, path, current usage, and watermark
	 * uses Logger::format_bytes() for printing current usage and watermark.
	 */
	void process_config(void);
	/* Dump current configuration settings from memory.
	 */
	void process_list_pins(void);
	/* Print pineed files.
	 */
	void process_list_popularity(void);
	/* Print popularity of each file.
	 */
	void process_which_tier(AdHoc &work);
	/* Return table of each argument file along with its corresponding tier name
	 * and full backend path.
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
private:
};
