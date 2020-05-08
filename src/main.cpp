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

#include "config.hpp"
#include "tierEngine.hpp"
#include "tools.hpp"

inline bool config_passed(int argc, char *argv[]){
  return (argc >= 3 && (strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--config") == 0));
}

int main(int argc, char *argv[]){
  bool daemon_mode = false;
  fs::path config_path = DEFAULT_CONFIG_PATH;
  parse_flags(argc, argv, config_path);
  TierEngine autotier(config_path);
  
  switch(get_command_index(argc, argv)){
  case RUN:
    daemon_mode = true;
  case ONESHOT:
    autotier.begin(daemon_mode);
    break;
  case STATUS:
    autotier.print_tier_info();
    break;
  case PIN:
    pin(argc, argv, autotier);
    break;
  case CONFIG:
    std::cout << "Config file: (" << config_path.string() << ")" << std::endl;
    std::cout << std::ifstream(config_path.string()).rdbuf();
    break;
  case UNPIN:
    unpin(argc, argv);
    break;
  case LPIN:
    std::cout << "Pinned files:" << std::endl;
    autotier.launch_crawlers(&TierEngine::print_file_pin);
    break;
  case LPOP:
    autotier.launch_crawlers(&TierEngine::emplace_file);
    autotier.sort();
    autotier.print_file_popularity();
    break;
  case HELP:
  default:
    usage();
    exit(1);
    break;
  }
  return 0;
}


