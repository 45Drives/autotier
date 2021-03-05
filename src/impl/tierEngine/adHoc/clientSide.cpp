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

void TierEngine::print_file_pins(fs::directory_entry &file, Tier *tptr, std::atomic<uintmax_t> &usage){
	(void) usage;
	File f(file, db_, tptr);
	if(f.is_pinned()){
		Logging::log.message(f.relative_path().string() + " is pinned to " + f.tier_ptr()->id() , 0);
	}
}

void TierEngine::print_file_popularity(fs::directory_entry &file, Tier *tptr, std::atomic<uintmax_t> &usage){
	(void) usage;
	File f(file, db_, tptr);
	Logging::log.message(f.relative_path().string() + " popularity: " + std::to_string(f.popularity()), 0);
	files_.clear();
}
