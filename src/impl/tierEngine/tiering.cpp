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

void TierEngine::begin(bool daemon_mode){
	Logging::log.message("autotier started.", 1);
	auto last_tier_time = std::chrono::steady_clock::now() - config_.tier_period_s();
	do{
		auto tier_time = std::chrono::steady_clock::now();
		auto wake_time = tier_time + config_.tier_period_s();
		tier(tier_time - last_tier_time);
		last_tier_time = std::chrono::steady_clock::now();
		// don't wait for oneshot execution
		while(daemon_mode && std::chrono::steady_clock::now() < wake_time && !stop_flag_){
			execute_queued_work();
			sleep_until(wake_time);
		}
	}while(daemon_mode && !stop_flag_);
}

void TierEngine::tier(std::chrono::steady_clock::duration period){
	launch_crawlers(&TierEngine::emplace_file);
	// one popularity calculation per loop
	calc_popularity(period);
	// mutex lock
	if(lock_mutex() == -1){
		Logging::log.warning("autotier already moving files.");
	}else{
		// mutex locked
		sort();
		simulate_tier();
		move_files();
		Logging::log.message("Tiering complete.", 1);
		unlock_mutex();
	}
	files_.clear();
}

void TierEngine::launch_crawlers(void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr, std::atomic<uintmax_t> &usage)){
	Logging::log.message("Gathering files.", 2);
	// get ordered list of files in each tier
	for(std::list<Tier>::iterator t = tiers_.begin(); t != tiers_.end(); ++t){
		std::atomic<uintmax_t> usage(0);
		crawl(t->path(), &(*t), function, usage);
		t->usage(usage);
	}
}

void TierEngine::crawl(fs::path dir, Tier *tptr, void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr, std::atomic<uintmax_t> &usage), std::atomic<uintmax_t> &usage){
	// TODO: Replace this with multithreaded BFS
	for(fs::directory_iterator itr{dir}; itr != fs::directory_iterator{}; *itr++){
		fs::file_status status = fs::symlink_status(*itr);
		if(fs::is_directory(status)){
			crawl(*itr, tptr, function, usage);
		}else if(!is_symlink(status)){
			(this->*function)(*itr, tptr, usage);
		}
	}
}

void TierEngine::emplace_file(fs::directory_entry &file, Tier *tptr, std::atomic<uintmax_t> &usage){
	files_.emplace_back(file.path(), db_, tptr);
	uintmax_t size = files_.back().size();
	usage += size;
	if(files_.back().is_pinned()){
		files_.pop_back();
	}else{
		tptr->subtract_file_size_sim(size); // remove file size from current usage
	}
}

void TierEngine::calc_popularity(std::chrono::steady_clock::duration period){
	Logging::log.message("Calculating file popularity.", 2);
	double period_d = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1,1>>>(period).count();
	Logging::log.message("Real period for popularity calc: " + std::to_string(period_d), 2);
	for(std::vector<File>::iterator f = files_.begin(); f != files_.end(); ++f){
		f->calc_popularity(period_d);
	}
}

void TierEngine::sort(void){
	Logging::log.message("Sorting files.", 2);
	// TODO: use std::execution::par for parallel sort after changing files_ to vector
	// NOTE: std::execution::par requires C++17
	std::sort(files_.begin(), files_.end(),
		[](const File &a, const File &b){
			if(a.popularity() == b.popularity()){
				struct timeval a_t = a.atime();
				struct timeval b_t = b.atime();
				if(a_t.tv_sec == b_t.tv_sec)
					return a_t.tv_usec > b_t.tv_usec;
				return a_t.tv_sec > b_t.tv_sec;
			}
			return a.popularity() > b.popularity();
		}
	);
}

void TierEngine::simulate_tier(void){
	Logging::log.message("Finding files' tiers.", 2);
	for(Tier &t : tiers_)
		t.get_capacity_and_usage();
	std::vector<File>::iterator fptr = files_.begin();
	std::list<Tier>::iterator tptr = tiers_.begin();
	while(fptr != files_.end()){
		if(tptr->full_test(*fptr)){
			if(std::next(tptr) != tiers_.end()){
				// move to next tier
				++tptr;
			} // else: out of space!
		}
		tptr->add_file_size_sim(fptr->size());
		if(fptr->tier_ptr() != &(*tptr)) tptr->enqueue_file_ptr(&(*fptr));
		++fptr;
	}
}

void TierEngine::move_files(void){
	/*  Currently, this starts at the lowest tier, assuming it has the most free space, and
	 * moves all incoming files from their current tiers before moving on to the next lowest
	 * tier. There should be a better way to shuffle all the files around to avoid over-filling
	 * a tier by doing them one at a time.
	 */
	Logging::log.message("Moving files.",2);
	for(std::list<Tier>::reverse_iterator titr = tiers_.rbegin(); titr != tiers_.rend(); titr++){
		// Maybe multithread this if fs::copy_file is thread-safe?
		titr->transfer_files();
	}
}

void TierEngine::sleep_until(std::chrono::steady_clock::time_point t){
	std::unique_lock<std::mutex> lk(sleep_mt_);
	sleep_cv_.wait_until(lk, t, [this](){ return this->stop_flag_ || !this->adhoc_work_.empty(); });
}
