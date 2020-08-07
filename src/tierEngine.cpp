/*
		Copyright (C) 2019-2020 Joshua Boudreau <jboudreau@45drives.com>
		
		This file is part of autotier.

		autotier is free software: you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation, either version 3 of the License, or
		(at your option) any later version.

		autotier is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with autotier.	If not, see <https://www.gnu.org/licenses/>.
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

std::string RUN_PATH = "/run/autotier";

sig_atomic_t stopFlag = false;

void int_handler(int){
	stopFlag = true;
}

TierEngine::TierEngine(const fs::path &config_path){
	signal(SIGINT, &int_handler);
	signal(SIGTERM, &int_handler);
	config.load(config_path, tiers, cache, hasCache);
	//tiers_ptr = &tiers;
	log_lvl = config.log_lvl;
	while(!is_directory(fs::path(RUN_PATH))){
    try{
      create_directories(fs::path(RUN_PATH));
    }catch(boost::filesystem::filesystem_error){
      char *home = getenv("HOME");
      if(home == NULL){
        error(CREATE_RUNPATH);
        exit(1);
      }
      RUN_PATH.assign(std::string(home) + "/.local/run/autotier");
    }
  }
	mutex_path = fs::path(RUN_PATH) / get_mutex_name(config_path);
	open_db();
}

TierEngine::~TierEngine(){
	sqlite3_close(db);
}

fs::path TierEngine::get_mountpoint(void){
	return config.mountpoint;
}

std::list<Tier> &TierEngine::get_tiers(void){
	return tiers;
}

void TierEngine::open_db(){
	int res = sqlite3_open((RUN_PATH + "/db.sqlite").c_str(), &db);
	char *errMsg = 0;
	if(res){
		std::cerr << "Error opening database: " << sqlite3_errmsg(db);
		exit(res);
	}else{
		Log("Opened database successfully", 2);
	}
	
	sqlite3_busy_timeout(db, 20);
	
	const char *sql =
	"CREATE TABLE IF NOT EXISTS Files("
	"	ID INT PRIMARY KEY NOT NULL,"
	"	RELATIVE_PATH TEXT NOT NULL,"
	"	CURRENT_TIER TEXT,"
	"	PIN TEXT,"
	"	POPULARITY REAL,"
	"	LAST_ACCESS INT"
	");";
		
	res = sqlite3_exec(db, sql, NULL, 0, &errMsg);
	
	if(res){
		std::cerr << "Error creating table: " << sqlite3_errmsg(db);
		exit(res);
	}
}

fs::path TierEngine::get_mutex_name(const fs::path &config_path){
	return fs::path(std::to_string(std::hash<std::string>{}(config_path.string())) + ".autotier.lock");
}

int TierEngine::lock_mutex(void){
	if(!is_directory(mutex_path.parent_path())){
		create_directories(mutex_path.parent_path());
	}
	int result = open(mutex_path.c_str(), O_CREAT|O_EXCL);
	close(result);
	return result;
}

void TierEngine::unlock_mutex(void){
	remove(mutex_path);
}

Tier *TierEngine::tier_lookup(fs::path p){
  for(std::list<Tier>::iterator t = tiers.begin(); t != tiers.end(); ++t){
    if(t->dir == p)
      return &(*t);
  }
}

void TierEngine::begin(bool daemon_mode){
	Log("autotier started.",1);
	unsigned int timer = 0;
	do{
		auto start = std::chrono::system_clock::now();
		launch_crawlers(&TierEngine::emplace_file);
		// one popularity calculation per loop
		calc_popularity();
		if(timer == 0){
			// mutex lock
			if(lock_mutex() == ERR){
				Log("autotier already moving files.",0);
			}else{
				// mutex locked
				// one tier execution per tier period
				sort();
				simulate_tier();
				move_files();
				
				Log("Tiering complete.",1);
				unlock_mutex();
			}
		}
		//files.erase(files.begin(), files.end());
		files.clear();
		for(std::list<Tier>::iterator t = tiers.begin(); t != tiers.end(); ++t){
			t->cleanup();
		}
		auto end = std::chrono::system_clock::now();
		auto duration = end - start;
		// don't wait for oneshot execution
		if(daemon_mode && duration < std::chrono::seconds(CALC_PERIOD))
			std::this_thread::sleep_for(std::chrono::seconds(CALC_PERIOD)-duration);
		timer = (++timer) % (config.period / CALC_PERIOD);
	}while(daemon_mode && !stopFlag);
}

void TierEngine::launch_crawlers(void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr)){
	Log("Gathering files.",2);
	// get ordered list of files in each tier
	for(std::list<Tier>::iterator t = tiers.begin(); t != tiers.end(); ++t){
    t->pinned_files_size = 0;
		crawl(t->dir, &(*t), function);
	}
}

void TierEngine::crawl(fs::path dir, Tier *tptr, void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr)){
	for(fs::directory_iterator itr{dir}; itr != fs::directory_iterator{}; *itr++){
		if(is_directory(*itr)){
			crawl(*itr, tptr, function);
		}else if(!is_symlink(*itr) &&
		!regex_match((*itr).path().filename().string(), std::regex("(^\\..*(\\.swp)$|^(\\.~lock\\.).*#$|^(~\\$))"))){
			(this->*function)(*itr, tptr);
		}
	}
}

void TierEngine::emplace_file(fs::directory_entry &file, Tier *tptr){
	files.emplace_back(fs::relative(file, tptr->dir), tptr, db);
  if(!files.back().pinned_to.empty()){
    tier_lookup(files.back().pinned_to)->pinned_files_size += files.back().size;
  }
	if(hasCache){
		File *fptr = &files.back();
		fptr->cache_path = cache->dir/relative(fptr->old_path, fptr->old_tier->dir);
	}
}

void TierEngine::print_file_pins(){
	for(std::list<File>::iterator fptr = files.begin(); fptr != files.end(); ++fptr){
		if(fptr->pinned_to.empty())
			continue;
		std::cout << fptr->old_path.string() << std::endl;
		std::cout << "pinned to" << std::endl;
		std::cout << fptr->pinned_to.string() << std::endl;
	}
	files.clear();
}

void TierEngine::print_file_popularity(){
	for(std::list<File>::iterator f = files.begin(); f != files.end(); ++f){
		std::cout << f->old_path.string() << " popularity: " << f->popularity << std::endl;
	}
	files.clear();
}

void TierEngine::sort(){
	Log("Sorting files.",2);
	files.sort(
		[](const File &a, const File &b){
			return (a.popularity == b.popularity)? a.times.actime > b.times.actime : a.popularity > b.popularity;
		}
	);
}

void TierEngine::simulate_tier(){
	Log("Finding files' tiers.",2);
	long tier_use = 0;
	std::list<File>::iterator fptr = files.begin();
	std::list<Tier>::iterator tptr = tiers.begin();
	tptr->watermark_bytes = tptr->get_capacity() * tptr->watermark / 100;
	while(fptr != files.end()){
		if(tier_use + fptr->size >= tptr->watermark_bytes){
			// move to next tier
			tier_use = 0;
			if(++tptr == tiers.end()) break;
			tptr->watermark_bytes = tptr->get_capacity() * tptr->watermark / 100;
		}
		tier_use += fptr->size;
		tptr->incoming_files.emplace_back(&(*fptr));
		fptr++;
	}
	tier_use = 0;
	fptr = files.begin();
	if(hasCache){
		cache->watermark_bytes = cache->get_capacity() * cache->watermark / 100;
		while(fptr != files.end() && (tier_use + fptr->size < cache->watermark_bytes)){
			tier_use += fptr->size;
			cache->incoming_files.emplace_back(&(*fptr));
			fptr++;
		}
	}
}

void TierEngine::move_files(){
	/*
	 * Currently, this starts at the lowest tier, assuming it has the most free space, and
	 * moves all incoming files from their current tiers before moving on to the next lowest
	 * tier. There should be a better way to shuffle all the files around to avoid over-filling
	 * a tier by doing them one at a time.
	 */
	Log("Moving files.",2);
	for(std::list<Tier>::reverse_iterator titr = tiers.rbegin(); titr != tiers.rend(); titr++){
		for(File * fptr : titr->incoming_files){
			if(!fptr->pinned_to.empty())
				fptr->new_path = fptr->pinned_to;
			else
				fptr->new_path = titr->dir;
			fptr->new_path /= relative(fptr->old_path, fptr->old_tier->dir);
			/*
			 * TODO: handle cases where file already exists at destination (should not happen but could)
			 * 1 - Check if new_path exists.
			 * 2 - Hash both old_path and new_path to check if they are the same file.
			 * 2a- If old_hash == new_hash, remove old_path and create symlink if needed.
			 * 2b- If old_hash != new_hash, rename the new_path file with new_path plus something to make it unique. 
			 *		 Be sure to check if new name doesnt exist before moving the file.
			 */
			fptr->move();
		}
	}
	if(hasCache){
		for(std::list<File>::iterator fptr = files.begin(); fptr != files.end(); ++fptr){
			fptr->uncache();
		}
		for(File * fptr : cache->incoming_files){
			fptr->cache();
		}
	}
}

void TierEngine::print_tier_info(void){
	int i = 1;
	
	std::cout << "Tiers from fastest to slowest:" << std::endl;
	std::cout << std::endl;
	for(std::list<Tier>::iterator tptr = tiers.begin(); tptr != tiers.end(); ++tptr){
		std::cout <<
		"Tier " << i++ << ":" << std::endl <<
		"tier name: \"" << tptr->id << "\"" << std::endl <<
		"tier path: " << tptr->dir.string() << std::endl <<
		"current usage: " << tptr->get_usage() * 100 / tptr->get_capacity() << "% (" << tptr->get_usage() << " bytes)" << std::endl <<
		"watermark: " << tptr->watermark << "% (" << tptr->get_capacity() * tptr->watermark / 100 << " bytes)" << std::endl <<
		std::endl;
	}
}

void TierEngine::pin_files(std::string tier_name, std::vector<fs::path> &files_){
	std::list<Tier>::iterator tptr_pin;
	for(tptr_pin = tiers.begin(); tptr_pin != tiers.end(); ++tptr_pin){
		if(tier_name == tptr_pin->id)
			break;
	}
	if(tptr_pin == tiers.end()){
		Log("Tier does not exist.",0);
		exit(1);
	}
	for(std::vector<fs::path>::iterator fptr = files_.begin(); fptr != files_.end(); ++fptr){
		File f(*fptr, db);
		if(!exists(f.current_tier / f.rel_path)){
			Log("File does not exist! " + fptr->string(),0);
			continue;
		}
		f.pinned_to = tptr_pin->dir;
	}
}

void TierEngine::unpin(int argc, char *argv[]){
	for(int i = 2; i < argc; i++){
		fs::path temp(argv[i]);
		File f(temp, db);
		if(!exists(f.current_tier / f.rel_path)){
			Log("File does not exist! " + f.rel_path.string(), 0);
			continue;
		}
		f.pinned_to = fs::path();
	}
}

void TierEngine::calc_popularity(){
	Log("Calculating file popularity.",2);
	for(std::list<File>::iterator f = files.begin(); f != files.end(); ++f){
		f->calc_popularity();
	}
}
