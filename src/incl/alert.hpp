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
#include <cstdint>

/**
 * @brief Class to print logs to either stdout/stderr or the syslog.
 *
 */
class Logger {
public:
	/**
	 * @brief Whether to print to stdout/stderr or the syslog
	 *
	 */
	enum output_t {
		STD,   ///< Print to stdout/stderr
		SYSLOG ///< Print to syslog
	};
	enum log_level_t { NONE, NORMAL, DEBUG };
	/**
	 * @brief Construct a new Logger object
	 *
	 * @param log_level Assigned to log_level_
	 * @param output Where to output, default is STD
	 */
	explicit Logger(log_level_t log_level, output_t output = STD);
	/**
	 * @brief Destroy the Logger object
	 * Close syslog if output_ == SYSLOG
	 *
	 */
	~Logger(void);
	/**
	 * @brief Print message if lvl >= log_level_.
	 * Use this for regular informational log messages.
	 *
	 * @param msg String to print
	 * @param lvl Log level to test against
	 */
	void message(const std::string &msg, log_level_t lvl) const;
	/**
	 * @brief Print message (to stderr if output_ == STD) prepended with "Warning: ".
	 * Use this for non-fatal errors.
	 *
	 * @param msg String to print
	 */
	void warning(const std::string &msg) const;
	/**
	 * @brief Print message (to stderr if output_ == STD) prepended with "Error: ".
	 * Use this for fatal errors.
	 *
	 * @param msg
	 */
	void error(const std::string &msg) const;
	/**
	 * @brief Set the log_level_ member to log_level
	 *
	 * @param log_level_ New log level
	 */
	void set_level(log_level_t log_level);
	/**
	 * @brief Set which type of logging to do.
	 * If switching from STD to SYSLOG, open the log.
	 * If switching from SYSLOG to STD, closes log.
	 *
	 * @param output
	 */
	void set_output(output_t output);
	/**
	 * @brief Return bytes as string in base-1024 SI units.
	 * Deprecated in favour of ffd::Bytes::get_str() from <a
	 * href="github.com/45drives/lib45d">lib45d</a>.
	 *
	 * @param bytes
	 * @return std::string
	 */
	std::string format_bytes(uintmax_t bytes) const;
	/**
	 * @brief Return bytes as double with unit returned
	 * by reference in base-1024 SI units.
	 * Only use this over ffd::Bytes::get_str() from <a href="github.com/45drives/lib45d">lib45d</a>
	 * if you're formatting a table and need the unit separate.
	 *
	 * @param bytes
	 * @param unit
	 * @return double
	 */
	double format_bytes(uintmax_t bytes, std::string &unit) const;
private:
	/**
	 * @brief Value from config file. Each log message
	 * passes a log level to check against this number.
	 * If the message's level is higher, it is printed.
	 *
	 */
	log_level_t log_level_;
	output_t output_; ///< Whether to output to stdout (STD) or syslog (SYSLOG)
};

/**
 * @brief Namespace for containing a global instance of a Logger object.
 *
 */
namespace Logging {
	/**
	 * @brief Global Logger object. Use Logging::log.<method> in source
	 * files including this header.
	 *
	 */
	extern Logger log;
} // namespace Logging
