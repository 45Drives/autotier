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

#include "file.hpp"

class File;

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

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
