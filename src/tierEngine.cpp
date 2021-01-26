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
#include "tier.hpp"
#include "file.hpp"
#include "alert.hpp"
#include "fusePassthrough.hpp"

#include <chrono>
#include <thread>
#include <regex>
#include <fcntl.h>
#include <sys/xattr.h>
#include <signal.h>
#include <sstream>
#include <cmath>

sig_atomic_t stopFlag = false;

void int_handler(int){
	stopFlag = true;
}

TierEngine::TierEngine(const fs::path &config_path)
		: tiers(), config_(config_path, std::ref(tiers)){
	signal(SIGINT, &int_handler);
	signal(SIGTERM, &int_handler);
	pick_run_path(config_path);
	open_db();
}

void TierEngine::pick_run_path(const fs::path &config_path){
	run_path_ = "/run/autotier";
	if(!fs::is_directory(run_path_)){
		try{
			fs::create_directories(run_path_);
		}catch(const fs::filesystem_error &){
			char *home = getenv("HOME");
			if(home == NULL){
				Logging::log.error("$HOME not exported.");
			}
			run_path_ = fs::path(home) / ".local/run/autotier";
			fs::create_directories(run_path_);
		}
	}else if(access(run_path_.c_str(), R_OK | W_OK) != 0){
		char *home = getenv("HOME");
		if(home == NULL){
			Logging::log.error("$HOME not exported.");
		}
		run_path_ = fs::path(home) / ".local/run/autotier";
		fs::create_directories(run_path_);
	}
	run_path_ /= std::to_string(std::hash<std::string>{}(config_path.string()));
	fs::create_directory(run_path_);
}

TierEngine::~TierEngine(){
	delete db_;
}

std::list<Tier> &TierEngine::get_tiers(void){
	return tiers;
}

rocksdb::DB *TierEngine::get_db(void){
	return db_;
}

void TierEngine::open_db(){
	std::string db_path = (run_path_ / "db").string();
	rocksdb::Options options;
	options.create_if_missing = true;
	rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db_);
	if(!status.ok()){
		Logging::log.error("Failed to open RocksDB database: " + db_path);
	}
}

int TierEngine::lock_mutex(void){
	int result = open((run_path_ / "autotier.lock").c_str(), O_CREAT|O_EXCL);
	close(result);
	return result;
}

void TierEngine::unlock_mutex(void){
	fs::remove(run_path_ / "autotier.lock");
}

Tier *TierEngine::tier_lookup(fs::path p){
	for(std::list<Tier>::iterator t = tiers.begin(); t != tiers.end(); ++t){
		if(t->path() == p)
			return &(*t);
	}
	return nullptr;
}

Tier *TierEngine::tier_lookup(std::string id){
	for(std::list<Tier>::iterator t = tiers.begin(); t != tiers.end(); ++t){
		if(t->id() == id)
			return &(*t);
	}
	return nullptr;
}

Config *TierEngine::get_config(void){
	return &config_;
}

void TierEngine::begin(bool daemon_mode){
	Logging::log.message("autotier started.", 1);
	do{
		auto start = std::chrono::system_clock::now();
		launch_crawlers(&TierEngine::emplace_file);
		// one popularity calculation per loop
		calc_popularity();
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
		files.clear();
		auto end = std::chrono::system_clock::now();
		auto duration = end - start;
		// don't wait for oneshot execution
		if(daemon_mode && duration < config_.tier_period_s())
			std::this_thread::sleep_for(config_.tier_period_s()-duration);
	}while(daemon_mode && !stopFlag);
}

void TierEngine::launch_crawlers(void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr)){
	Logging::log.message("Gathering files.", 2);
	// get ordered list of files in each tier
	for(std::list<Tier>::iterator t = tiers.begin(); t != tiers.end(); ++t){
		crawl(t->path(), &(*t), function);
	}
}

void TierEngine::crawl(fs::path dir, Tier *tptr, void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr)){
	for(fs::directory_iterator itr{dir}; itr != fs::directory_iterator{}; *itr++){
		if(is_directory(*itr)){
			crawl(*itr, tptr, function);
		}else if(!is_symlink(*itr)){
			(this->*function)(*itr, tptr);
		}
	}
}

void TierEngine::emplace_file(fs::directory_entry &file, Tier *tptr){
	files.emplace_back(file.path(), db_, tptr);
	if(files.back().is_pinned()){
		tptr->add_file_size(files.back().size());
		files.pop_back();
	}
}

void TierEngine::print_file_pins(){
	for(std::list<File>::iterator fptr = files.begin(); fptr != files.end(); ++fptr){
		if(fptr->is_pinned()){
			Logging::log.message(fptr->relative_path().string() + " is pinned to " + fptr->tier_ptr()->id() , 1);
		}
	}
	files.clear();
}

void TierEngine::print_file_popularity(){
	for(std::list<File>::iterator f = files.begin(); f != files.end(); ++f){
		Logging::log.message(f->relative_path().string() + " popularity: " + std::to_string(f->popularity()), 1);
	}
	files.clear();
}

void TierEngine::sort(){
	Logging::log.message("Sorting files.", 2);
	files.sort(
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

void TierEngine::simulate_tier(){
	Logging::log.message("Finding files' tiers.",2);
	std::list<File>::iterator fptr = files.begin();
	std::list<Tier>::iterator tptr = tiers.begin();
	while(fptr != files.end()){
		if(tptr->full_test(*fptr)){
			if(std::next(tptr) != tiers.end()){
				// move to next tier
				++tptr;
			} // else: out of space!
		}
		tptr->add_file_size(fptr->size());
		if(fptr->tier_ptr() != &(*tptr)) tptr->enqueue_file_ptr(&(*fptr));
		++fptr;
	}
}

void TierEngine::move_files(){
	/*  Currently, this starts at the lowest tier, assuming it has the most free space, and
	 * moves all incoming files from their current tiers before moving on to the next lowest
	 * tier. There should be a better way to shuffle all the files around to avoid over-filling
	 * a tier by doing them one at a time.
	 */
	Logging::log.message("Moving files.",2);
	for(std::list<Tier>::reverse_iterator titr = tiers.rbegin(); titr != tiers.rend(); titr++){
		titr->transfer_files();
	}
}

void TierEngine::print_tier_info(void){
	int i = 1;
	
	Logging::log.message("Tiers from fastest to slowest:", 1);
	Logging::log.message("", 1);
	for(std::list<Tier>::iterator tptr = tiers.begin(); tptr != tiers.end(); ++tptr){
		std::stringstream ss;
		ss <<
		"Tier " << i++ << ":" << std::endl <<
		"tier name: \"" << tptr->id() << "\"" << std::endl <<
		"tier path: " << tptr->path().string() << std::endl <<
		"current usage: " << tptr->usage_percent() << "% (" << Logging::log.format_bytes(tptr->usage_bytes()) << ")" << std::endl <<
		"watermark: " << tptr->watermark() << "% (" << Logging::log.format_bytes(tptr->watermark_bytes()) << ")" << std::endl <<
		std::endl;
		Logging::log.message(ss.str(), 1);
	}
}

void TierEngine::pin_files(std::string tier_name, std::vector<fs::path> &files_){
	Tier *tptr;
	Logging::log.message("Tier name: " + tier_name, 2);
	if((tptr = tier_lookup(tier_name)) == nullptr){
		Logging::log.error("Tier does not exist.");
	}
	for(std::vector<fs::path>::iterator fptr = files_.begin(); fptr != files_.end(); ++fptr){
		File f(*fptr, db_);
		if(!exists(f.full_path())){
			Logging::log.warning("File does not exist! " + fptr->string());
			continue;
		}
		if(f.full_path() != tptr->path() / f.relative_path()){
			tptr->enqueue_file_ptr(&f);
			tptr->transfer_files();
		}
		f.pin();
	}
}

void TierEngine::unpin(int optind, int argc, char *argv[]){
	// argv = {"unpin", file(s), ...}
	while(optind < argc){
		fs::path temp(argv[optind++]);
		File f(temp, db_);
		if(!exists(f.full_path())){
			Logging::log.warning("File does not exist! " + f.relative_path().string());
			continue;
		}
		f.unpin();
	}
}

void TierEngine::calc_popularity(void){
	Logging::log.message("Calculating file popularity.", 2);
	double tier_period_s = config_.tier_period_s().count();
	for(std::list<File>::iterator f = files.begin(); f != files.end(); ++f){
		f->calc_popularity(tier_period_s);
	}
}
