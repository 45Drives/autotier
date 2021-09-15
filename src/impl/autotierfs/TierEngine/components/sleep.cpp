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

#include "TierEngine/components/sleep.hpp"

TierEngineSleep::TierEngineSleep(const fs::path &config_path, const ConfigOverrides &config_overrides)
    : TierEngineBase(config_path, config_overrides)
    , sleep_mt_() {

}

TierEngineSleep::~TierEngineSleep() {

}

void TierEngineSleep::sleep_until(std::chrono::steady_clock::time_point t) {
    std::unique_lock<std::mutex> lk(sleep_mt_);
	sleep_cv_.wait_until(lk, t, [this](){ return this->stop_flag_ || !this->adhoc_work_.empty(); });
}

void TierEngineSleep::sleep_until_woken(void) {
    std::unique_lock<std::mutex> lk(sleep_mt_);
	sleep_cv_.wait(lk, [this](){ return this->stop_flag_ || !this->adhoc_work_.empty(); });
}
