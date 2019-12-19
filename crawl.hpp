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

#include <iostream>

#include <boost/filesystem.hpp>
#include <utime.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
namespace fs = boost::filesystem;

#include "alert.hpp"
#include "config.hpp"

#define BUFF_SZ 4096

class Config; // forward declaration

class File{
public:
  unsigned long priority;
  long last_atime;
  long size;
  struct utimbuf times;
  fs::path symlink_path;
  fs::path old_path;
  fs::path new_path;
  fs::path pinned_to;
  void write_xattrs(){
    if(setxattr(new_path.c_str(),"user.autotier_last_atime",&last_atime,sizeof(last_atime),0)==ERR)
      error(SETX);
    if(setxattr(new_path.c_str(),"user.autotier_priority",&priority,sizeof(priority),0)==ERR)
      error(SETX);
    if(setxattr(new_path.c_str(),"user.autotier_pin",pinned_to.c_str(),strlen(pinned_to.c_str()),0)==ERR)
      error(SETX);
  }
  File(fs::path path_){
    char strbuff[BUFF_SZ];
    ssize_t attr_len;
    old_path = path_;
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
    if(getxattr(old_path.c_str(),"user.autotier_priority",&priority,sizeof(priority)) <= 0){
      priority = 0;
    }
    // age
    priority = priority >> 1;
    if(times.actime > last_atime){
      priority |= ((unsigned long)0x01 << (sizeof(unsigned long)*8 - 1));
    }
    last_atime = times.actime;
  }
  File(const File &rhs) {
    priority = rhs.priority;
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
    Log("Destroying file.",2);
    write_xattrs();
  }
  File &operator=(const File &rhs){
    priority = rhs.priority;
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
  long watermark_bytes;
  int watermark;
  fs::path dir;
  std::string id;
  std::list<File *> incoming_files;
  Tier(std::string id_){
    id = id_;
  }
  long get_fs_usage(File *file = NULL);
  long set_capacity();
};

class TierEngine{
private:
  std::vector<Tier> tiers;
  std::list<File> files;
  Config config;
public:
  TierEngine(const fs::path &config_path){
    config.load(config_path, tiers);
  }
  void begin(void);
  void launch_crawlers(void);
  void crawl(fs::path dir);
  void sort(void);
  void simulate_tier(void);
  void move_files(void);
  //void dump_tiers(void);
};

void copy_ownership_and_perms(const fs::path &src, const fs::path &dst);

bool verify_copy(const fs::path &src, const fs::path &dst);

void destroy_tiers(void);

void dump_tiers(void);
