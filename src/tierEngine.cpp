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

#include "version.hpp"
#include "tierEngine.hpp"
#include "alert.hpp"
#include "fusePassthrough.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <atomic>

extern "C" {
	#include <fcntl.h>
}

int TierEngine::lock_mutex(void){
	int result = open((run_path_ / "autotier.lock").c_str(), O_CREAT|O_EXCL, 0700);
	close(result);
	return result;
}

void TierEngine::unlock_mutex(void){
	fs::remove(run_path_ / "autotier.lock");
}

void TierEngine::open_db(bool read_only){
	std::string db_path = (run_path_ / "db").string();
	rocksdb::Options options;
	options.create_if_missing = true;
	rocksdb::Status status;
	if(read_only)
		status = rocksdb::DB::OpenForReadOnly(options, db_path, &db_);
	else
		status = rocksdb::DB::Open(options, db_path, &db_);
	if(!status.ok()){
		Logging::log.error("Failed to open RocksDB database: " + db_path);
	}
}

TierEngine::TierEngine(const fs::path &config_path, const ConfigOverrides &config_overrides, bool read_only)
		: stop_flag_(false), tiers_(), config_(config_path, std::ref(tiers_), config_overrides, read_only), run_path_(config_.run_path()){
	open_db(read_only);
}

TierEngine::~TierEngine(void){
	delete db_;
}

std::list<Tier> &TierEngine::get_tiers(void){
	return tiers_;
}

rocksdb::DB *TierEngine::get_db(void){
	return db_;
}

Tier *TierEngine::tier_lookup(fs::path p){
	for(std::list<Tier>::iterator t = tiers_.begin(); t != tiers_.end(); ++t){
		if(t->path() == p)
			return &(*t);
	}
	return nullptr;
}

Tier *TierEngine::tier_lookup(std::string id){
	for(std::list<Tier>::iterator t = tiers_.begin(); t != tiers_.end(); ++t){
		if(t->id() == id)
			return &(*t);
	}
	return nullptr;
}

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

#define TABLE_HEADER_LINE
#define UNIT_GAP 1
#define ABSW     7
#define ABSU     3 + UNIT_GAP
#define PERCENTW 6
#define PERCENTU 1 + UNIT_GAP

inline int find_max_width(const std::vector<std::string> &names){
	int res = -1;
	for(const std::string &name : names){
		int length = name.length();
		if(length > res) res = length;
	}
	return res;
}

void TierEngine::stop(void){
	std::lock_guard<std::mutex> lk(sleep_mt_);
	stop_flag_ = true;
	sleep_cv_.notify_one();
}

void TierEngine::process_adhoc_requests(void){
	std::vector<std::string> payload;
	while(!stop_flag_){
		get_fifo_payload(payload, run_path_ / "request.pipe");
		if(stop_flag_)
			return;
		AdHoc work(payload);
		payload.clear();
		switch(work.cmd_){
			case ONESHOT:
				process_oneshot(work);
				break;
			case PIN:
			case UNPIN:
				process_pin_unpin(work);
				break;
			case WHICHTIER:
				process_which_tier(work);
				break;
			case STATUS:
				process_status(work);
				break;
			default:
				Logging::log.warning("Received bad ad hoc command.");
				payload.clear();
				payload.emplace_back("ERR");
				payload.emplace_back("Not a command.");
				send_fifo_payload(payload, run_path_ / "response.pipe");
				break;
		}
		sleep_cv_.notify_one();
	}
}

void TierEngine::process_oneshot(const AdHoc &work){
	std::vector<std::string> payload;
	if(!work.args_.empty()){
		payload.emplace_back("ERR");
		std::string err_msg = "autotier oneshot takes no arguments. Offender(s):";
		for(const std::string &str : work.args_)
			err_msg += " " + str;
		payload.emplace_back(err_msg);
		send_fifo_payload(payload, run_path_ / "response.pipe");
		return;
	}
	adhoc_work_.push(work);
	payload.emplace_back("OK");
	payload.emplace_back("Work queued.");
	send_fifo_payload(payload, run_path_ / "response.pipe");
}

void TierEngine::process_pin_unpin(const AdHoc &work){
	std::vector<std::string> payload;
	std::vector<std::string>::const_iterator itr = work.args_.begin();
	if(work.cmd_ == PIN){
		std::string tier_id = work.args_.front();
		if(tier_lookup(tier_id) == nullptr){
			payload.emplace_back("ERR");
			payload.emplace_back("Tier does not exist: \"" + tier_id + "\"");
			send_fifo_payload(payload, run_path_ / "response.pipe");
			return;
		}
		++itr;
	}
	std::vector<std::string> not_in_fs;
	for( ; itr != work.args_.end(); ++itr){
		if(!std::equal(mount_point_.string().begin(), mount_point_.string().end(), itr->begin())){
			not_in_fs.push_back(*itr);
		}
	}
	if(!not_in_fs.empty()){
		payload.emplace_back("ERR");
		std::string err_msg = "Files are not in autotier filesystem:";
		for(const std::string &str : not_in_fs)
			err_msg += " " + str;
		payload.emplace_back(err_msg);
		send_fifo_payload(payload, run_path_ / "response.pipe");
		return;
	}
	adhoc_work_.push(work);
	payload.clear();
	payload.emplace_back("OK");
	payload.emplace_back("Work queued.");
	send_fifo_payload(payload, run_path_ / "response.pipe");
}

void TierEngine::process_which_tier(AdHoc &work){
	std::vector<std::string> payload;
	payload.emplace_back("OK");
	int namew = 0;
	int tierw = 0;
	std::vector<std::string> not_in_fs;
	for(std::string &arg : work.args_){
		if(std::equal(mount_point_.string().begin(), mount_point_.string().end(), arg.begin())){
			arg = fs::relative(arg, mount_point_).string();
		}
		int len = arg.length();
		if(len > namew)
			namew = len;
	}
	int file_header_len = std::string("File").length();
	if(file_header_len > namew)
		namew = file_header_len;
	for(const Tier &tier : tiers_){
		int len = tier.id().length() + 2; // 2 quotes
		if(len > tierw)
			tierw = len;
	}
	std::stringstream header;
	header << std::setw(namew) << std::left << "File" << "  ";
	header << std::setw(tierw) << std::left << "Tier" << "  ";
	header << std::left << "Backend Path";
#ifdef TABLE_HEADER_LINE
	header << std::endl;
	auto fill = header.fill();
	header << std::setw(80) << std::setfill('-') << "";
	header.fill(fill);
#endif
	payload.emplace_back(header.str());
	for(const std::string &arg : work.args_){
		std::stringstream record;
		record << std::setw(namew) << std::left << arg << "  ";
		Metadata f(arg.c_str(), db_);
		if(f.not_found()){
				record << "not found.";
		}else{
			Tier *tptr = tier_lookup(fs::path(f.tier_path()));
			if(tptr == nullptr)
				record << std::setw(tierw) << std::left << "UNK" << "  ";
			else
				record << std::setw(tierw) << std::left << "\"" + tptr->id() + "\"" << "  ";
			record << std::left << (fs::path(f.tier_path()) / arg).string();
		}
		payload.emplace_back(record.str());
	}
	send_fifo_payload(payload, run_path_ / "response.pipe");
}

void TierEngine::process_status(const AdHoc &work){
	uintmax_t total_capacity = 0;
	uintmax_t total_quota_capacity = 0;
	uintmax_t total_usage = 0;
	std::vector<std::string> payload;
	
	bool json;
	std::stringstream json_ss(work.args_.front());
	
	try{
		json_ss >> std::boolalpha >> json;
	}catch (const std::ios_base::failure &){
		Logging::log.error("Could not extract boolean from string.", false);
		payload.emplace_back("ERR");
		payload.emplace_back("Could not determine whether to use table or JSON output.");
		send_fifo_payload(payload, run_path_ / "response.pipe");
		return;
	}
	
	payload.emplace_back("OK");
	
	std::string unit("");
	for(const Tier &t : tiers_){
		total_capacity += t.capacity();
		total_quota_capacity += t.quota_bytes();
		total_usage += t.usage_bytes();
	}
	double overall_quota = (double)total_quota_capacity  * 100.0 / (double)total_capacity;
	double total_percent_usage = (double)total_usage * 100.0 / (double)total_capacity;
	std::stringstream ss;
	if(json){
		ss << 
		"{"
			"\"version\":\"" VERS "\","
			"\"combined\":{"
				"\"capacity\":" + std::to_string(total_capacity) + ","
				"\"capacity_pretty\":\"" + Logging::log.format_bytes(total_capacity) + "\","
				"\"quota\":" + std::to_string(total_quota_capacity) + ","
				"\"quota_pretty\":\"" + Logging::log.format_bytes(total_quota_capacity) + "\","
				"\"usage\":" + std::to_string(total_usage) + ","
				"\"usage_pretty\":\"" + Logging::log.format_bytes(total_usage) + "\""
			"},"
			"\"tiers\":[";
		for(std::list<Tier>::iterator tptr = tiers_.begin(); tptr != tiers_.end(); ++tptr){
			ss <<
			"{"
				"\"name\":\"" + tptr->id() + "\","
				"\"capacity\":" + std::to_string(tptr->capacity()) + ","
				"\"capacity_pretty\":\"" + Logging::log.format_bytes(tptr->capacity()) + "\","
				"\"quota\":" + std::to_string(tptr->quota_bytes()) + ","
				"\"quota_pretty\":\"" + Logging::log.format_bytes(tptr->quota_bytes()) + "\","
				"\"usage\":" + std::to_string(tptr->usage_bytes()) + ","
				"\"usage_pretty\":\"" + Logging::log.format_bytes(tptr->usage_bytes()) + "\","
				"\"path\":\"" + tptr->path().string() + "\""
			"}";
			if(std::next(tptr) != tiers_.end())
				ss << ",";
		}
		ss <<
			"]"
		"}";
	}else{
		std::vector<std::string> names;
		names.push_back("combined");
		for(std::list<Tier>::iterator tptr = tiers_.begin(); tptr != tiers_.end(); ++tptr){
			names.push_back(tptr->id());
		}
		int namew = find_max_width(names);
		{
			// Header
			ss << std::setw(namew) << std::left << "Tier";
			ss << " ";
			ss << std::setw(ABSW) << std::right << "Size";
			ss << std::setw(ABSU) << ""; // unit
			ss << " ";
			ss << std::setw(ABSW) << std::right << "Quota";
			ss << std::setw(ABSU) << ""; // unit
			ss << " ";
			ss << std::setw(PERCENTW) << std::right << "Quota";
			ss << std::setw(PERCENTU) << "%"; // unit
			ss << " ";
			ss << std::setw(ABSW) << std::right << "Use";
			ss << std::setw(ABSU) << ""; // unit
			ss << " ";
			ss << std::setw(PERCENTW) << std::right << "Use";
			ss << std::setw(PERCENTU) << "%"; // unit
			ss << " ";
			ss << std::left << "Path";
	#ifdef TABLE_HEADER_LINE
			ss << std::endl;
			auto fill = ss.fill();
			ss << std::setw(80) << std::setfill('-') << "";
			ss.fill(fill);
	#endif
			ss << std::endl;
		}
		{
			// Combined
			ss << std::setw(namew) << std::left << "combined"; // tier
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right << Logging::log.format_bytes(total_capacity, unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right << Logging::log.format_bytes(total_quota_capacity, unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(PERCENTW) << std::right << overall_quota;
			ss << std::setw(PERCENTU) << "%"; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right << Logging::log.format_bytes(total_usage, unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(PERCENTW) << std::right << total_percent_usage;
			ss << std::setw(PERCENTU) << "%"; // unit
			ss << std::endl;
		}
		for(std::list<Tier>::iterator tptr = tiers_.begin(); tptr != tiers_.end(); ++tptr){
			// Tiers
			ss << std::setw(namew) << std::left << tptr->id(); // tier
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right << Logging::log.format_bytes(tptr->capacity(), unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right << Logging::log.format_bytes(tptr->quota_bytes(), unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(PERCENTW) << std::right << tptr->quota_percent();
			ss << std::setw(PERCENTU) << "%"; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right << Logging::log.format_bytes(tptr->usage_bytes(), unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(PERCENTW) << std::right << tptr->usage_percent();
			ss << std::setw(PERCENTU) << "%"; // unit
			ss << " ";
			ss << std::left << tptr->path().string();
			ss << std::endl;
		}
	}
	std::string line;
	while(getline(ss, line)){
		payload.emplace_back(line);
	}
	send_fifo_payload(payload, run_path_ / "response.pipe");
}

void TierEngine::execute_queued_work(void){
	while(!adhoc_work_.empty()){
		AdHoc work = adhoc_work_.pop();
		switch(work.cmd_){
			case ONESHOT:
				tier(std::chrono::seconds(-1)); // don't affect period
				break;
			case PIN:
				pin_files(work.args_);
				break;
			case UNPIN:
				unpin_files(work.args_);
				break;
			default:
				Logging::log.warning("Trying to execute bad ad hoc command.");
				break;
		}
	}
}

void TierEngine::pin_files(const std::vector<std::string> &args){
	Tier *tptr;
	std::string tier_id = args.front();
	if((tptr = tier_lookup(tier_id)) == nullptr){
		Logging::log.warning("Tier does not exist, cannot pin files. Tier name given: " + tier_id);
		return;
	}
	for(std::vector<std::string>::const_iterator fptr = std::next(args.begin()); fptr != args.end(); ++fptr){
		fs::path mounted_path = *fptr;
		fs::path relative_path = fs::relative(mounted_path, mount_point_);
		Metadata f(relative_path.c_str(), db_);
		if(f.not_found()){
			Logging::log.warning("File to be pinned was not in database: " + mounted_path.string());
			continue;
		}
		Logging::log.message("old tier path: " + f.tier_path(), 0);
		Tier *old_tptr = tier_lookup(fs::path(f.tier_path()));
		File file(old_tptr->path() / relative_path, db_, old_tptr);
		Logging::log.message("old full path: " + file.full_path().string(), 0);
		file.pin();
		tptr->enqueue_file_ptr(&file);
	}
	tptr->transfer_files();
}

void TierEngine::unpin_files(const std::vector<std::string> &args){
	for(const std::string &mounted_path : args){
		fs::path relative_path = fs::relative(mounted_path, mount_point_);
		Metadata f(relative_path.c_str(), db_);
		if(f.not_found()){
			Logging::log.warning("File to be unpinned was not in database: " + mounted_path);
			continue;
		}
		f.pinned(false);
		f.update(relative_path.c_str(), db_);
	}
}

void TierEngine::mount_point(const fs::path &mount_point){
	mount_point_ = mount_point;
}
