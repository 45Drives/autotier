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

/* popularity calculation
 * y[n] = MULTIPLIER/(DAMPING * (time_diff + 1)) + (1 - 1/DAMPING) * y[n-1]
 */
#define DAMPING 1000000.0
#define MULTIPLIER 1000.0
/* DAMPING is how slowly popularity changes
 * TIME_DIFF is time since last file access
 * MULTIPLIER is to scale values
 */

#define CALC_PERIOD 1
/* period in seconds for popularity calculation
 */

#define AVG_USAGE 0.238 // 40hr/(7days * 24hr/day)
/* for calculating initial popularity for new files
 */

#include "alert.hpp"
#include "tier.hpp"

class Tier;
/* forward declaration of class Tier
 */

#include <sqlite3.h>
#include <utime.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class File{
private:
	sqlite3 *db;
  /* database for storing file metadata
   * was previously stored in extended attributes
   */
public:
	File(fs::path path_, Tier *tptr, sqlite3 *db_);
  /* file constructor used while tiering
   */
	File(fs::path path_, sqlite3 *db_);
  /* file constructor used by FUSE filesystem
   */
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
