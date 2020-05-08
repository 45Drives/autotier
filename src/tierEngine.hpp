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

#pragma once

#define MUTEX_PATH "/run/autotier"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include "tier.hpp"
#include "file.hpp"
#include "config.hpp"
#include "alert.hpp"

// forward declarations
class Tier;
class File;
class Config;

class TierEngine{
private:
  int mutex;
  fs::path mutex_path;
  std::vector<Tier> tiers;
  std::list<File> files;
  Config config;
  fs::path get_mutex_name(const fs::path &config_path);
  int lock_mutex(void);
  void unlock_mutex(void);
public:
  TierEngine(const fs::path &config_path){
    config.load(config_path, tiers);
    log_lvl = config.log_lvl;
    mutex_path = fs::path(MUTEX_PATH) / get_mutex_name(config_path);
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
