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

#include "file.hpp"
#include "alert.hpp"

#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <iostream>

void File::log_movement(){
  Log("OldPath: " + old_path.string(),3);
  Log("NewPath: " + new_path.string(),3);
  Log("SymLink: " + symlink_path.string(),3);
  Log("UserPin: " + pinned_to.string(),3);
  Log("",3);
}

void File::move(){
  if(old_path == new_path) return;
  if(new_path == symlink_path && is_symlink(new_path))
    remove(symlink_path);
  if(!is_directory(new_path.parent_path()))
    create_directories(new_path.parent_path());
  Log("Copying " + old_path.string() + " to " + new_path.string(),2);
  bool copy_success = true;
  try{
    copy_file(old_path, new_path); // move item to slow tier
  }catch(boost::filesystem::filesystem_error const & e){
    copy_success = false;
    std::cerr << "Copy failed: " << e.what() << std::endl;
    if(e.code() == boost::system::errc::file_exists){
      std::cerr << "User intervention required to delete duplicate file" << std::endl;
    }else if(e.code() == boost::system::errc::no_such_file_or_directory){
      deleted = true;
      std::cerr << "No action required." << std::endl;
    }
  }
  if(copy_success){
    copy_ownership_and_perms();
    Log("Copy succeeded.\n",2);
    remove(old_path);
    if(new_path != symlink_path){
      if(is_symlink(symlink_path)) remove(symlink_path);
      create_symlink(new_path, symlink_path);
    }
  }
  utime(new_path.c_str(), &times); // overwrite mtime and atime with previous times
}

void File::copy_ownership_and_perms(){
  struct stat info;
  stat(old_path.c_str(), &info);
  chown(new_path.c_str(), info.st_uid, info.st_gid);
  chmod(new_path.c_str(), info.st_mode);
}

void File::calc_popularity(){
  double diff;
  if(times.actime > last_atime){
    // increase popularity
    diff = times.actime - last_atime;
  }else{
    // decrease popularity
    diff = time(NULL) - last_atime;
  }
  if(diff < 1) diff = 1;
  double delta = (popularity / DAMPING) * (1.0 - (diff / NORMALIZER) * popularity);
  double delta_cap = -1.0*popularity/2.0; // limit change to half of current val
  if(delta < delta_cap)
    delta = delta_cap;
  popularity += delta;
  if(popularity < FLOOR) // ensure val is positive else unstable (-inf)
    popularity = FLOOR;
}

void File::write_xattrs(){
  if(deleted) return;
  if(setxattr(new_path.c_str(),"user.autotier_last_atime",&last_atime,sizeof(last_atime),0)==ERR)
    error(SETX);
  if(setxattr(new_path.c_str(),"user.autotier_popularity",&popularity,sizeof(popularity),0)==ERR)
    error(SETX);
  if(setxattr(new_path.c_str(),"user.autotier_pin",pinned_to.c_str(),strlen(pinned_to.c_str()),0)==ERR)
    error(SETX);
}

File::File(fs::path path_, Tier *tptr){
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

File::File(const File &rhs){
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

File::~File(){
  write_xattrs();
}

File &File::operator=(const File &rhs){
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
