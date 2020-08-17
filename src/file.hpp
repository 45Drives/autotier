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
	File(fs::path path_, sqlite3 *db_, Tier *tptr = NULL);
  /* file constructor used while tiering
   * overwrites current tier path based on tptr if tptr != NULL,
   * else does not modify current tier path and
   * grabs current tier path from db
   */
	~File();
  /* destructor - calls put_info(db)
   */
	size_t ID;
  /* std::hash of rel_path
   * used for indexing database table
   */
	double popularity = MULTIPLIER*AVG_USAGE;
  /* popularity rating for sorting files
   * defaults to new file popularity
   * but is overridden with value from databse if exists
   */
	time_t last_atime;
  /* last access time taken from stat() call
   */
	unsigned long size;
  /* size of file in bytes
   */
	Tier *old_tier;
  /* pointer to old tier for use while moving
   */
  struct timeval times[2];
	fs::path rel_path;
  /* file path relative to tier directory
   */
	fs::path old_path;
  /* absolute path to file before moving
   */
	fs::path new_path;
  /* absolute path to file after moving
   */
	fs::path pinned_to;
  /* absolute path to root of tier to which file is pinned
   */
	fs::path current_tier;
  /* absolute path to root of tier file currently occupies
   */
	void move(void);
  /* IF new_path != old path THEN
   * moves file from old_path to new_path
   * ELSE
   * do nothing
   * END IF
   * does not modify atime or mtime
   */
	void copy_ownership_and_perms(void);
  /* copy file ownership (user and group) from old_path to new_path
   * copy file permissions from old_path to new_path
   */
	void calc_popularity(void);
  /* calculate new popularity value of file
   */
	bool is_open(void);
  /* fork and exec lsof, use return value to determine if the file is
   * currently open. There is probably a less expensive way of doing this.
   */
	int get_info(sqlite3 *db);
  /* execute SQL to retrieve file metadata from db
   * using callback(count, data, cols) to load data
   */
	int put_info(sqlite3 *db);
  /* execute SQL to insert/update file metadata into db
   */
	int callback(int count, char *data[], char *cols[]);
  /* SQL callback - retrieves 1 record from DB per call and loads data from each
   * column into File object
   */
};
