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

#include "TierEngine/components/adhoc.hpp"

#include "alert.hpp"
#include "conflicts.hpp"
#include "metadata.hpp"
#include "version.hpp"

#include <iomanip>
#include <sstream>

extern "C" {
#include <grp.h>
#include <sys/stat.h>
#include <sys/time.h>
}

TierEngineAdhoc::TierEngineAdhoc(const fs::path &config_path,
								 const ConfigOverrides &config_overrides) try
	: TierEngineBase(config_path, config_overrides)
	, oneshot_in_queue_(false)
	, socket_server_((run_path_ / "adhoc.socket").string()) {
	set_socket_permissions();
} catch (const ffd::SocketException &e) {
	Logging::log.error(std::string("Error while constructing socket server: ") + e.what());
	exit(EXIT_FAILURE);
}

TierEngineAdhoc::~TierEngineAdhoc() {}

void TierEngineAdhoc::set_socket_permissions(void) {
	struct group *at_grp = getgrnam("autotier");
	if (at_grp != nullptr) {
		if (chown((run_path_ / "adhoc.socket").c_str(), -1, at_grp->gr_gid) == -1) {
			int error = errno;
			Logging::log.warning(std::string("Failed to chown adhoc.socket: ") + strerror(error));
		}
		if (chmod((run_path_ / "adhoc.socket").c_str(), 0775) == -1) {
			int error = errno;
			Logging::log.warning(std::string("Failed to chmod adhoc.socket: ") + strerror(error));
		}
	} else {
		Logging::log.warning("`autotier` group not found, ad hoc commands must be run as root.");
	}
}

void TierEngineAdhoc::process_adhoc_requests(void) {
	std::vector<std::string> payload;
	while (!stop_flag_) {
		try {
			socket_server_.wait_for_connection();
			socket_server_.receive_data_async(payload);
		} catch (const ffd::SocketAcceptException &err) {
			if (err.get_errno() == EINVAL && stop_flag_) {
				Logging::log.message("Adhoc server exiting after shutting down socket.",
									 Logger::DEBUG);
				return;
			}
			Logging::log.warning(std::string("Socket accept error: ") + err.what());
			continue;
		} catch (const ffd::SocketReadException &err) {
			Logging::log.warning(std::string("Socket receive error: ") + err.what());
			continue;
		}
		AdHoc work(payload);
		try {
			switch (work.cmd_) {
				case ONESHOT:
					process_oneshot(work);
					break;
				case PIN:
				case UNPIN:
					process_pin_unpin(work);
					break;
				case STATUS:
					process_status(work);
					break;
				case CONFIG:
					process_config();
					break;
				case LPIN:
					process_list_pins();
					break;
				case LPOP:
					process_list_popularity();
					break;
				case WHICHTIER:
					process_which_tier(work);
					break;
				default:
					Logging::log.warning("Received bad ad hoc command.");
					payload.clear();
					payload.push_back("ERR");
					payload.push_back("Not a command.");
					try {
						socket_server_.send_data_async(payload);
					} catch (const ffd::SocketException &err) {
						Logging::log.warning(std::string("Socket reply error: ") + err.what());
						// let it notify main tier thread
					}
					break;
			}
		} catch (const ffd::SocketException &err) {
			Logging::log.warning(std::string("Socket reply error: ") + err.what());
		}
		socket_server_.close_connection();
		sleep_cv_.notify_one();
	}
}

void TierEngineAdhoc::process_oneshot(const AdHoc &work) {
	std::vector<std::string> payload;
	if (!work.args_.empty()) {
		payload.push_back("ERR");
		std::string err_msg = "autotier oneshot takes no arguments. Offender(s):";
		for (const std::string &str : work.args_)
			err_msg += " " + str;
		payload.push_back(err_msg);
		socket_server_.send_data_async(payload);
		return;
	}
	if (currently_tiering()) {
		payload.push_back("ERR");
		payload.push_back("autotier already tiering.");
		socket_server_.send_data_async(payload);
		return;
	}
	adhoc_work_.push(work);
	payload.push_back("OK");
	payload.push_back("Work queued.");
	socket_server_.send_data_async(payload);
}

void TierEngineAdhoc::process_pin_unpin(const AdHoc &work) {
	std::vector<std::string> payload;
	std::vector<std::string>::const_iterator itr = work.args_.begin();
	if (work.cmd_ == PIN) {
		std::string tier_id = work.args_.front();
		if (tier_lookup(tier_id) == nullptr) {
			payload.push_back("ERR");
			payload.push_back("Tier does not exist: \"" + tier_id + "\"");
			socket_server_.send_data_async(payload);
			return;
		}
		++itr;
	}
	std::vector<std::string> not_in_fs;
	for (; itr != work.args_.end(); ++itr) {
		if (!std::equal(mount_point_.string().begin(), mount_point_.string().end(), itr->begin())) {
			not_in_fs.push_back(*itr);
		}
	}
	if (!not_in_fs.empty()) {
		payload.push_back("ERR\n");
		std::string err_msg = "Files are not in autotier filesystem:";
		for (const std::string &str : not_in_fs)
			err_msg += " " + str;
		payload.push_back(err_msg);
		socket_server_.send_data_async(payload);
		return;
	}
	adhoc_work_.push(work);
	payload.push_back("OK");
	payload.push_back("Work queued.");
	socket_server_.send_data_async(payload);
}

#define TABLE_HEADER_LINE
#define UNIT_GAP 1
#define ABSW     7
#define ABSU     3 + UNIT_GAP
#define PERCENTW 6
#define PERCENTU 1 + UNIT_GAP

namespace l {
	inline int find_max_width(const std::vector<std::string> &names) {
		int res = -1;
		for (const std::string &name : names) {
			int length = name.length();
			if (length > res)
				res = length;
		}
		return res;
	}
} // namespace l

void TierEngineAdhoc::process_status(const AdHoc &work) {
	ffd::Bytes total_capacity = 0;
	ffd::Bytes total_quota_capacity = 0;
	ffd::Bytes total_usage = 0;
	std::vector<std::string> payload;

	bool json;
	std::stringstream json_ss(work.args_.front());

	try {
		json_ss >> std::boolalpha >> json;
	} catch (const std::ios_base::failure &) {
		Logging::log.error("Could not extract boolean from string.");
		payload.push_back("ERR");
		payload.push_back("Could not determine whether to use table or JSON output.");
		socket_server_.send_data_async(payload);
		return;
	}

	payload.push_back("OK");

	std::string unit("");
	for (const Tier &t : tiers_) {
		total_capacity += t.capacity();
		total_quota_capacity += t.quota();
		total_usage += t.usage_bytes();
	}
	double overall_quota = total_quota_capacity / total_capacity * 100.0;
	double total_percent_usage = total_usage / total_capacity * 100.0;

	std::vector<std::string> conflicts;
	bool has_conflicts = check_conflicts(conflicts, run_path_);

	std::stringstream ss;
	if (json) {
		ss << 
		"{"
			"\"version\":\"" VERS "\","
			"\"combined\":{"
				"\"capacity\":" + std::to_string(total_capacity.get()) + ","
				"\"capacity_pretty\":\"" + total_capacity.get_str() + "\","
				"\"quota\":" + std::to_string(total_quota_capacity.get()) + ","
				"\"quota_pretty\":\"" + total_quota_capacity.get_str() + "\","
				"\"usage\":" + std::to_string(total_usage.get()) + ","
				"\"usage_pretty\":\"" + total_usage.get_str() + "\","
				"\"path\":\"" + mount_point_.string() + "\""
			"},"
			"\"tiers\":[";
		for (std::list<Tier>::iterator tptr = tiers_.begin(); tptr != tiers_.end(); ++tptr) {
			ss <<
				"{"
					"\"name\":\"" + tptr->id() + "\","
					"\"capacity\":" + std::to_string(tptr->capacity().get()) + ","
					"\"capacity_pretty\":\"" + tptr->capacity().get_str() + "\","
					"\"quota\":" + std::to_string(tptr->quota().get()) + ","
					"\"quota_pretty\":\"" + tptr->quota().get_str() + "\","
					"\"usage\":" + std::to_string(tptr->usage_bytes().get()) + ","
					"\"usage_pretty\":\"" + tptr->usage_bytes().get_str() + "\","
					"\"path\":\"" + tptr->path().string() + "\""
				"}";
			if (std::next(tptr) != tiers_.end())
				ss << ",";
		}
		ss << "],"
			  "\"conflicts\":{"
			  "\"has_conflicts\":"
		   << std::boolalpha << has_conflicts << std::noboolalpha
		   << ","
			  "\"paths\":[";
		for (std::vector<std::string>::iterator itr = conflicts.begin(); itr != conflicts.end();
			 ++itr) {
			ss << "\"" + *itr + "\"";
			if (std::next(itr) != conflicts.end())
				ss << ",";
		}
		ss << "]"
			  "}"
			  "}";
	} else {
		std::vector<std::string> names;
		names.push_back("combined");
		for (std::list<Tier>::iterator tptr = tiers_.begin(); tptr != tiers_.end(); ++tptr) {
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
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right
			   << Logging::log.format_bytes(total_capacity.get(), unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right
			   << Logging::log.format_bytes(total_quota_capacity.get(), unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(PERCENTW) << std::right
			   << overall_quota;
			ss << std::setw(PERCENTU) << "%"; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right
			   << Logging::log.format_bytes(total_usage.get(), unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(PERCENTW) << std::right
			   << total_percent_usage;
			ss << std::setw(PERCENTU) << "%"; // unit
			ss << " ";
			ss << std::left << mount_point_.string();
			ss << std::endl;
		}
		for (std::list<Tier>::iterator tptr = tiers_.begin(); tptr != tiers_.end(); ++tptr) {
			// Tiers
			ss << std::setw(namew) << std::left << tptr->id(); // tier
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right
			   << Logging::log.format_bytes(tptr->capacity().get(), unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right
			   << Logging::log.format_bytes(tptr->quota().get(), unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(PERCENTW) << std::right
			   << tptr->quota_percent();
			ss << std::setw(PERCENTU) << "%"; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(ABSW) << std::right
			   << Logging::log.format_bytes(tptr->usage_bytes().get(), unit);
			ss << std::setw(ABSU) << unit; // unit
			ss << " ";
			ss << std::fixed << std::setprecision(2) << std::setw(PERCENTW) << std::right
			   << tptr->usage_percent();
			ss << std::setw(PERCENTU) << "%"; // unit
			ss << " ";
			ss << std::left << tptr->path().string();
			ss << std::endl;
		}
		if (has_conflicts) {
			ss << "\n" << std::endl;
			ss << "autotier encountered conflicting file paths between tiers:" << std::endl;
			for (const std::string &conflict : conflicts) {
				ss << conflict + "(.autotier_conflict)" << std::endl;
			}
		}
	}
	payload.push_back(ss.str());
	socket_server_.send_data_async(payload);
}

void TierEngineAdhoc::process_config(void) {
	std::vector<std::string> payload;
	payload.push_back("OK");
	std::stringstream ss;
	config_.dump(tiers_, ss);
	payload.push_back(ss.str());
	socket_server_.send_data_async(payload);
}

void TierEngineAdhoc::process_list_pins(void) {
	std::vector<std::string> payload;
	payload.push_back("OK");
	std::stringstream ss;
	ss << "File : Tier Path" << std::endl;
	rocksdb::Iterator *it = db_->NewIterator(rocksdb::ReadOptions());
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		Metadata f(it->value().ToString());
		if (f.pinned())
			ss << it->key().ToString() << " : " << f.tier_path() << std::endl;
	}
	payload.push_back(ss.str());
	socket_server_.send_data_async(payload);
}

void TierEngineAdhoc::process_list_popularity(void) {
	std::vector<std::string> payload;
	payload.push_back("OK");
	std::stringstream ss;
	ss << "File : Popularity (accesses per hour)" << std::endl;
	rocksdb::Iterator *it = db_->NewIterator(rocksdb::ReadOptions());
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		Metadata f(it->value().ToString());
		ss << it->key().ToString() << " : " << f.popularity() << std::endl;
	}
	payload.push_back(ss.str());
	socket_server_.send_data_async(payload);
}

void TierEngineAdhoc::process_which_tier(AdHoc &work) {
	std::vector<std::string> payload;
	payload.push_back("OK");
	int namew = 0;
	int tierw = 0;
	for (std::string &arg : work.args_) {
		if (std::equal(mount_point_.string().begin(), mount_point_.string().end(), arg.begin())) {
			arg = fs::relative(arg, mount_point_).string();
		}
		int len = arg.length();
		if (len > namew)
			namew = len;
	}
	int file_header_len = std::string("File").length();
	if (file_header_len > namew)
		namew = file_header_len;
	for (const Tier &tier : tiers_) {
		int len = tier.id().length() + 2; // 2 quotes
		if (len > tierw)
			tierw = len;
	}
	std::stringstream ss;
	ss << std::setw(namew) << std::left << "File"
	   << "  ";
	ss << std::setw(tierw) << std::left << "Tier"
	   << "  ";
	ss << std::left << "Backend Path";
#ifdef TABLE_HEADER_LINE
	ss << std::endl;
	auto fill = ss.fill();
	ss << std::setw(80) << std::setfill('-') << "";
	ss.fill(fill);
#endif
	ss << std::endl;
	for (const std::string &arg : work.args_) {
		ss << std::setw(namew) << std::left << arg << "  ";
		Metadata f(arg.c_str(), db_);
		if (f.not_found()) {
			ss << "not found";
		} else {
			Tier *tptr = tier_lookup(fs::path(f.tier_path()));
			if (tptr == nullptr)
				ss << std::setw(tierw) << std::left << "UNK"
				   << "  ";
			else
				ss << std::setw(tierw) << std::left << "\"" + tptr->id() + "\""
				   << "  ";
			ss << std::left << (fs::path(f.tier_path()) / arg).string();
		}
		ss << std::endl;
	}
	payload.push_back(ss.str());
	socket_server_.send_data_async(payload);
}

void TierEngineAdhoc::execute_queued_work(void) {
	while (!adhoc_work_.empty()) {
		AdHoc work = adhoc_work_.pop();
		switch (work.cmd_) {
			case ONESHOT:
				oneshot_in_queue_ = false;
				tier();
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

void TierEngineAdhoc::pin_files(const std::vector<std::string> &args) {
	Tier *tptr;
	std::string tier_id = args.front();
	if ((tptr = tier_lookup(tier_id)) == nullptr) {
		Logging::log.warning("Tier does not exist, cannot pin files. Tier name given: " + tier_id);
		return;
	}
	for (std::vector<std::string>::const_iterator fptr = std::next(args.begin());
		 fptr != args.end();
		 ++fptr) {
		fs::path mounted_path = *fptr;
		fs::path relative_path = fs::relative(mounted_path, mount_point_);
		Metadata f(relative_path.string(), db_);
		if (f.not_found()) {
			Logging::log.warning("File to be pinned was not in database: " + mounted_path.string());
			continue;
		}
		fs::path old_path = f.tier_path() / relative_path;
		fs::path new_path = tptr->path() / relative_path;
		struct stat st;
		if (stat(old_path.c_str(), &st) == -1) {
			Logging::log.warning("stat failed on " + old_path.string() + ": " + strerror(errno));
			continue;
		}
		struct timeval times[2];
		times[0].tv_sec = st.st_atim.tv_sec;
		times[0].tv_usec = st.st_atim.tv_nsec / 1000;
		times[1].tv_sec = st.st_mtim.tv_sec;
		times[1].tv_usec = st.st_mtim.tv_nsec / 1000;
		if (tptr->move_file(old_path, new_path, config_.copy_buff_sz())) {
			if (utimes(new_path.c_str(), times) == -1) {
				int error = errno;
				Logging::log.error("Failed to set utimes of " + new_path.string() + ": " + strerror(error));
			}
			f.pinned(true);
			f.tier_path(tptr->path().string());
			f.update(relative_path.string(), db_);
		}
	}
	if (!config_.strict_period())
		enqueue_work(ONESHOT, std::vector<std::string>{});
}

void TierEngineAdhoc::unpin_files(const std::vector<std::string> &args) {
	for (const std::string &mounted_path : args) {
		fs::path relative_path = fs::relative(mounted_path, mount_point_);
		Metadata f(relative_path.c_str(), db_);
		if (f.not_found()) {
			Logging::log.warning("File to be unpinned was not in database: " + mounted_path);
			continue;
		}
		f.pinned(false);
		f.update(relative_path.c_str(), db_);
	}
}

void TierEngineAdhoc::shutdown_socket_server(void) {
	Logging::log.message("Shutting down socket.", Logger::log_level_t::DEBUG);
	try {
		socket_server_.shutdown();
	} catch (ffd::SocketException &e) {
		Logging::log.error(std::string("Socket shutdown error: ") + e.what());
	}
}
