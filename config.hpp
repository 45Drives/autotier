#pragma once

#include <iostream>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#define DEFAULT_CONFIG_PATH "/etc/autotier.conf"
#define ERR -1
#define DISABLED -999

class Config{
private:
  void generate_config(std::fstream &file);
  bool verify(void);
public:
  void load(const fs::path &config_path);
  void dump(std::ostream &os) const;
};

void discard_comments(std::string &str);

extern Config config;
