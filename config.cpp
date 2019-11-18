#include "config.hpp"
#include "error.hpp"
#include "crawl.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <boost/filesystem.hpp>

Config config;

void Config::load(const fs::path &config_path){
  std::fstream config_file(config_path.string(), std::fstream::in);
  if(!config_file){
    if(!is_directory(config_path.parent_path())) create_directories(config_path.parent_path());
    config_file.open(config_path.string(), std::fstream::out);
    this->generate_config(config_file);
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
  "# file age is calculated as (current time - file mtime), i.e. the amount\n"
  "# of time that has passed since the file was last modified.\n"
  "[Tier 2]\n"
  "DIR=\n"
  "EXPIRES=\n"
  "# ... (add as many tiers as you like)\n"
  << std::endl;
}

bool Config::verify(){
  bool errors = false;
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
  }
  return errors;
}

void Config::dump(std::ostream &os) const{
  for(Tier *tptr = highest_tier; tptr != NULL; tptr=tptr->lower){
    os << "[" << tptr->id << "]" << std::endl;
    os << "DIR=" << tptr->dir.string() << std::endl;
    os << "EXPIRES=" << tptr->expires << std::endl;
    os << std::endl;
  }
}
