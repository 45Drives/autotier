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

#include "TierEngine/components/mutex.hpp"

extern "C" {
	#include <fcntl.h>
}

TierEngineMutex::TierEngineMutex(const fs::path &config_path, const ConfigOverrides &config_overrides)
    : TierEngineBase(config_path, config_overrides)
    , lock_file_mt_() {

}

TierEngineMutex::~TierEngineMutex(void) {
    unlock_mutex();
}

int TierEngineMutex::lock_mutex(void) {
	int result = open((run_path_ / "autotier.lock").c_str(), O_CREAT|O_EXCL, 0700);
	close(result);
	return result;
}

void TierEngineMutex::unlock_mutex(void) {
	fs::remove(run_path_ / "autotier.lock");
}
