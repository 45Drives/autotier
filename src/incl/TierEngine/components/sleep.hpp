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

#include <chrono>
#include <mutex>

/**
 * @brief TierEngine component for dealing with thread sleeping.
 *
 */
class TierEngineSleep : virtual public TierEngineBase {
public:
	/**
	 * @brief Construct a new Tier Engine Sleep object
	 *
	 * @param config_path
	 * @param config_overrides
	 */
	TierEngineSleep(const fs::path &config_path, const ConfigOverrides &config_overrides);
	/**
	 * @brief Destroy the Tier Engine Sleep object
	 *
	 */
	~TierEngineSleep(void);
	/**
	 * @brief call wait_until on the condition variable. Puts thread
	 * to sleep until time reaches t or woken by sleep_cv_.notify_one()
	 *
	 * @param t Time to wake up by
	 */
	void sleep_until(std::chrono::steady_clock::time_point t);
	/**
	 * @brief call wait on the condition variable. Puts thread
	 * to sleep until woken by sleep_cv_.notify_one()
	 *
	 */
	void sleep_until_woken(void);
protected:
	std::mutex sleep_mt_; ///< Useless lock to release for the condition_variable to use wait_until.
};