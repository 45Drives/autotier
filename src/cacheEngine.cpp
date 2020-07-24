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

#include "cacheEngine.hpp"

void CacheEngine::simulate_tier(){
  Log("Determining files to cache.",2);
  long tier_use = 0;
  std::list<File>::iterator fptr = files.begin();
  cacheTier->watermark_bytes = cacheTier->get_capacity() * cacheTier->watermark / 100;
  while(fptr != files.end() && (tier_use + fptr->size < cacheTier->watermark_bytes)){
    tier_use += fptr->size;
    cacheTier->incoming_files.emplace_back(&(*fptr));
    fptr++;
  }
  while(fptr != files.end()){
    coldTier->incoming_files.emplace_back(&(*fptr));
    fptr++;
  }
}

void CacheEngine::move_files(){
  /*
   * Currently, this starts at the lowest tier, assuming it has the most free space, and
   * moves all incoming files from their current tiers before moving on to the next lowest
   * tier. There should be a better way to shuffle all the files around to avoid over-filling
   * a tier by doing them one at a time.
   */
  Log("Caching files.",2);
  for(File * fptr : cacheTier->incoming_files){ // cache
    fptr->new_path = cacheTier->dir/relative(fptr->old_path, fptr->old_tier->dir);
    fptr->cache();
  }
  for(File * fptr : coldTier->incoming_files){ // cache
    fptr->new_path = coldTier->dir/relative(fptr->old_path, fptr->old_tier->dir);
    fptr->uncache();
  }
}
