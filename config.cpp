#include "config.hpp"
#include "error.hpp"
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
  
  while(config_file){
    std::size_t str_itr;
    std::stringstream line_stream;
    std::string line, key, value;
    getline(config_file, line);
    // discard comments
    if(line.empty() || line.front() == '#') continue;
    if((str_itr = line.find('#')) != std::string::npos){
      str_itr--;
      while(line.at(str_itr) == ' ' || line.at(str_itr) == '\t') str_itr--;
      line = line.substr(0,str_itr + 1);
    }
    line_stream.str(line);
    getline(line_stream, key, '=');
    getline(line_stream, value);
    if(key == "FAST_TIER_DIR"){
      this->fast_tier = value;
    }else if(key == "SLOW_TIER_DIR"){
      this->slow_tier = value;
    }else if(key == "THRESHOLD"){
      try{
        this->threshold = stol(value);
      }catch(std::invalid_argument){
        this->threshold = ERR;
      }
    } // else ignore
  }
  
  if(this->verify()){
    error(LOAD_CONF);
    exit(1);
  }
}

void Config::generate_config(std::fstream &file){
  file <<
  "# autotier config\n"
  "FAST_TIER_DIR=                # full path to fast tier storage pool\n"
  "SLOW_TIER_DIR=                # full path to slow tier storage pool\n"
  "THRESHOLD=                    # file age in seconds at which to move file to slower tier\n"
  "# file age is calculated as (current time - file mtime), i.e. the amount\n"
  "# of time that has passed since the file was last modified."
  << std::endl;
}

bool Config::verify(){
  bool errors = false;
  if(!is_directory(this->fast_tier)){
    error(FAST_TIER_DNE);
    errors = true;
  }
  if(!is_directory(this->slow_tier)){
    error(SLOW_TIER_DNE);
    errors = true;
  }
  if(this->threshold == ERR){
    error(THRESHOLD_ERR);
    errors = true;
  }
  return errors;
}

const fs::path &Config::get_fast_tier() const{
  return this->fast_tier;
}

const fs::path &Config::get_slow_tier() const{
  return this->slow_tier;
}

long Config::get_threshold() const{
  return this->threshold;
}

void Config::dump(std::ostream &os) const{
  os << "FAST_TIER_DIR=" << this->fast_tier.string() << std::endl;
  os << "SLOW_TIER_DIR=" << this->slow_tier.string() << std::endl;
  os << "THRESHOLD=" << this->threshold << std::endl;
}
