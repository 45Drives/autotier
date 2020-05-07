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

#pragma once

#define AUDIT_LOG_PATH "/var/log/audit/audit.log"

// popularity variables
// y' = (1/DAMPING)*y*(1-(TIME_DIFF/NORMALIZER)*y)
// DAMPING is how slowly popularity changes
// TIME_DIFF is time since last file access
// NORMALIZER is TIME_DIFF at which popularity -> 1.0
// FLOOR ensures y stays above zero for stability
#define DAMPING 10.0
#define NORMALIZER 1000.0
#define FLOOR 0.1
#define CALC_PERIOD 1

#include <boost/filesystem.hpp>
#include <utime.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <time.h>
#include <fstream>
namespace fs = boost::filesystem;

#include "alert.hpp"
#include "config.hpp"

#define BUFF_SZ 4096

class Config; // forward declaration

class File{
public:
  //unsigned long priority;
  double popularity;   // calculated from number of accesses
  long last_atime;
  long size;
  Tier *old_tier;
  struct utimbuf times;
  fs::path symlink_path;
  fs::path old_path;
  fs::path new_path;
  fs::path pinned_to;
  void write_xattrs(void);
  void log_movement(void);
  void move(void);
  void copy_ownership_and_perms(void);
  bool verify_copy(void);
  void calc_popularity(void);
  File(fs::path path_, Tier *tptr){
    char strbuff[BUFF_SZ];
    ssize_t attr_len;
    new_path = old_path = path_;
    old_tier = tptr;
    struct stat info;
    stat(old_path.c_str(), &info);
    size = (long)info.st_size;
    times.actime = info.st_atime;
    times.modtime = info.st_mtime;
    if((attr_len = getxattr(old_path.c_str(),"user.autotier_pin",strbuff,sizeof(strbuff))) != ERR){
      strbuff[attr_len] = '\0'; // c-string
      pinned_to = fs::path(strbuff);
    }
    if(getxattr(old_path.c_str(),"user.autotier_last_atime",&last_atime,sizeof(last_atime)) <= 0){
      last_atime = times.actime;
    }
    if(getxattr(old_path.c_str(),"user.autotier_popularity",&popularity,sizeof(popularity)) <= 0){
      // initialize
      popularity = 1.0;
    }
    last_atime = times.actime;
  }
  File(const File &rhs) {
    //priority = rhs.priority;
    popularity = rhs.popularity;
    last_atime = rhs.last_atime;
    size = rhs.size;
    times.modtime = rhs.times.modtime;
    times.actime = rhs.times.actime;
    symlink_path = rhs.symlink_path;
    old_path = rhs.old_path;
    new_path = rhs.new_path;
    pinned_to = rhs.pinned_to;
  }
  ~File(){
    write_xattrs();
  }
  File &operator=(const File &rhs){
    //priority = rhs.priority;
    popularity = rhs.popularity;
    last_atime = rhs.last_atime;
    size = rhs.size;
    times.modtime = rhs.times.modtime;
    times.actime = rhs.times.actime;
    symlink_path = rhs.symlink_path;
    old_path = rhs.old_path;
    new_path = rhs.new_path;
    pinned_to = rhs.pinned_to;
    return *this;
  }
};

class Tier{
public:
  unsigned long watermark_bytes;
  int watermark;
  fs::path dir;
  std::string id;
  std::list<File *> incoming_files;
  Tier(std::string id_){
    id = id_;
  }
  unsigned long get_capacity();
  unsigned long get_usage();
  void cleanup(void){
    incoming_files.erase(incoming_files.begin(), incoming_files.end());
  }
};

class TierEngine{
private:
  std::vector<Tier> tiers;
  std::list<File> files;
  Config config;
public:
  TierEngine(const fs::path &config_path){
    config.load(config_path, tiers);
    log_lvl = config.log_lvl;
  }
  void begin(bool daemon_mode);
  void launch_crawlers(void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr));
  void crawl(fs::path dir, Tier *tptr, void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr));
  void emplace_file(fs::directory_entry &file, Tier *tptr);
  void print_file_pin(fs::directory_entry &file, Tier *tptr);
  void print_file_popularity(void);
  void sort(void);
  void simulate_tier(void);
  void move_files(void);
  void print_tier_info(void);
  void pin_files(std::string tier_name, std::vector<fs::path> &files_);
  void calc_popularity(void);
};
