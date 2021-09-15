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

/**
 * @brief TierEngine component for ensuring only one instance of autotier runs for a given run path
 * 
 */
class TierEngineMutex : virtual public TierEngineBase {
public:
    /**
     * @brief Construct a new Tier Engine Mutex object
     * 
     * @param config_path Path to config file
     * @param config_overrides Config Overrides from main()
     */
    TierEngineMutex(const fs::path &config_path, const ConfigOverrides &config_overrides);
    /**
     * @brief Destroy the Tier Engine Mutex object, calling unlock_mutex() first.
     * 
     */
    ~TierEngineMutex(void);
protected:
    /**
     * @brief Used to ensure currently_tiering_ is set atomically with locking the
     * file mutex.
     * 
     */
	std::mutex lock_file_mt_;
    /**
     * @brief Opens file at mutex_path such that if the file already exists, opening fails.
     * Uses this as a mutex lock - if the file exists, the critical section is locked.
     * To unlock, delete the file.
     * 
     * @return int 0 if locked, -1 if failed to lock.
     */
    int lock_mutex(void);
    /**
     * @brief Deletes mutex lock file, unlocking the critical section.
     * 
     */
    void unlock_mutex(void);
private:
    int mutex_; ///< File handle for mutexing tiering of files.
};