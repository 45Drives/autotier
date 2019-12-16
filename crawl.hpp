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

#define BUFF_SZ 4096

class File{
public:
  long age; // seconds since last access
  long priority;
  long last_age;
  struct utimbuf times;
  fs::path path;
  File(fs::path path_){
    path = path_;
    struct stat info;
    stat(path.c_str(), &info);
    times.actime = info.st_atime;
    times.modtime = info.st_mtime;
    age = time(NULL) - times.actime;
    if(getxattr(path.c_str(),"user.autotier_last_age",&last_age,sizeof(last_age)) <= 0){
      last_age = age;
      std::cout << "setting xattr: " << last_age << std::endl;
      if(setxattr(path.c_str(),"user.autotier_last_age",&last_age,sizeof(last_age),0)==ERR)
        error(SETX);
    }else{
      std::cout << "got xattr: " << last_age << std::endl;
    }
    if(getxattr(path.c_str(),"user.autotier_priority",&priority,sizeof(priority)) <= 0){
      priority = 0;
      if(setxattr(path.c_str(),"user.autotier_priority",&priority,sizeof(priority),0)==ERR)
        error(SETX);
    }
  }
  File(const File &rhs) {
    age = rhs.age;
    priority = rhs.priority;
    last_age = rhs.last_age;
    times.modtime = rhs.times.modtime;
    times.actime = rhs.times.actime;
    path = rhs.path;
  }
  void write_xattrs(){
    if(setxattr(path.c_str(),"user.autotier_last_age",&last_age,sizeof(last_age),0)==ERR)
      error(SETX);
    if(setxattr(path.c_str(),"user.autotier_priority",&priority,sizeof(priority),0)==ERR)
      error(SETX);
  }
};

class Tier{
public:
  int max_watermark;
  int min_watermark;
  Tier *higher;
  Tier *lower;
  fs::path dir;
  std::string id;
  std::list<File> files; // freshest to stalest
  void crawl(fs::path dir);
  void tier_down(File &file);
  void tier_up(File &file);
};

extern Tier *highest_tier;
extern Tier *lowest_tier;

void launch_crawlers(void);

void copy_ownership_and_perms(const fs::path &src, const fs::path &dst);

bool verify_copy(const fs::path &src, const fs::path &dst);

int get_fs_usage(const fs::path &dir);
// returns %usage of fs as integer 0-100

void destroy_tiers(void);
