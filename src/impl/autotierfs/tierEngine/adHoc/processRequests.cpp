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
#include "version.hpp"
#include <sstream>

void TierEngine::process_adhoc_requests(void){
	std::vector<std::string> payload;
	while(!stop_flag_){
		try{
			get_fifo_payload(payload, run_path_ / "request.pipe");
		}catch(const fifo_exception &err){
			Logging::log.warning(err.what());
			continue;
		}
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
			case LPOP:
				process_list_popularity();
				break;
			case LPIN:
				process_list_pins();
				break;
			default:
				Logging::log.warning("Received bad ad hoc command.");
				payload.clear();
				payload.emplace_back("ERR");
				payload.emplace_back("Not a command.");
				try{
					send_fifo_payload(payload, run_path_ / "response.pipe");
				}catch(const fifo_exception &err){
					Logging::log.warning(err.what());
					// let it notify main tier thread
				}
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
	try{
		send_fifo_payload(payload, run_path_ / "response.pipe");
	}catch(const fifo_exception &err){
		Logging::log.warning(err.what());
	}
}

void TierEngine::process_pin_unpin(const AdHoc &work){
	std::vector<std::string> payload;
	std::vector<std::string>::const_iterator itr = work.args_.begin();
	if(work.cmd_ == PIN){
		std::string tier_id = work.args_.front();
		if(tier_lookup(tier_id) == nullptr){
			payload.emplace_back("ERR");
			payload.emplace_back("Tier does not exist: \"" + tier_id + "\"");
			try{
				send_fifo_payload(payload, run_path_ / "response.pipe");
			}catch(const fifo_exception &err){
				Logging::log.warning(err.what());
			}
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
		try{
			send_fifo_payload(payload, run_path_ / "response.pipe");
		}catch(const fifo_exception &err){
			Logging::log.warning(err.what());
		}
		return;
	}
	adhoc_work_.push(work);
	payload.clear();
	payload.emplace_back("OK");
	payload.emplace_back("Work queued.");
	try{
		send_fifo_payload(payload, run_path_ / "response.pipe");
	}catch(const fifo_exception &err){
		Logging::log.warning(err.what());
	}
}

#define TABLE_HEADER_LINE
#define UNIT_GAP 1
#define ABSW     7
#define ABSU     3 + UNIT_GAP
#define PERCENTW 6
#define PERCENTU 1 + UNIT_GAP

namespace l{
	inline int find_max_width(const std::vector<std::string> &names){
		int res = -1;
		for(const std::string &name : names){
			int length = name.length();
			if(length > res) res = length;
		}
		return res;
	}
}

void TierEngine::process_which_tier(AdHoc &work){
	std::vector<std::string> payload;
	payload.emplace_back("OK");
	int namew = 0;
	int tierw = 0;
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
	try{
		send_fifo_payload(payload, run_path_ / "response.pipe");
	}catch(const fifo_exception &err){
		Logging::log.warning(err.what());
	}
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
		Logging::log.error("Could not extract boolean from string.");
		payload.emplace_back("ERR");
		payload.emplace_back("Could not determine whether to use table or JSON output.");
		try{
			send_fifo_payload(payload, run_path_ / "response.pipe");
		}catch(const fifo_exception &err){
			Logging::log.warning(err.what());
		}
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
		int namew = l::find_max_width(names);
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
	try{
		send_fifo_payload(payload, run_path_ / "response.pipe");
	}catch(const fifo_exception &err){
		Logging::log.warning(err.what());
	}
}

void TierEngine::process_list_popularity(void){
	std::vector<std::string> payload;
	payload.emplace_back("OK");
	std::stringstream ss;
	ss << "File : Popularity (accesses per hour)" << std::endl;
	rocksdb::Iterator *it = db_->NewIterator(rocksdb::ReadOptions());
	for(it->SeekToFirst(); it->Valid(); it->Next()){
		Metadata f(it->value().ToString());
		ss << it->key().ToString() << " : " << f.popularity() << std::endl;
	}
	payload.emplace_back(ss.str());
	try{
		send_fifo_payload(payload, run_path_ / "response.pipe");
	}catch(const fifo_exception &err){
		Logging::log.warning(err.what());
	}
}

void TierEngine::process_list_pins(void){
	std::vector<std::string> payload;
	payload.emplace_back("OK");
	std::stringstream ss;
	ss << "File : Tier Path" << std::endl;
	rocksdb::Iterator *it = db_->NewIterator(rocksdb::ReadOptions());
	for(it->SeekToFirst(); it->Valid(); it->Next()){
		Metadata f(it->value().ToString());
		if(f.pinned())
			ss << it->key().ToString() << " : " << f.tier_path() << std::endl;
	}
	payload.emplace_back(ss.str());
	try{
		send_fifo_payload(payload, run_path_ / "response.pipe");
	}catch(const fifo_exception &err){
		Logging::log.warning(err.what());
	}
}
