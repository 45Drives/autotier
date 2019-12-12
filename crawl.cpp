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

#include "crawl.hpp"
#include "config.hpp"
#include "alert.hpp"
#include "xxhash64.h"
#include <iomanip>
#include <regex>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <utime.h>
#include <unistd.h>
#include <fcntl.h>
#include <list>

Tier *highest_tier = NULL;
Tier *lowest_tier = NULL;

void launch_crawlers(){
  // get ordered list of files in each tier
  for(Tier *tptr = highest_tier; tptr != NULL; tptr=tptr->lower){
      tptr->crawl(tptr->dir);
  }
  
  std::cout << "Files from freshest to stalest: " << std::endl;
  
  for(Tier *tptr = highest_tier; tptr != NULL; tptr=tptr->lower){
    std::cout << tptr->id << std::endl;
    for(File f : tptr->files){
      std::cout << "Age: " << f.age << " Location: " << f.path << std::endl;
    }
    std::cout << std::endl;
  }
  
  /*
  // tier down
  for(Tier *tptr = highest_tier; tptr->lower != NULL; tptr=tptr->lower){
    if(tptr->usage_watermark == DISABLED || get_fs_usage(tptr->dir) >= tptr->usage_watermark)
      std::list<File>::reverse_iterator itr = tptr->files.rbegin();
  }
  // tier up
  for(Tier *tptr = lowest_tier; tptr->higher != NULL; tptr=tptr->higher){
    if(tptr->higher->usage_watermark == DISABLED || get_fs_usage(tptr->higher->dir) < tptr->higher->usage_watermark)
      
  }//*/
}

void Tier::crawl(fs::path dir){
  for(fs::directory_iterator itr{dir}; itr != fs::directory_iterator{}; *itr++){
    if(is_directory(*itr)){
      this->crawl(*itr);
    }else if(!is_symlink(*itr) &&
    !regex_match((*itr).path().filename().string(), std::regex("(^\\..*(\\.swp)$|^(\\.~lock\\.).*#$|^(~\\$))"))){
      this->files.push_back(File{(time(NULL) - last_times(*itr).actime),*itr});
    }
    this->files.sort([](const File &a, const File &b){return a.age < b.age;});
  }
}

static void tier_up(fs::path from_here, Tier *tptr){
  struct utimbuf times = last_times(from_here);
  if((time(NULL) - times.actime) >= tptr->higher->expires) return;
  Log("Tiering up",2);
  fs::path to_here = tptr->higher->dir/relative(from_here, tptr->dir);
  if(is_symlink(to_here)){
    remove(to_here);
  }
  copy_file(from_here, to_here); // move item back to original location
  copy_ownership_and_perms(from_here, to_here);
  if(verify_copy(from_here, to_here)){
    Log("Copy succeeded.",2);
    remove(from_here);
  }else{
    Log("Copy failed.",0);
  }
  utime(to_here.c_str(), &times); // overwrite mtime and atime with previous times
}

static void tier_down(fs::path from_here, Tier *tptr){
  struct utimbuf times = last_times(from_here);
  if((time(NULL) - times.actime) < tptr->expires) return;
  Log("Tiering down",2);
  fs::path to_here = tptr->lower->dir/relative(from_here, tptr->dir);
  if(!is_directory(to_here.parent_path()))
    create_directories(to_here.parent_path());
  copy_file(from_here, to_here); // move item to slow tier
  copy_ownership_and_perms(from_here, to_here);
  if(verify_copy(from_here, to_here)){
    Log("Copy succeeded.",2);
    remove(from_here);
    create_symlink(to_here, from_here); // create symlink fast/item -> slow/item
  }else{
    Log("Copy failed.",0);
  }
  utime(to_here.c_str(), &times); // overwrite mtime and atime with previous times
}

void copy_ownership_and_perms(const fs::path &src, const fs::path &dst){
  struct stat info;
  stat(src.c_str(), &info);
  chown(dst.c_str(), info.st_uid, info.st_gid);
  chmod(dst.c_str(), info.st_mode);
}

bool verify_copy(const fs::path &src, const fs::path &dst){
  int bytes_read;
  char *src_buffer = new char[4096];
  char *dst_buffer = new char[4096];
  
  int srcf = open(src.c_str(),O_RDONLY);
  int dstf = open(dst.c_str(),O_RDONLY);
  
  XXHash64 src_hash(0);
  XXHash64 dst_hash(0);
  
  while((bytes_read = read(srcf,src_buffer,sizeof(char[4096]))) > 0){
    src_hash.add(src_buffer,bytes_read);
  }
  while((bytes_read = read(dstf,dst_buffer,sizeof(char[4096]))) > 0){
    dst_hash.add(dst_buffer,bytes_read);
  }
  
  close(srcf);
  close(dstf);
  delete [] src_buffer;
  delete [] dst_buffer;
  
  uint64_t src_result = src_hash.hash();
  uint64_t dst_result = dst_hash.hash();
  
  std::stringstream ss;
  
  ss << "SRC HASH: 0x" << std::hex << src_result << std::endl;
  ss << "DST HASH: 0x" << std::hex << dst_result << std::endl;
  
  Log(ss.str(),2);
  
  return (src_result == dst_result);
}

struct utimbuf last_times(const fs::path &file){
  struct stat info;
  stat(file.c_str(), &info);
  struct utimbuf times;
  times.actime = info.st_atime;
  times.modtime = info.st_mtime;
  return times;
}

int get_fs_usage(const fs::path &dir){
  struct statvfs fs_stats;
  if((statvfs(dir.c_str(), &fs_stats) == -1))
    return -1;
  return (int)((fs_stats.f_blocks - fs_stats.f_bfree) * (fsblkcnt_t)100 / fs_stats.f_blocks); 
}

void destroy_tiers(void){
  if(highest_tier == lowest_tier){
    delete highest_tier;
    return;
  }
  for(Tier *tptr = highest_tier->lower; tptr != NULL; tptr = tptr->lower)
    delete tptr->higher;
  delete lowest_tier;
}
