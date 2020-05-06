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
#include <iostream>
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
#include <fstream>

void TierEngine::begin(bool daemon_mode){
  Log("autotier started.",1);
  do{
    launch_crawlers(&TierEngine::emplace_file);
    sort();
    simulate_tier();
    move_files();
  }while(daemon_mode);
  Log("Tiering complete.",1);
}

void TierEngine::launch_crawlers(void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr)){
  Log("Gathering files.",2);
  // get ordered list of files in each tier
  for(std::vector<Tier>::iterator t = tiers.begin(); t != tiers.end(); ++t){
    crawl(t->dir, &(*t), function);
  }
}

void TierEngine::crawl(fs::path dir, Tier *tptr, void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr)){
  for(fs::directory_iterator itr{dir}; itr != fs::directory_iterator{}; *itr++){
    if(is_directory(*itr)){
      crawl(*itr, tptr, function);
    }else if(!is_symlink(*itr) &&
    !regex_match((*itr).path().filename().string(), std::regex("(^\\..*(\\.swp)$|^(\\.~lock\\.).*#$|^(~\\$))"))){
      (this->*function)(*itr, tptr);
    }
  }
}

void TierEngine::emplace_file(fs::directory_entry &file, Tier *tptr){
  files.emplace_back(file, tptr);
}

void TierEngine::print_file_pin(fs::directory_entry &file, Tier *tptr){
  int attr_len;
  char strbuff[BUFF_SZ];
  if((attr_len = getxattr(file.path().c_str(),"user.autotier_pin",strbuff,sizeof(strbuff))) != ERR){
    if(attr_len == 0)
      return;
    strbuff[attr_len] = '\0'; // c-string
    std::cout << file.path().string() << std::endl;
    std::cout << "pinned to" << std::endl;
    std::vector<Tier>::iterator tptr_;
    for(tptr_ = tiers.begin(); tptr_ != tiers.end(); ++tptr_){
      if(std::string(strbuff) == tptr_->dir.string())
        break;
    }
    if(tptr_ == tiers.end()){
      Log("Tier does not exist.",0);
    }else{
      std::cout << tptr_->id << std::endl;
    }
    std::cout << "(" << strbuff << ")" << std::endl;
    std::cout << std::endl;
  }
}

void TierEngine::print_file_popularity(){
  for(File f : files){
    std::cout << f.old_path.string() << " popularity: " << f.popularity << std::endl;
  }
}

void TierEngine::sort(){
  Log("Sorting files.",2);
  files.sort(
    [](const File &a, const File &b){
      return (a.popularity == b.popularity)? a.times.actime > b.times.actime : a.popularity > b.popularity;
    }
  );
}

void TierEngine::simulate_tier(){
  Log("Finding files' tiers.",2);
  long tier_use = 0;
  std::list<File>::iterator fptr = files.begin();
  std::vector<Tier>::iterator tptr = tiers.begin();
  tptr->watermark_bytes = tptr->get_capacity() * tptr->watermark / 100;
  while(fptr != files.end()){
    if(tier_use + fptr->size >= tptr->watermark_bytes){
      // move to next tier
      tier_use = 0;
      if(++tptr == tiers.end()) break;
      tptr->watermark_bytes = tptr->get_capacity() * tptr->watermark / 100;
    }
    tier_use += fptr->size;
    /*
     * TODO: only place file in incoming queue if destination tier != current tier
     */
    tptr->incoming_files.emplace_back(&(*fptr));
    fptr++;
  }
}

void TierEngine::move_files(){
  /*
   * Currently, this starts at the lowest tier, assuming it has the most free space, and
   * moves all incoming files from their current tiers before moving on to the next lowest
   * tier. There should be a better way to shuffle all the files around to avoid over-filling
   * a tier by doing them one at a time.
   */
  Log("Moving files.",2);
  for(std::vector<Tier>::reverse_iterator titr = tiers.rbegin(); titr != tiers.rend(); titr++){
    for(File * fptr : titr->incoming_files){
      fptr->symlink_path = tiers.front().dir/relative(fptr->old_path, fptr->old_tier->dir);
      if(!fptr->pinned_to.empty())
        fptr->new_path = fptr->pinned_to;
      else
        fptr->new_path = titr->dir;
      fptr->new_path /= relative(fptr->old_path, fptr->old_tier->dir);
      /*
       * TODO: handle cases where file already exists at destination (should not happen but could)
       * 1 - Check if new_path exists.
       * 2 - Hash both old_path and new_path to check if they are the same file.
       * 2a- If old_hash == new_hash, remove old_path and create symlink if needed.
       * 2b- If old_hash != new_hash, rename the new_path file with new_path plus something to make it unique. 
       *     Be sure to check if new name doesnt exist before moving the file.
       */
      fptr->log_movement();
      if(fptr->new_path != fptr->symlink_path){
        fptr->move();
        if(is_symlink(fptr->symlink_path)) remove(fptr->symlink_path);
        create_symlink(fptr->new_path, fptr->symlink_path);
      }else{ // moving to top tier
        if(is_symlink(fptr->new_path)) remove(fptr->new_path);
        fptr->move();
      }
    }
  }
}

void TierEngine::print_tier_info(void){
  int i = 1;
  std::cout << "Tiers from fastest to slowest:" << std::endl;
  std::cout << std::endl;
  for(std::vector<Tier>::iterator tptr = tiers.begin(); tptr != tiers.end(); ++tptr){
    std::cout <<
    "Tier " << i++ << ":" << std::endl <<
    "tier name: \"" << tptr->id << "\"" << std::endl <<
    "tier path: " << tptr->dir.string() << std::endl <<
    "current usage: " << tptr->get_usage() * 100 / tptr->get_capacity() << "% (" << tptr->get_usage() << " bytes)" << std::endl <<
    "watermark: " << tptr->watermark << "% (" << tptr->get_capacity() * tptr->watermark / 100 << " bytes)" << std::endl <<
    std::endl;
  }
}

void TierEngine::pin_files(std::string tier_name, std::vector<fs::path> &files_){
  std::vector<Tier>::iterator tptr;
  for(tptr = tiers.begin(); tptr != tiers.end(); ++tptr){
    if(tier_name == tptr->id)
      break;
  }
  if(tptr == tiers.end()){
    Log("Tier does not exist.",0);
    exit(1);
  }
  for(std::vector<fs::path>::iterator fptr = files_.begin(); fptr != files_.end(); ++fptr){
    if(!exists(*fptr)){
      Log("File does not exist! "+fptr->string(),0);
      continue;
    }
    if(setxattr(fptr->c_str(),"user.autotier_pin",tptr->dir.c_str(),strlen(tptr->dir.c_str()),0)==ERR)
      error(SETX);
  }
}

void File::log_movement(){
  Log("OldPath: " + old_path.string(),3);
  Log("NewPath: " + new_path.string(),3);
  Log("SymLink: " + symlink_path.string(),3);
  Log("UserPin: " + pinned_to.string(),3);
  Log("",3);
}

void File::move(){
  if(old_path == new_path) return;
  if(!is_directory(new_path.parent_path()))
    create_directories(new_path.parent_path());
  Log("Copying " + old_path.string() + " to " + new_path.string(),2);
  copy_file(old_path, new_path); // move item to slow tier
  copy_ownership_and_perms();
  if(verify_copy()){
    Log("Copy succeeded.\n",2);
    remove(old_path);
  }else{
    Log("Copy failed!\n",0);
    /*
     * TODO: put in place protocol for what to do when this happens
     */
  }
  utime(new_path.c_str(), &times); // overwrite mtime and atime with previous times
}

void File::copy_ownership_and_perms(){
  struct stat info;
  stat(old_path.c_str(), &info);
  chown(new_path.c_str(), info.st_uid, info.st_gid);
  chmod(new_path.c_str(), info.st_mode);
}

bool File::verify_copy(){
  /*
   * TODO: more efficient error checking than this? Also make
   * optional in global configuration?
   */
  int bytes_read;
  char *src_buffer = new char[4096];
  char *dst_buffer = new char[4096];
  
  int srcf = open(old_path.c_str(),O_RDONLY);
  int dstf = open(new_path.c_str(),O_RDONLY);
  
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
  ss << "DST HASH: 0x" << std::hex << dst_result;
  
  Log(ss.str(),2);
  
  return (src_result == dst_result);
}

unsigned long Tier::get_capacity(){
  /*
   * Returns maximum number of bytes
   * available in a tier
   */
  struct statvfs fs_stats;
  if((statvfs(dir.c_str(), &fs_stats) == -1))
    return -1;
  return (fs_stats.f_blocks * fs_stats.f_bsize);
}

unsigned long Tier::get_usage(){
  /*
   * Returns number of free bytes in a tier
   */
  struct statvfs fs_stats;
  if((statvfs(dir.c_str(), &fs_stats) == -1))
    return -1;
  return (fs_stats.f_blocks - fs_stats.f_bfree) * fs_stats.f_bsize;
}
