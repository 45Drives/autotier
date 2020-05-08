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

#include "tierEngine.hpp"
#include "tier.hpp"
#include "file.hpp"
#include "alert.hpp"

#include <chrono>
#include <thread>
#include <regex>
#include <fcntl.h>
#include <sys/xattr.h>

fs::path TierEngine::get_mutex_name(const fs::path &config_path){
  return fs::path(std::to_string(std::hash<std::string>{}(config_path.string())) + ".autotier.lock");
}

int TierEngine::lock_mutex(void){
  if(!is_directory(mutex_path.parent_path())){
    create_directories(mutex_path.parent_path());
  }
  int result = open(mutex_path.c_str(), O_CREAT|O_EXCL);
  close(result);
  return result;
}

void TierEngine::unlock_mutex(void){
  remove(mutex_path);
}

void TierEngine::begin(bool daemon_mode){
  Log("autotier started.",1);
  unsigned int timer = 0;
  do{
    auto start = std::chrono::system_clock::now();
    launch_crawlers(&TierEngine::emplace_file);
    // one popularity calculation per loop
    calc_popularity();
    if(timer == 0){
      // mutex lock
      if(lock_mutex() == ERR){
        Log("autotier already moving files.",0);
      }else{
        // mutex locked
        // one tier execution per tier period
        sort();
        simulate_tier();
        move_files();
        Log("Tiering complete.",1);
        unlock_mutex();
      }
    }
    files.erase(files.begin(), files.end());
    for(std::vector<Tier>::iterator t = tiers.begin(); t != tiers.end(); ++t){
      t->cleanup();
    }
    auto end = std::chrono::system_clock::now();
    auto duration = end - start;
    // don't wait for oneshot execution
    if(daemon_mode && duration < std::chrono::seconds(CALC_PERIOD))
      std::this_thread::sleep_for(std::chrono::seconds(CALC_PERIOD)-duration);
    timer = (++timer) % (config.period / CALC_PERIOD);
  }while(daemon_mode);
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
      //fptr->log_movement();
      fptr->move();
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

void TierEngine::calc_popularity(){
  Log("Calculating file popularity.",2);
  for(std::list<File>::iterator f = files.begin(); f != files.end(); ++f){
    f->calc_popularity();
  }
}
