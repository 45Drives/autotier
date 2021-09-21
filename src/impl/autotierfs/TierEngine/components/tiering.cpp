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

#include "TierEngine/components/tiering.hpp"
#include "alert.hpp"
#include "file.hpp"

#include <regex>
#include <thread>

#if __cplusplus >= 201703L
	#include <execution>
#endif

TierEngineTiering::TierEngineTiering(const fs::path &config_path, const ConfigOverrides &config_overrides)
    : TierEngineDatabase(config_path, config_overrides)
    , TierEngineSleep(config_path, config_overrides)
    , TierEngineAdhoc(config_path, config_overrides)
	, TierEngineMutex(config_path, config_overrides)
	, TierEngineBase(config_path, config_overrides) {

}

TierEngineTiering::~TierEngineTiering() {

}

void TierEngineTiering::begin(bool daemon_mode) {
	Logging::log.message("autotier started.", Logger::log_level_t::NORMAL);
	if(config_.tier_period_s() < std::chrono::seconds(0)){
		last_tier_time_ = std::chrono::steady_clock::now();
		while(daemon_mode && !stop_flag_){
			execute_queued_work();
			sleep_until_woken();
		}
	}else{
		last_tier_time_ = std::chrono::steady_clock::now() - config_.tier_period_s();
		do{
			auto wake_time = std::chrono::steady_clock::now() + config_.tier_period_s();
			tier();
			while(daemon_mode && std::chrono::steady_clock::now() < wake_time && !stop_flag_){
				execute_queued_work();
				sleep_until(wake_time);
			}
		}while(daemon_mode && !stop_flag_);
	}
}

bool TierEngineTiering::tier(void) {
	{
		std::unique_lock<std::mutex> lk(lock_file_mt_, std::try_to_lock);
		if(!lk.owns_lock() || lock_mutex() == -1){
			Logging::log.warning("autotier already moving files.");
			return false;
		}
		currently_tiering_ = true;
		launch_crawlers(&TierEngineTiering::emplace_file);
		// one popularity calculation per loop
		calc_popularity();
		// mutex locked
		sort();
		simulate_tier();
		move_files();
		Logging::log.message("Tiering complete.", Logger::log_level_t::DEBUG);
		files_.clear();
		currently_tiering_ = false;
		unlock_mutex();
	}
	return true;
}

void TierEngineTiering::launch_crawlers(
		void (TierEngineTiering::*function)(
			fs::directory_entry &itr, Tier *tptr, std::atomic<ffd::Bytes::bytes_type> &usage
		)
	) {
	Logging::log.message("Gathering files.", Logger::log_level_t::DEBUG);
	// get ordered list of files in each tier
	for(std::list<Tier>::iterator t = tiers_.begin(); t != tiers_.end(); ++t){
		std::atomic<ffd::Bytes::bytes_type> usage(0);
		crawl(t->path(), &(*t), function, usage);
		t->usage(ffd::Bytes{usage});
	}
}

void TierEngineTiering::crawl(
		fs::path dir, Tier *tptr,
		void (TierEngineTiering::*function)(
			fs::directory_entry &itr, Tier *tptr, std::atomic<ffd::Bytes::bytes_type> &usage
		),
		std::atomic<ffd::Bytes::bytes_type> &usage
	) {
	// TODO: Replace this with multithreaded BFS
	std::regex temp_file_re("\\.[^/]*\\.autotier\\.hide$");
	for(fs::directory_iterator itr{dir}; itr != fs::directory_iterator{}; *itr++){
		fs::file_status status = fs::symlink_status(*itr);
		if(fs::is_directory(status)){
			crawl(*itr, tptr, function, usage);
		}else if(!is_symlink(status) && !std::regex_match(itr->path().string(), temp_file_re)){
			(this->*function)(*itr, tptr, usage);
		}
	}
}

void TierEngineTiering::emplace_file(
		fs::directory_entry &file, Tier *tptr, std::atomic<ffd::Bytes::bytes_type> &usage
	) {
	files_.emplace_back(file.path(), db_, tptr);
	ffd::Bytes size = files_.back().size();
	usage += size.get();
	if(files_.back().is_pinned()){
		files_.pop_back();
	}else{
		tptr->subtract_file_size_sim(size); // remove file size from current usage
	}
}

void TierEngineTiering::calc_popularity(void) {
	Logging::log.message("Calculating file popularity.", Logger::log_level_t::DEBUG);
	auto tier_time = std::chrono::steady_clock::now();
	std::chrono::steady_clock::duration period = tier_time - last_tier_time_;
	last_tier_time_ = tier_time;
	double period_d = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1,1>>>(period).count();
	Logging::log.message("Real period for popularity calc: " + std::to_string(period_d), Logger::log_level_t::DEBUG);
	for(std::vector<File>::iterator f = files_.begin(); f != files_.end(); ++f){
		f->calc_popularity(period_d);
	}
}

void TierEngineTiering::sort(void) {
	Logging::log.message("Sorting files.", Logger::log_level_t::DEBUG);
	std::sort(
#if __cplusplus >= 201703L
		std::execution::par,
#endif
		files_.begin(), files_.end(),
		[](const File &a, const File &b){
			double a_pop = a.popularity();
			double b_pop = b.popularity();
			if(a_pop == b_pop){
				struct timeval a_t = a.atime();
				struct timeval b_t = b.atime();
				if(a_t.tv_sec == b_t.tv_sec)
					return a_t.tv_usec > b_t.tv_usec;
				return a_t.tv_sec > b_t.tv_sec;
			}
			return a_pop > b_pop;
		}
	);
}

void TierEngineTiering::simulate_tier(void) {
	Logging::log.message("Finding files' tiers.", Logger::log_level_t::DEBUG);
	for(Tier &t : tiers_)
		t.reset_sim();
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

void TierEngineTiering::move_files(void) {
	std::vector<std::thread> threads;
	Logging::log.message("Moving files.",Logger::log_level_t::DEBUG);
	for(std::list<Tier>::iterator titr = tiers_.begin(); titr != tiers_.end(); ++titr){
		threads.emplace_back(&Tier::transfer_files, titr, config_.copy_buff_sz(), run_path_);
	}
	for(auto &thread : threads){
		thread.join();
	}
}

void TierEngineTiering::stop(void) {
	std::lock_guard<std::mutex> lk(sleep_mt_);
	stop_flag_ = true;
	sleep_cv_.notify_one();
	shutdown_socket_server();
}

bool TierEngineTiering::currently_tiering(void) const {
	return currently_tiering_;
}

bool TierEngineTiering::strict_period(void) const {
	return config_.strict_period();
}

void TierEngineTiering::exit(int status) {
	Logging::log.message("Ensuring mutex is unlocked before exiting.", Logger::log_level_t::DEBUG);
	unlock_mutex();
	stop();
	::exit(status);
}
