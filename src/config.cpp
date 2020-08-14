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

#include "config.hpp"
#include "alert.hpp"
#include "tierEngine.hpp"
#include "tier.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <regex>

void Config::load(const fs::path &config_path, std::list<Tier> &tiers){
	log_lvl = 1; // default to 1
	std::fstream config_file(config_path.string(), std::ios::in);
	if(!config_file){
		if(!is_directory(config_path.parent_path())) create_directories(config_path.parent_path());
		config_file.open(config_path.string(), std::ios::out);
		this->generate_config(config_file);
		config_file.close();
		config_file.open(config_path.string(), std::ios::in);
	}
	Tier *tptr = NULL;
	while(config_file){
		std::stringstream line_stream;
		std::string line, key, value;
		
		getline(config_file, line);
		
		// discard comments
		if(line.empty() || line.front() == '#') continue;
		strip_whitespace(line);
		
		if(line.empty()) continue;
		
		if(line.front() == '['){
			std::string id = line.substr(1,line.find(']')-1);
			if(regex_match(id,std::regex("^\\s*[Gg]lobal\\s*$"))){
				if(this->load_global(config_file, id) == EOF) break;
			}
      tiers.emplace_back(id);
      tptr = &tiers.back();
		}else if(tptr){
			line_stream.str(line);
			getline(line_stream, key, '=');
			getline(line_stream, value);
			strip_whitespace(key);
			strip_whitespace(value);
			if(key == "DIR"){
				tptr->dir = value;
			}else if(key == "WATERMARK"){
				try{
					tptr->watermark = stoi(value);
				}catch(const std::invalid_argument &){
					tptr->watermark = ERR;
				}
			} // else ignore
		}
	}
	
	if(!verify(tiers)){
		error(LOAD_CONF);
		exit(1);
	}
}

int Config::load_global(std::fstream &config_file, std::string &id){
	while(config_file){
		std::stringstream line_stream;
		std::string line, key, value;
		
		getline(config_file, line);
		
		// discard comments
		if(line.empty() || line.front() == '#') continue;
		strip_whitespace(line);
		
		if(line.front() == '['){
			id = line.substr(1,line.find(']')-1);
			return 0;
		}
		
		line_stream.str(line);
		getline(line_stream, key, '=');
		getline(line_stream, value);
		strip_whitespace(key);
		strip_whitespace(value);
		
		
		if(key == "LOG_LEVEL"){
			try{
				this->log_lvl = stoi(value);
			}catch(const std::invalid_argument &){
				this->log_lvl = ERR;
			}
		}else if(key == "TIER_PERIOD"){
			try{
				this->period = stoul(value);
			}catch(const std::invalid_argument &){
				this->period = ERR;
			}
		}else if(key == "MOUNT_POINT"){
			this->mountpoint = fs::path(value);
		} // else if ...
	}
	// if here, EOF reached
	return EOF;
}

void strip_whitespace(std::string &str){
#ifdef DEBUG_WS
	std::cout << "Input: \"" << str << "\"" << std::endl;
#endif
	if(str.length() == 0) return;
	std::size_t strItr;
	// back ws
	if((strItr = str.find('#')) == std::string::npos){ // strItr point to '#' or end
		strItr = str.length();
	}
	strItr--; // point to last character
	while(strItr && (str.at(strItr) == ' ' || str.at(strItr) == '\t')){ // remove whitespace
		strItr--;
	} // strItr points to last character
	str = str.substr(0,strItr + 1);
	if(str.empty()) return;
	// front ws
	strItr = 0;
	while(strItr < str.length() && (str.at(strItr) == ' ' || str.at(strItr) == '\t')){ // remove whitespace
		strItr++;
	} // strItr points to first character
	str = str.substr(strItr, str.length() - strItr);
#ifdef DEBUG_WS
	std::cout << "Output: \"" << str << "\"" << std::endl;
#endif
}

void Config::generate_config(std::fstream &file){
	file <<
	"# autotier config\n"
	"[Global]						# global settings\n"
	"LOG_LEVEL=1				 # 0 = none, 1 = normal, 2 = debug\n"
	"TIER_PERIOD=1000		# number of seconds between file move batches\n"
	"MOUNT_POINT=/mnt/autotier"
	"\n"
	"[Tier 1]\n"
	"DIR=								# full path to tier storage pool\n"
	"WATERMARK=			# % usage at which to tier down from tier\n"
	"# file age is calculated as (current time - file mtime), i.e. the amount\n"
	"# of time that has passed since the file was last modified.\n"
	"[Tier 2]\n"
	"DIR=\n"
	"WATERMARK=\n"
	"# ... (add as many tiers as you like)\n"
	<< std::endl;
}

bool Config::verify(const std::list<Tier> &tiers){
	bool no_errors = true;
	if(tiers.empty()){
		error(NO_TIERS);
		no_errors = false;
	}else if(tiers.size() == 1){
		error(ONE_TIER);
		no_errors = false;
	}
	if(log_lvl == ERR){
		error(LOG_LVL);
		no_errors = false;
	}
	if(period == (unsigned long)ERR){
		error(PERIOD);
		no_errors = false;
	}
	if(!fs::exists(mountpoint) || !fs::is_directory(mountpoint)){
		error(MOUNTPOINT);
		no_errors = false;
	}
	for(Tier t : tiers){
		if(!is_directory(t.dir)){
			std::cerr << t.id << ": ";
			error(TIER_DNE);
      no_errors = false;
		}
		if(t.watermark == ERR || t.watermark > 100 || t.watermark < 0){
			std::cerr << t.id << ": ";
			error(WATERMARK_ERR);
      no_errors = false;
		}
	}
	return no_errors;
}

void Config::dump(std::ostream &os, const std::list<Tier> &tiers) const{
	os << "[Global]" << std::endl;
	os << "LOG_LEVEL=" << this->log_lvl << std::endl;
  os << "MOUNT_POINT=" << this->mountpoint.string() << std::endl;
	os << std::endl;
	for(Tier t : tiers){
		os << "[" << t.id << "]" << std::endl;
		os << "DIR=" << t.dir.string() << std::endl;
		os << "WATERMARK=" << t.watermark << std::endl;
		os << std::endl;
	}
}
