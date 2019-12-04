/*
    Copyright (C) 2019 Joshua Boudreau
    
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
#include "crawl.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <boost/filesystem.hpp>

Config config;

void Config::load(const fs::path &config_path){
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
    discard_comments(line);
    
    if(line.front() == '['){
      if(tptr){
        tptr->lower = new Tier;
        tptr->lower->higher = tptr;
        tptr->lower->lower = NULL;
        tptr = tptr->lower;
      }else{
        tptr = new Tier;
        tptr->higher = tptr->lower = NULL;
        highest_tier = tptr;
      }
      tptr->id = line.substr(1,line.find(']')-1);
      tptr->usage_watermark = DISABLED; // default to disabled until line is read below
    }else if(tptr){
      line_stream.str(line);
      getline(line_stream, key, '=');
      getline(line_stream, value);
      if(key == "DIR"){
        tptr->dir = value;
      }else if(key == "EXPIRES"){
        try{
          tptr->expires = stol(value);
        }catch(std::invalid_argument){
          tptr->expires = ERR;
        }
      }else if(key == "USAGE_WATERMARK"){
        if(value.empty()){
          tptr->usage_watermark = DISABLED;
        }else{
          try{
            tptr->usage_watermark = stoi(value);
          }catch(std::invalid_argument){
            tptr->usage_watermark = ERR;
          }
        }
      } // else ignore
    }else{
      error(NO_FIRST_TIER);
      exit(1);
    }
  }
  
  lowest_tier = tptr;
    
  if(this->verify()){
    error(LOAD_CONF);
    exit(1);
  }
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
  "[Tier 1]\n"
  "DIR=                # full path to tier storage pool\n"
  "EXPIRES=            # file age in seconds at which to move file to slower tier\n"
  "USAGE_WATERMARK=    # % usage at which to tier down, omit to disable\n"
  "# file age is calculated as (current time - file mtime), i.e. the amount\n"
  "# of time that has passed since the file was last modified.\n"
  "[Tier 2]\n"
  "DIR=\n"
  "EXPIRES=\n"
  "USAGE_WATERMARK=\n"
  "# ... (add as many tiers as you like)\n"
  << std::endl;
}

bool Config::verify(){
  bool errors = false;
  if(highest_tier == NULL || lowest_tier == NULL){
    error(NO_TIERS);
    errors = true;
  }else if(highest_tier == lowest_tier){
    error(ONE_TIER);
    errors = true;
  }
  for(Tier *tptr = highest_tier; tptr != NULL; tptr=tptr->lower){
    if(!is_directory(tptr->dir)){
      std::cerr << tptr->id << ": ";
      error(TIER_DNE);
      errors = true;
    }
    if(tptr->expires == ERR){
      std::cerr << tptr->id << ": ";
      error(THRESHOLD_ERR);
      errors = true;
    }
    if(tptr->usage_watermark != DISABLED &&
    (tptr->usage_watermark == ERR || tptr->usage_watermark > 100 || tptr->usage_watermark < 0)){
      std::cerr << tptr->id << ": ";
      error(WATERMARK_ERR);
      errors = true;
    }
  }
  return errors;
}

void Config::dump(std::ostream &os) const{
  for(Tier *tptr = highest_tier; tptr != NULL; tptr=tptr->lower){
    os << "[" << tptr->id << "]" << std::endl;
    os << "DIR=" << tptr->dir.string() << std::endl;
    os << "EXPIRES=" << tptr->expires << std::endl;
    os << "USAGE_WATERMARK=";
    if(tptr->usage_watermark == DISABLED)
      os << "(DISABLED)" << std::endl;
    else
      os << tptr->usage_watermark << std::endl;
    os << std::endl;
  }
}
