/*
		Copyright (C) 2019-2020 Joshua Boudreau <jboudreau@45drives.com>
		
		This file is part of autotier.

		autotier is free software: you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation, either version 3 of the License, or
		(at your option) any later version.

		autotier is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with autotier.	If not, see <https://www.gnu.org/licenses/>.
*/


#pragma once

/* popularity variables
 * y' = (1/DAMPING)*y*(1-(TIME_DIFF/NORMALIZER)*y)
 * DAMPING is how slowly popularity changes
 * TIME_DIFF is time since last file access
 * NORMALIZER is TIME_DIFF at which popularity -> 1.0
 * FLOOR ensures y stays above zero for stability
 */
#define DAMPING 1000000.0
#define MULTIPLIER 1000.0

#define CALC_PERIOD 1

#define AVG_USAGE 0.238 // 40hr/(7days * 24hr/day)

#define BUFF_SZ 4096

#include "alert.hpp"
#include "tier.hpp"

class Tier;

#include <sqlite3.h>
#include <utime.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class File{
private:
	sqlite3 *db;
public:
	File(fs::path path_, Tier *tptr, sqlite3 *db_);
	File(fs::path path_, sqlite3 *db_);
	~File();
	size_t ID;
	double popularity = MULTIPLIER*AVG_USAGE;
	time_t last_atime;
	unsigned long size;
	Tier *old_tier;
  struct timeval times[2];
	//struct utimbuf times;
	fs::path rel_path;
	fs::path old_path;
	fs::path new_path;
	fs::path pinned_to;
	fs::path current_tier;
	void move(void);
	void copy_ownership_and_perms(void);
	void calc_popularity(void);
	bool is_open(void);
	int get_info(sqlite3 *db);
	int put_info(sqlite3 *db);
	int callback(int count, char *data[], char *cols[]);
};
