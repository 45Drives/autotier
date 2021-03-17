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

#include <string>

enum output_t {STD, SYSLOG};

class Logger{
private:
	int log_level_;
	/* Value from config file. Each log message
	 * passes a log level to check against this number.
	 * If the message's level is higher, it is printed.
	 */
	output_t output_;
	/* Whether to output to stdout (STD) or syslog (SYSLOG)
	 */
public:
	explicit Logger(int log_level, output_t output = STD);
	/* Constructs Logger, assigning log_level to the
	 * internal log_level_. Defaults to stdout output.
	 * Syslog output set by passing SYSLOG for output.
	 */
	~Logger(void);
	/* Close syslog if output_ == SYSLOG
	 */
	void message(const std::string &msg, int lvl) const;
	/* Print message if lvl >= log_level_.
	 * Use this for regular informational log messages.
	 */
	void warning(const std::string &msg) const;
	/* Print message (to stderr if output_ == STD) prepended with "Warning: ".
	 * Use this for non-fatal errors.
	 */
	void error(const std::string &msg, bool exit_ = true) const;
	/* Print message (to stderr if output_ == STD) prepended with "Error: ".
	 * Exits with EXIT_FAILURE if exit_ is left as true.
	 * exit_ is used to delay exiting until later if there are
	 * multiple errors to print.
	 */
	void set_level(int log_level_);
	/* Set log level.
	 */
	void set_output(output_t output);
	/* Set which type of logging to do.
	 */
	std::string format_bytes(uintmax_t bytes) const;
	/* Return bytes as string in base-1024 SI units.
	 */
	double format_bytes(uintmax_t bytes, std::string &unit) const;
	/* Return bytes as double with unit returned
	 * by reference in base-1024 SI units.
	 */
};

namespace Logging{
	extern Logger log;
	/* Global Logger object. Use Logging::log.<method> in source
	 * files including this header.
	 */
}
