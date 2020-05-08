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

// popularity variables
// y' = (1/DAMPING)*y*(1-(TIME_DIFF/NORMALIZER)*y)
// DAMPING is how slowly popularity changes
// TIME_DIFF is time since last file access
// NORMALIZER is TIME_DIFF at which popularity -> 1.0
// FLOOR ensures y stays above zero for stability
#define DAMPING 50.0
#define NORMALIZER 1000.0
#define FLOOR 0.1
#define CALC_PERIOD 1

#define BUFF_SZ 4096

#include "alert.hpp"
#include "tier.hpp"

class Tier;

#include <utime.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class File{
private:
  bool deleted = false;
public:
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
  void calc_popularity(void);
  File(fs::path path_, Tier *tptr);
  File(const File &rhs);
  ~File();
  File &operator=(const File &rhs);
};
