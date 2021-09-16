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

#include "config.hpp"
#include "alert.hpp"
#include "tier.hpp"
#include <45d/config/ConfigSubsectionGuard.hpp>
#include <45d/Quota.hpp>
#include <sstream>
#include <regex>

extern "C" {
	#include <sys/statvfs.h>
}

void validate_backend_path(const fs::path &path, const std::string &prefix, const std::string &path_desc, bool &errors, bool create = false){
	bool is_directory = false;
	bool no_perm = false;
	if(path.is_relative()){
		Logging::log.error(prefix + ": " + path_desc + " must be an absolute path: \"" + path.string() + "\"");
		errors = true;
		return;
	}
	try{
		is_directory = fs::is_directory(path);
	}catch(const fs::filesystem_error &e){
		Logging::log.error(prefix + ": Failed to check " + path_desc + ": " + std::string(e.what()));
		errors = true;
		no_perm = true;
	}
	if(!is_directory && !no_perm && create){
		try{
			is_directory = fs::create_directories(path);
		}catch(const fs::filesystem_error &e){
			Logging::log.error(prefix + ": Failed to create " + path_desc + ": " + std::string(e.what()));
			errors = true;
			no_perm = true;
		}
	}
	if(is_directory && !no_perm){
		int mode = R_OK|W_OK;
		if(access(path.c_str(), mode) != 0){
			int err = errno;
			Logging::log.error(prefix + ": Cannot access " + path_desc + ": " + std::string(strerror(err)) + ": \"" + path.string() + "\"");
			errors = true;
		}
	}
	if(!is_directory && !no_perm){
		Logging::log.error(prefix + ": " + path_desc + " is not a directory: " + path.string());
		errors = true;
	}
}

Config::Config(const fs::path &config_path, std::list<Tier> &tiers, const ConfigOverrides &config_overrides)
try : ffd::ConfigParser(config_path.string()) {
	load_config(config_path, tiers, config_overrides);
} catch (const ffd::NoConfigException &) {
	init_config_file(config_path);
	load_config(config_path, tiers, config_overrides);
}

void Config::load_config(const fs::path &config_path, std::list<Tier> &tiers, const ConfigOverrides &config_overrides) {
	bool errors;

	std::vector<std::string> valid_global_headers = {"Global", "global"};
	std::vector<std::string>::iterator global_header_itr = valid_global_headers.begin();
	while (global_header_itr != valid_global_headers.end()) {
		try {
			ffd::ConfigSubsectionGuard guard(*this, "Global");
			int log_level_tmp = get<int>("Log Level", LogLevel::NORMAL);
			log_level_ = (Logger::log_level_t)(log_level_tmp > 2 ? 2 : (log_level_tmp < 0 ? 0 : log_level_tmp));
			copy_buff_sz_ = get<ffd::Bytes>("Copy Buffer Size", ffd::Bytes(1024*1024)).get();
			tier_period_s_ = std::chrono::seconds(get<int64_t>("Tier Period", int64_t(TIER_PERIOD_DISBLED)));
			strict_period_ = get<bool>("Strict Period", false);
			run_path_ = get<std::string>("Run Path", "/var/lib/autotier");
			break;
		} catch (const std::out_of_range &e) {
			++global_header_itr;
		}
	}
	if (global_header_itr == valid_global_headers.end()) {
		Logging::log.warning("No global section in config! Trying top level scope or defaults (no tiering).");
		int log_level_tmp = get<int>("Log Level", LogLevel::NORMAL);
		log_level_ = (Logger::log_level_t)(log_level_tmp > 2 ? 2 : (log_level_tmp < 0 ? 0 : log_level_tmp));
		copy_buff_sz_ = get<ffd::Bytes>("Copy Buffer Size", ffd::Bytes(1024*1024)).get();
		tier_period_s_ = std::chrono::seconds(get<int64_t>("Tier Period", int64_t(TIER_PERIOD_DISBLED)));
		strict_period_ = get<bool>("Strict Period", false);
		run_path_ = get<std::string>("Run Path", "/var/lib/autotier");
	}

	// fill overrides
	if(config_overrides.log_level_override.overridden()){
		log_level_ = config_overrides.log_level_override.value();
	}
	Logging::log.set_level(log_level_);
	Logging::log.message("Global config loaded.", Logger::log_level_t::DEBUG);
	
	for (ffd::ConfigNode *subsection : sub_confs_) {
		std::string tier_name = subsection->value_;
		if (regex_match(tier_name, std::regex("^\\s*[Gg]lobal\\s*$")))
			continue;
		Logging::log.message("Checking config[" + tier_name + "]", Logger::log_level_t::DEBUG);
		ffd::ConfigSubsectionGuard guard(*this, tier_name);
		std::string tier_path = get<std::string>("Path", &errors);
		if (errors)
			continue;
		struct statvfs fs_stats;
		if((statvfs(tier_path.c_str(), &fs_stats) == -1)){
			Logging::log.error("statvfs() failed on " + tier_path);
			exit(EXIT_FAILURE);
		}
		ffd::Bytes tier_size(fs_stats.f_blocks * fs_stats.f_frsize);
		ffd::Quota quota = get_quota("Quota", tier_size, ffd::Quota(tier_size, 1.0));
		if (log_level_ >= 2) {
			Logging::log.message("Tier path: " + tier_path, Logger::log_level_t::DEBUG);
			Logging::log.message("Tier quota: " + std::to_string(quota.get_fraction()*100.0) + "% " + quota.get_str() + " (" + ffd::Bytes(quota.get_max()).get_str() + ")", Logger::log_level_t::DEBUG);
		}
		tiers.emplace_back(tier_name, tier_path, quota);
	}
	Logging::log.message("Tier configs loaded.", Logger::log_level_t::DEBUG);
	
	run_path_ /= std::to_string(std::hash<std::string>{}(config_path.string()));
	validate_backend_path(run_path_, "Global", "Metadata Path", errors, true);
	
	if(tiers.empty()){
		Logging::log.error("No tiers defined.");
		errors = true;
	}else if(tiers.size() == 1){
		Logging::log.error("Only one tier is defined. Two or more are needed.");
		errors = true;
	}
	
	if(errors){
		Logging::log.error("Please fix these mistakes in " + config_path.string());
		exit(EXIT_FAILURE);
	}
}

size_t Config::copy_buff_sz(void) const{
	return copy_buff_sz_;
}

std::chrono::seconds Config::tier_period_s(void) const{
	return tier_period_s_;
}

bool Config::strict_period(void) const{
	return strict_period_;
}

fs::path Config::run_path(void) const{
	return run_path_;
}

void Config::dump(const std::list<Tier> &tiers, std::stringstream &ss) const{
	ss << "[Global]" << std::endl;
	ss << "Log Level = " << log_level_ << std::endl;
	ss << "Tier Period = " << tier_period_s_.count() << std::endl;
	ss << "Strict Period = " << (strict_period_ == 1? "true" : "false") << std::endl;
	ss << "Copy Buffer Size = " << Logging::log.format_bytes(copy_buff_sz_) << std::endl;
	ss << " " << std::endl;
	for(const Tier &t : tiers){
		ss << "[" << t.id() << "]" << std::endl;
		ss << "Path = " << t.path() << std::endl;
		ss << "Quota = " << t.quota().get_fraction() * 100.0 << " % (" << t.quota().get_str() << ")" << std::endl;
		ss << " " << std::endl;
	}
}

void init_config_file(const fs::path &config_path) {
	boost::system::error_code ec;
	fs::create_directories(config_path.parent_path(), ec);
	if (ec) {
		Logging::log.error("Error creating path: " + config_path.parent_path().string());
		exit(EXIT_FAILURE);
	}
	std::ofstream f(config_path.string());
	if (!f) {
		Logging::log.error("Error opening config file: " + config_path.string());
		exit(EXIT_FAILURE);
	}
	f <<
	"# autotier config\n"
	"[Global]                       # global settings\n"
	"Log Level = 1                  # 0 = none, 1 = normal, 2 = debug\n"
	"Tier Period = 1000             # number of seconds between file move batches\n"
	"Copy Buffer Size = 1 MiB       # size of buffer for moving files between tiers\n"
	"\n"
	"[Tier 1]                       # tier name\n"
	"Path =                         # full path to tier storage pool\n"
	"Quota =                        # absolute or % usage to keep tier under\n"
	"# Quota format: x (%|B|MB|MiB|KB|KiB|MB|MiB|...)\n"
	"# Example: Quota = 5.3 TiB\n"
	"\n"
	"[Tier 2]\n"
	"Path =\n"
	"Quota =\n"
	"# ... (add as many tiers as you like)\n";
	f.close();
}
