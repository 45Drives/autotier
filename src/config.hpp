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

#include "tier.hpp"

#include <iostream>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#define DEFAULT_CONFIG_PATH "/etc/autotier.conf"
#define ERR -1
#define DISABLED -999

class Config{
private:
  void generate_config(std::fstream &file);
  bool verify(const std::vector<Tier> &tiers);
public:
  int log_lvl = ERR;
  unsigned long period = ERR;
  void load(const fs::path &config_path, std::vector<Tier> &tiers);
  int load_global(std::fstream &config_file, std::string &id);
  void dump(std::ostream &os, const std::vector<Tier> &tiers) const;
};

void discard_comments(std::string &str);
