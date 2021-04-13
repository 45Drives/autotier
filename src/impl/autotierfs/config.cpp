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
#include "stripWhitespace.hpp"
#include <sstream>
#include <regex>
#include <cmath>

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

void parse_quota(const std::string &value, Tier *tptr, bool &errors){
	std::smatch m;
	if(regex_search(value, m, std::regex("^(\\d+)\\s*%$"))){
		try{
			int quota_percent = std::stoi(m.str(1));
			if(quota_percent > 100 || quota_percent < 0){
				Logging::log.error(tptr->id() + ": Invalid percent in quota: " + value);
				errors = true;
			}
			tptr->quota_percent(quota_percent);
		}catch(const std::invalid_argument &){
			Logging::log.error(tptr->id() + ": Invalid quota: " + value);
			errors = true;
			return;
		}
	}else if(regex_search(value, m, std::regex("^(\\d+)\\s*([kKmMgGtTpPeEzZyY]?)(i?)[bB]$"))){
		double num;
		try{
			num = std::stod(m[1]);
			if(num < 0.0){
				Logging::log.error(tptr->id() + ": Quota cannot be negative: " + value);
				errors = true;
			}
		}catch(const std::invalid_argument &){
			Logging::log.error(tptr->id() + ": Invalid quota: " + value);
			errors = true;
			return;
		}
		char prefix = (m.str(2).empty())? 0 : m.str(2).front();
		double base = (m.str(3).empty())? 1000.0 : 1024.0;
		double exp;
		switch(prefix){
			case 0:
				exp = 0.0;
				break;
			case 'k':
			case 'K':
				exp = 1.0;
				break;
			case 'm':
			case 'M':
				exp = 2.0;
				break;
			case 'g':
			case 'G':
				exp = 3.0;
				break;
			case 't':
			case 'T':
				exp = 4.0;
				break;
			case 'p':
			case 'P':
				exp = 5.0;
				break;
			case 'e':
			case 'E':
				exp = 6.0;
				break;
			case 'z':
			case 'Z':
				exp = 7.0;
				break;
			case 'y':
			case 'Y':
				exp = 8.0;
				break;
			default:
				Logging::log.error(tptr->id() + ": Invalid quota unit: " + value);
				errors = true;
				return;
		}
		tptr->quota_bytes(num * pow(base, exp));
	}else{
		Logging::log.error(tptr->id() + ": Invalid quota: " + value);
		errors = true;
		return;
	}
}

Config::Config(const fs::path &config_path, std::list<Tier> &tiers, const ConfigOverrides &config_overrides){
	bool errors = false;
	std::string line, key, value;
	
	// open file
	std::ifstream config_file(config_path.string());
	if(!config_file){
		config_file.close();
		init_config_file(config_path);
		config_file.open(config_path.string());
	}
	
	// fill overrides
	if(config_overrides.log_level_override.overridden()){
		log_level_ = config_overrides.log_level_override.value();
	}
	
	Tier *tptr = nullptr;
	while(config_file){
		getline(config_file, line);
		
		strip_whitespace(line);
		// full line comments:
		if(line.empty() || line.front() == '#')
			continue; // ignore comments
		
		if(line.front() == '['){
			std::string id = line.substr(1, line.find(']')-1);
			if(regex_match(id, std::regex("^\\s*[Gg]lobal\\s*$"))){
				if(load_global(config_file, id, errors) == EOF) break;
			}
			tiers.emplace_back(id);
			tptr = &tiers.back();
		}else if(tptr){
			std::stringstream line_stream(line);
			
			getline(line_stream, key, '=');
			getline(line_stream, value);
			
			strip_whitespace(key);
			strip_whitespace(value);
			
			if(key.empty() || value.empty())
				continue; // ignore unassigned fields
			
			if(key == "Path"){
				tptr->path(value);
				validate_backend_path(tptr->path(), tptr->id(), "Path", errors);
			}else if(key == "Quota"){
				parse_quota(value, tptr, errors);
			}else{ // else if ...
				Logging::log.warning(tptr->id() + ": Unknown field: " + key + " = " + value);
			}
		}
	}
	
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
	
	Logging::log.set_level(log_level_);
	
	for(Tier & t: tiers){
		t.get_capacity_and_usage();
		t.calc_quota_bytes();
	}
}

Config::Config(const fs::path &config_path, const ConfigOverrides &config_overrides){
	bool errors = false;
	std::string line, key, value;
	
	// open file
	std::ifstream config_file(config_path.string());
	if(!config_file){
		Logging::log.error("Failed to open config file.");
		exit(EXIT_FAILURE);
	}
	
	// fill overrides
	if(config_overrides.log_level_override.overridden()){
		log_level_ = config_overrides.log_level_override.value();
	}
	
	while(config_file){
		getline(config_file, line);
		
		strip_whitespace(line);
		// full line comments:
		if(line.empty() || line.front() == '#')
			continue; // ignore comments
		
		if(line.front() == '['){
			std::string id = line.substr(1, line.find(']')-1);
			if(regex_match(id, std::regex("^\\s*[Gg]lobal\\s*$"))){
				if(load_global(config_file, id, errors) == EOF) break;
			}
		}
	}
	
	run_path_ /= std::to_string(std::hash<std::string>{}(config_path.string()));
	validate_backend_path(run_path_, "Global", "Metadata Path", errors, true);
	
	if(errors){
		Logging::log.error("Please fix these mistakes in " + config_path.string());
		exit(EXIT_FAILURE);
	}
	
	Logging::log.set_level(log_level_);
}

int Config::load_global(std::ifstream &config_file, std::string &id, bool &errors){
	std::string line, key, value;
	while(config_file){
		getline(config_file, line);
		
		strip_whitespace(line);
		// full line comments:
		if(line.empty() || line.front() == '#')
			continue; // ignore comments
		
		if(line.front() == '['){
			id = line.substr(1,line.find(']')-1);
			return 0;
		}
		
		std::stringstream line_stream(line);
		getline(line_stream, key, '=');
		getline(line_stream, value);
		strip_whitespace(key);
		strip_whitespace(value);
		
		if(key == "Log Level" && log_level_ == LOG_LEVEL_NOT_SET){
			try{
				log_level_ = stoi(value);
			}catch(const std::invalid_argument &){
				Logging::log.error("Invalid Log Level: " + value);
				errors = true;
			}
		}else if(key == "Tier Period"){
			try{
				tier_period_s_ = std::chrono::seconds(stoi(value));
			}catch(const std::invalid_argument &){
				Logging::log.error("Invalid Log Level: " + value);
				errors = true;
			}
		}else if(key == "Strict Period"){
			if(regex_match(value, std::regex("true", std::regex_constants::icase)))
				strict_period_ = true;
			else if(regex_match(value, std::regex("false", std::regex_constants::icase)))
				strict_period_ = false;
			else{
				Logging::log.error("Invalid boolean for Strict Period: " + value);
				errors = true;
			}
		}else if(key == "Metadata Path"){
			run_path_ = value;
		}else{ // else if ...
			Logging::log.warning("Unknown global field: " + key + " = " + value);
		}
	}
	// if here, EOF reached
	return EOF;
}

void Config::init_config_file(const fs::path &config_path) const{
	boost::system::error_code ec;
	fs::create_directories(config_path.parent_path(), ec);
	if(ec){
		Logging::log.error("Error creating path: " + config_path.parent_path().string());
		exit(EXIT_FAILURE);
	}
	std::ofstream f(config_path.string());
	if(!f){
		Logging::log.error("Error opening config file: " + config_path.string());
		exit(EXIT_FAILURE);
	}
	f <<
	"# autotier config\n"
	"[Global]                       # global settings\n"
	"Log Level = 1                  # 0 = none, 1 = normal, 2 = debug\n"
	"Tier Period = 1000             # number of seconds between file move batches\n"
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
	ss << "Log Level = " + std::to_string(log_level_) << std::endl;
	ss << "Tier Period = " + std::to_string(tier_period_s_.count()) << std::endl;
	ss << "Strict Period = " + (strict_period_ == 1? std::string("true") : std::string("false")) << std::endl;
	ss << " " << std::endl;
	for(const Tier &t : tiers){
		ss << "[" + t.id() + "]" << std::endl;
		ss << "Path = " + t.path().string() << std::endl;
		ss << "Quota = " + std::to_string(t.quota_percent()) + " % (" + Logging::log.format_bytes(t.quota_bytes()) + ")" << std::endl;
		ss << " " << std::endl;
	}
}
