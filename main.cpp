/*
    Copyright (C) 2019 Joshua Boudreau
    
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

#include "config.hpp"
#include "crawl.hpp"
#include <iostream>

inline bool config_passed(int argc, char *argv[]){
  return (argc >= 3 && (strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--config") == 0));
}

int main(int argc, char *argv[]){
  fs::path config_path = (config_passed(argc, argv))? argv[2] : DEFAULT_CONFIG_PATH;
  TierEngine autotier(config_path);
  autotier.launch_crawlers();
  return 0;
}
