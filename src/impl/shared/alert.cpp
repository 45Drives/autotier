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
}

namespace Logging{
	Logger log(Logger::log_level_t::NORMAL);
}

Logger::Logger(log_level_t log_level, output_t output){
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

void Logger::message(const std::string &msg, log_level_t lvl) const{
	if(log_level_ >= lvl){
		switch(output_){
			case STD:
				std::cout << msg << std::endl;
				break;
			case SYSLOG:
				syslog(LOG_INFO, "%s", msg.c_str());
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
	}
}

void Logger::error(const std::string &msg) const{
	switch(output_){
		case STD:
			std::cerr << "Error: " << msg << std::endl;
			break;
		case SYSLOG:
			syslog(LOG_ALERT, "%s", msg.c_str());
			break;
	}
}

void Logger::set_level(log_level_t log_level){
	log_level_ = log_level;
}

void Logger::set_output(output_t output){
	if (output_ == output)
		return;
	if (output == Logger::output_t::SYSLOG) {
		openlog("autotier", LOG_USER, LOG_USER);
	} else if (output == Logger::output_t::STD) {
		closelog();
	}
	output_ = output;
}

#define N_INDEX 9
const char *units[N_INDEX] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};

std::string Logger::format_bytes(uintmax_t bytes) const{
	if(bytes == 0) return "0 B";
	std::stringstream formatted_ss;
	int i = std::min(int(log(bytes) / log(1024.0)), N_INDEX - 1);
	double p = pow(1024.0, i);
	double formatted = double(bytes) / p;
	formatted_ss << std::fixed << std::setprecision(2) << formatted << " " << units[i];
	return formatted_ss.str();
}

double Logger::format_bytes(uintmax_t bytes, std::string &unit) const{
	if(bytes == 0){
		unit = units[0];
		return 0.0;
	}
	int i = std::min(int(log(bytes) / log(1024.0)), N_INDEX - 1);
	double p = pow(1024.0, i);
	double formatted = double(bytes) / p;
	unit = units[i];
	return formatted;
}
