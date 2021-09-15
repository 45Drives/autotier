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

class TierEngineMutex : virtual public TierEngineBase {
public:
    TierEngineMutex(const fs::path &config_path, const ConfigOverrides &config_overrides);
    ~TierEngineMutex(void);
protected:
    int lock_mutex(void);
    /* Opens file at mutex_path such that if the file already exists, opening fails.
        * Uses this as a mutex lock - if the file exists, the critical section is locked.
        * To unlock, delete the file.
        */
    void unlock_mutex(void);
    /* Deletes mutex lock file.
        */
private:
    int mutex_;
	/* File handle for mutexing tiering of files.
	 */
	std::mutex lock_file_mt_;
	/* Used to ensure currently_tiering_ is set atomically with locking the
	 * file mutex.
	 */
};