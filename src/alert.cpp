/*
    Copyright (C) 2019-2020 Joshua Boudreau
    
    This file is part of autotier.

    autotier is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    autotier is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with autotier.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "alert.hpp"
#include "config.hpp"
#include <iostream>

int log_lvl;

std::string errors[NUM_ERRORS] = {
  "Error loading configuration file.",
  "Tier directory does not exist. Please check config.",
  "First tier must be specified with '[<tier name>]' before any other config options.",
  "No tiers defined in config file.",
  "Only one tier defined in config file, two or more are needed.",
  "WATERMARK must be a positive integer between 0 and 100.",
  "Error setting extended attribute.",
  "Error reading log level.",
  "Error reading tier period.",
  "Error constructing mutex file path."
};

void error(enum Error error){
  std::cerr << errors[error] << std::endl;
}

void Log(std::string msg, int lvl){
  if(log_lvl < lvl) return;
  std::cout << msg << std::endl;
}
