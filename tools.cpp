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

#include "tools.hpp"
#include "crawl.hpp"
#include "config.hpp"
#include <iostream>
#include <regex>
#include <vector>
#include <boost/filesystem.hpp>

bool recursive_flag_set = false;

std::regex command_list[NUM_COMMANDS] = {
  std::regex("^[Rr]un|RUN$"),
  std::regex("^[Ll]ist|LIST$"),
  std::regex("^[Pp]in|PIN$"),
  std::regex("^[Cc]onfig|CONFIG$"),
  std::regex("^[Hh]elp|HELP$"),
  std::regex("^[Uu]npin|UNPIN$")
};

int get_command_index(int argc, char *argv[]){
  if(argc < 2){ // no command
    return -1;
  }
  std::string command = argv[1];
  for(int itr = 0; itr < NUM_COMMANDS; itr++){
    if(regex_match(command,command_list[itr]))
      return itr;
  }
  return -1;
}

void parse_flags(int argc, char *argv[], fs::path &config_path){
  for(int i = 0; i < argc; i++){
    if(regex_match(argv[i],std::regex("^-c|--config$"))){
      if(++i >= argc){
        usage();
        exit(1);
      }
      config_path = fs::path(argv[i]);
    }else if(regex_match(argv[i],std::regex("^-r|--recursive$"))){
      recursive_flag_set = true;
    }
  }
}

void pin(int argc, char *argv[], TierEngine &autotier){
  if(argc < 4){
    usage();
    exit(1);
  }
  std::string tier_name = argv[2];
  std::vector<fs::path> files;
  for(int i = 3; i < argc; i++){
    files.emplace_back(argv[i]);
  }
  Log("Pinning files to " + tier_name, 2);
  autotier.pin_files(tier_name, files);
}

void unpin(int argc, char *argv[]){
  if(argc < 3){
    usage();
    exit(1);
  }
  for(int i = 2; i < argc; i++){
    fs::path temp(argv[i]);
    if(!exists(temp)){
      Log("File does not exist! " + temp.string(),0);
      continue;
    }
    if(removexattr(temp.c_str(),"user.autotier_pin")==ERR)
      Log("Error removing autotier_pin xattr on " + temp.string(),0);
  }
}

void usage(){
  std::cout << 
  "autotier usage:\n"
  "  autotier <command> <flags> [{-c|--config} </path/to/config>]\n"
  "commands:\n"
  "  run          - execute tiering of files\n"
  "  list         - list defined tiers\n"
  "  pin [-r|--recursive] <\"/path/to/file\"> {<\"tier name\">|<\"path/to/pinned/tier\">}\n"
  "               - pin file to tier using tier name in config file or full path to *tier root*\n"
  "               - if a path to a directory is passed, all top-level files will be pinned\n"
  "  config       - display current configuration\n"
  "  help         - display this message\n"
  "flags:\n"
  "  -c --config <path/to/config>\n"
  "               - override configuration file path (default /etc/autotier.conf)\n"
  "  -r --recursive\n"
  "               - only for pin command: pass directory path and all contained files will be pinned\n"
  << std::endl;
}