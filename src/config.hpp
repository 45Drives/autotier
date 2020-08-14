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

#include "tier.hpp"

#include <iostream>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#define DEFAULT_CONFIG_PATH "/etc/autotier.conf"
#define ERR -1
#define DISABLED -999

enum BYTE_FORMAT {BYTES, POWTEN, POWTWO};

class Config{
private:
	void generate_config(std::fstream &file);
  /* called if config file DNE
   * prints default config options to file
   */
	bool verify(const std::list<Tier> &tiers);
  /* ensures all config options are legal
   * returns true if no errors found, false otherwise
   */
  int load_global(std::fstream &config_file, std::string &id);
  /* called by load() when [global] config header is found
   */
public:
	int log_lvl = ERR;
  /* value read from config file which may be overridden in main()
   * by CLI flags [ --verbose | --quiet ]
   */
  int byte_format = BYTES;
  /* set by CLI flag in main() - sets print format for bytes as:
   * bytes, base 1000 SI units, or base 1024 binary units
   */
	unsigned long period = ERR;
  /* number of seconds between tiering
   */
	fs::path mountpoint;
  /* path read from config file at which to mount autotier
   * can be overridden with CLI flag [ -m --mountpoint </path/to/mountpoint> ]
   */
	void load(const fs::path &config_path, std::list<Tier> &tiers);
  /* open config file at config_path, parse global and tier options,
   * populate list of tiers
   */
	void dump(std::ostream &os, const std::list<Tier> &tiers) const;
  /* print out loaded options from config file for the global section
   * and for each tier to os
   */
};

void strip_whitespace(std::string &str);
/* modifies str by first removing comments at end if they exist,
 * then removing all whitespace characters from the end and beginning of str.
 */
