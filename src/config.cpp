/*
    Copyright (C) 2019-2020 Joshua Boudreau
    
    This file is part of autotier.

    autotier is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    autotier is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with autotier.  If not, see <https://www.gnu.org/licenses/>.
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

void Config::load(const fs::path &config_path, std::vector<Tier> &tiers){
  log_lvl = 1; // default to 1
  std::fstream config_file(config_path.string(), std::ios::in);
  if(!config_file){
    if(!is_directory(config_path.parent_path())) create_directories(config_path.parent_path());
    config_file.open(config_path.string(), std::ios::out);
    this->generate_config(config_file);
    config_file.close();
    config_file.open(config_path.string(), std::ios::in);
  }
  
  while(config_file){
    std::stringstream line_stream;
    std::string line, key, value;
    
    getline(config_file, line);
    
    // discard comments
    if(line.empty() || line.front() == '#') continue;
    discard_comments(line);
    
    if(line.front() == '['){
      std::string id = line.substr(1,line.find(']')-1);
      if(regex_match(id,std::regex("^\\s*[Gg]lobal\\s*$"))){
        if(this->load_global(config_file, id) == EOF) break;
      }
      tiers.emplace_back(id);
    }else if(!tiers.empty()){
      line_stream.str(line);
      getline(line_stream, key, '=');
      getline(line_stream, value);
      if(key == "DIR"){
        tiers.back().dir = value;
      }else if(key == "WATERMARK"){
        try{
          tiers.back().watermark = stoi(value);
        }catch(std::invalid_argument){
          tiers.back().watermark = ERR;
        }
      } // else ignore
    }else{
      error(NO_FIRST_TIER);
      exit(1);
    }
  }
    
  if(verify(tiers)){
    error(LOAD_CONF);
    exit(1);
  }
  
  //if(log_lvl >= 2) dump(std::cout, tiers);
}

int Config::load_global(std::fstream &config_file, std::string &id){
  while(config_file){
    std::stringstream line_stream;
    std::string line, key, value;
    
    getline(config_file, line);
    
    // discard comments
    if(line.empty() || line.front() == '#') continue;
    discard_comments(line);
    
    if(line.front() == '['){
      id = line.substr(1,line.find(']')-1);
      return 0;
    }
    
    line_stream.str(line);
    getline(line_stream, key, '=');
    getline(line_stream, value);
    
    if(key == "LOG_LEVEL"){
      try{
        this->log_lvl = stoi(value);
      }catch(std::invalid_argument){
        this->log_lvl = ERR;
      }
    }else if(key == "TIER_PERIOD"){
      try{
        this->period = stoul(value);
      }catch(std::invalid_argument){
        this->period = ERR;
      }
    } // else if ...
  }
  // if here, EOF reached
  return EOF;
}

void discard_comments(std::string &str){
  std::size_t str_itr;
  if((str_itr = str.find('#')) != std::string::npos){
    str_itr--;
    while(str.at(str_itr) == ' ' || str.at(str_itr) == '\t') str_itr--;
    str = str.substr(0,str_itr + 1);
  }
}

void Config::generate_config(std::fstream &file){
  file <<
  "# autotier config\n"
  "[Global]            # global settings\n"
  "LOG_LEVEL=1         # 0 = none, 1 = normal, 2 = debug\n"
  "TIER_PERIOD=1000    # number of seconds between file move batches\n"
  "\n"
  "[Tier 1]\n"
  "DIR=                # full path to tier storage pool\n"
  "WATERMARK=      # % usage at which to tier down from tier\n"
  "# file age is calculated as (current time - file mtime), i.e. the amount\n"
  "# of time that has passed since the file was last modified.\n"
  "[Tier 2]\n"
  "DIR=\n"
  "WATERMARK=\n"
  "# ... (add as many tiers as you like)\n"
  << std::endl;
}

bool Config::verify(const std::vector<Tier> &tiers){
  bool errors = false;
  if(tiers.empty()){
    error(NO_TIERS);
    errors = true;
  }else if(tiers.size() == 1){
    error(ONE_TIER);
    errors = true;
  }
  if(log_lvl == ERR){
    error(LOG_LVL);
    errors = true;
  }
  if(period == ERR){
    error(PERIOD);
    errors = true;
  }
  for(Tier t : tiers){
    if(!is_directory(t.dir)){
      std::cerr << t.id << ": ";
      error(TIER_DNE);
      errors = true;
    }
    if(t.watermark == ERR || t.watermark > 100 || t.watermark < 0){
      std::cerr << t.id << ": ";
      error(WATERMARK_ERR);
      errors = true;
    }
  }
  return errors;
}

void Config::dump(std::ostream &os, const std::vector<Tier> &tiers) const{
  os << "[Global]" << std::endl;
  os << "LOG_LEVEL=" << this->log_lvl << std::endl;
  os << std::endl;
  for(Tier t : tiers){
    os << "[" << t.id << "]" << std::endl;
    os << "DIR=" << t.dir.string() << std::endl;
    os << "WATERMARK=" << t.watermark << std::endl;
    os << std::endl;
  }
}
