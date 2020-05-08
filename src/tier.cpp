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

#include "tier.hpp"

#include <sys/statvfs.h>

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
