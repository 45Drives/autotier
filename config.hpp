#pragma once

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#define DEFAULT_CONFIG_PATH "/etc/autotier.conf"
#define ERR -1

class Config{
private:
  fs::path fast_tier;
  fs::path slow_tier;
  long threshold;
  void generate_config(std::fstream &file);
  bool verify(void);
public:
  void load(const fs::path &config_path);
  const fs::path &get_fast_tier(void) const;
  const fs::path &get_slow_tier(void) const;
  long get_threshold(void) const;
};

extern Config config;
