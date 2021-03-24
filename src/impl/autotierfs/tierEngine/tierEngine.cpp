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

#include "tierEngine.hpp"
#include "alert.hpp"

TierEngine::TierEngine(const fs::path &config_path, const ConfigOverrides &config_overrides, bool read_only)
		: stop_flag_(false), tiers_(), config_(config_path, std::ref(tiers_), config_overrides, read_only), run_path_(config_.run_path()){
	if(create_run_path() != 0){
		Logging::log.error("Could not initialize metadata directory.");
		exit(EXIT_FAILURE);
	}
	open_db(read_only);
}

TierEngine::~TierEngine(void){
	delete db_;
}

void TierEngine::stop(void){
	std::lock_guard<std::mutex> lk(sleep_mt_);
	stop_flag_ = true;
	sleep_cv_.notify_one();
}
