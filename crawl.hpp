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

#pragma once

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class Tier{
public:
  int usage_watermark;
  long expires;
  fs::path dir;
  std::string id;
  Tier *higher;
  Tier *lower;
  void crawl(fs::path dir, void (*action)(fs::path, Tier *));
};

extern Tier *highest_tier;
extern Tier *lowest_tier;

void launch_crawlers(void);

static void tier_up(fs::path from_here, Tier *tptr);
static void tier_down(fs::path from_here, Tier *tptr);

void copy_ownership_and_perms(const fs::path &src, const fs::path &dst);

bool verify_copy(const fs::path &src, const fs::path &dst);

struct utimbuf last_times(const fs::path &file);

int get_fs_usage(const fs::path &dir);
// returns &usage of fs as integer 0-100

void destroy_tiers(void);
