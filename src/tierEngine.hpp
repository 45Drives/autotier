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

#include <sqlite3.h>
#include <string>
#include <signal.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include "tier.hpp"
#include "file.hpp"
#include "config.hpp"
#include "alert.hpp"

extern std::string RUN_PATH;

// forward declarations
class Tier;
class File;
class Config;

class TierEngine{
private:
	sqlite3 *db;
	pid_t fusermount_pid = 0;
	int mutex;
	fs::path mutex_path;
	Config config;
	fs::path get_mutex_name(const fs::path &config_path);
	int lock_mutex(void);
	void unlock_mutex(void);
	void open_db(void);
protected:
	bool hasCache = false;
	Tier *cache = NULL;
	std::list<Tier> tiers;
	std::list<File> files;
public:
	TierEngine(const fs::path &config_path);
	~TierEngine(void);
	fs::path get_mountpoint(void);
	std::list<Tier> &get_tiers(void);
  Tier *tier_lookup(fs::path p);
  Config *get_config(void);
	void begin(bool daemon_mode);
	void launch_crawlers(void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr));
	void crawl(fs::path dir, Tier *tptr, void (TierEngine::*function)(fs::directory_entry &itr, Tier *tptr));
	void emplace_file(fs::directory_entry &file, Tier *tptr);
	void print_file_pins(void);
	void print_file_popularity(void);
	void sort(void);
	void simulate_tier(void);
	void move_files(void);
	void print_tier_info(void);
	void pin_files(std::string tier_name, std::vector<fs::path> &files_);
	void unpin(int optind, int argc, char *argv[]);
	void calc_popularity(void);
};
