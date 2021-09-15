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

#include "alert.hpp"

#include <45d/config/ConfigParser.hpp>
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
	ConfigOverride<Logger::log_level_t> log_level_override;
};

class Tier;

/**
 * @brief Configuration class
 * 
 */
class Config : public ffd::ConfigParser{
public:
	enum LogLevel { NONE, NORMAL, DEBUG }; ///< Enum for log level
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
	size_t copy_buff_sz(void) const;
	/* Get copy_buff_sz_.
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
	void dump(const std::list<Tier> &tiers, std::stringstream &ss) const;
	/* print out loaded options from config file for the global section
	 * and for each tier
	 */
private:
	/**
	 * @brief value read from config file which may be overridden in main()
	 * by CLI flags [ --verbose | --quiet ]
	 * 
	 */
	Logger::log_level_t log_level_;
	/**
	 * @brief Size of buffer for copying a file from one tier to another. 1MiB default.
	 * 
	 */
	size_t copy_buff_sz_;
	/**
	 * @brief Polling period to check whether to send new files in seconds.
	 * 
	 */
	std::chrono::seconds tier_period_s_;
	/**
	 * @brief If true, tiering only happens once per period; if false, writing
	 * into tier that is over quota will trigger file tiering.
	 * 
	 */
	bool strict_period_;
	/**
	 * @brief Path to database and FIFOs. Default location: /var/lib/autotier
	 * 
	 */
	fs::path run_path_;
	/**
	 * @brief parse global and tier options, populate list of tiers
	 * 
	 * @param config_path Path to config file
	 * @param tiers List of tiers found in config
	 * @param config_overrides Overridden config values from CLI flags
	 */
	void load_config(const fs::path &config_path, std::list<Tier> &tiers, const ConfigOverrides &config_overrides);
};

/**
 * @brief When config file DNE, this is called to create and initialize one.
 * 
 * @param config_path Path to config file
 */
void init_config_file(const fs::path &config_path);
