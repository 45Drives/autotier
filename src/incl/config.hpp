/*
 *    Copyright (C) 2019-2021 Joshua Boudreau <jboudreau@45drives.com>
 *    
 *    This file is part of autotier.
 * 
 *    autotier is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 * 
 *    autotier is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 * 
 *    You should have received a copy of the GNU General Public License
 *    along with autotier.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <fstream>
#include <chrono>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#define DEFAULT_CONFIG_PATH "/etc/autotier.conf"
#define TIER_PERIOD_DISBLED -1
#define LOG_LEVEL_NOT_SET -1

template<class T>
/* ConfigOverride is used with command line flags
 * to allow for easy overridding of configuration values.
 */
class ConfigOverride{
private:
	T value_;
	/* The value to override with.
	 */
	bool overridden_;
	/* Whether or not the field is overridden.
	 * Since ConfigOverride objects are constructed in main()
	 * without a valuem, overridden_ will be set to false. If
	 * the value is updated through copying a newly constructed
	 * ConfigOverride object with a value, this will be true.
	 */
public:
	ConfigOverride(void) : value_() {
		overridden_ = false;
	}
	ConfigOverride(const T &value_passed) : value_(value_passed){
		overridden_ = true;
	}
	~ConfigOverride(void) = default;
	const T &value(void) const{
		return value_;
	}
	const bool &overridden(void) const{
		return overridden_;
	}
};

struct ConfigOverrides{
	ConfigOverride<int> log_level_override;
};

class Tier;

class Config{
private:
	int log_level_ = LOG_LEVEL_NOT_SET;
	/* value read from config file which may be overridden in main()
	 * by CLI flags [ --verbose | --quiet ]
	 */
	std::chrono::seconds tier_period_s_ = std::chrono::seconds(TIER_PERIOD_DISBLED);
	/* Polling period to check whether to send new files in seconds.
	 */
	bool strict_period_ = false;
	/* If true, tiering only happens once per period; if false, writing
	 * into tier that is over quota will trigger file tiering.
	 */
	fs::path run_path_ = "/var/lib/autotier";
	/* Path to database and FIFOs. Default location: /var/lib/autotier
	 */
	int load_global(std::ifstream &config_file, std::string &id, bool & errors);
	/* called by load() when [global] config header is found
	 */
	void init_config_file(const fs::path &config_path) const;
	/* When config file DNE, this is called to create and initialize one.
	 */
public:
	Config(const fs::path &config_path, std::list<Tier> &tiers, const ConfigOverrides &config_overrides);
	/* open config file at config_path, parse global and tier options,
	 * populate list of tiers
	 */
	Config(const fs::path &config_path, const ConfigOverrides &config_overrides);
	/* Only read global.
	 */
	~Config() = default;
	/* Default destructor.
	 */
	std::chrono::seconds tier_period_s(void) const;
	/* Get tier_period_s_.
	 */
	bool strict_period(void) const;
	/* Return true if strict_period_ == 1, else 0.
	 */
	fs::path run_path(void) const;
	/* Get run_path_.
	 */
	void dump(const std::list<Tier> &tiers) const;
	/* print out loaded options from config file for the global section
	 * and for each tier
	 */
};
