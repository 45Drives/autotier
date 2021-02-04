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

#include "alert.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>

extern "C"{
	#include <syslog.h>
	#include <fuse_log.h>
}

namespace Logging{
	Logger log(1);
}

Logger::Logger(int log_level, output_t output){
	log_level_ = log_level;
	output_ = output;
	if(output_ == SYSLOG){
		openlog("autotier", LOG_USER, LOG_USER);
	}
}

Logger::~Logger(void){
	if(output_ == SYSLOG)
		closelog();
}

void Logger::message(const std::string &msg, int lvl) const{
	if(log_level_ >= lvl){
		switch(output_){
			case STD:
				std::cout << msg << std::endl;
				break;
			case SYSLOG:
				syslog(LOG_INFO, "%s", msg.c_str());
				break;
			case FUSELOG:
				fuse_log(FUSE_LOG_INFO, "%s", msg.c_str());
				break;
		}
	}
}

void Logger::warning(const std::string &msg) const{
	switch(output_){
		case STD:
			std::cerr << "Warning: " << msg << std::endl;
			break;
		case SYSLOG:
			syslog(LOG_WARNING, "%s", msg.c_str());
			break;
		case FUSELOG:
			fuse_log(FUSE_LOG_WARNING, "%s", msg.c_str());
			break;
	}
}

void Logger::error(const std::string &msg, bool exit_) const{
	switch(output_){
		case STD:
			std::cerr << "Error: " << msg << std::endl;
			break;
		case SYSLOG:
			syslog(LOG_ALERT, "%s", msg.c_str());
			break;
		case FUSELOG:
			fuse_log(FUSE_LOG_ALERT, "%s", msg.c_str());
			break;
	}
	if(exit_) exit(EXIT_FAILURE);
}

void Logger::set_level(int log_level){
	log_level_ = log_level;
}

void Logger::set_output(output_t output){
	output_ = output;
}

#define N_INDEX 9
std::string Logger::format_bytes(uintmax_t bytes) const{
	if(bytes == 0) return "0 B";
	std::stringstream formatted_ss;
	std::string units[N_INDEX] = {" B", " KiB", " MiB", " GiB", " TiB", " PiB", " EiB", " ZiB", " YiB"};
	int i = std::min(int(log(bytes) / log(1024.0)), N_INDEX - 1);
	double p = pow(1024.0, i);
	double formatted = double(bytes) / p;
	formatted_ss << std::fixed << std::setprecision(2) << formatted << units[i];
	return formatted_ss.str();
}
