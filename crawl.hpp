#pragma once

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class Tier{
public:
  long expires;
  fs::path dir;
  int id;
  Tier *higher;
  Tier *lower;
  void crawl(void (*action)());
  void tier_up(fs::path item);
  void tier_down(fs::path item);
};

extern Tier *highest_tier;
extern Tier *lowest_tier;

void launch_crawlers(void);

void crawl(const fs::path &src, void (*action)(fs::path, Tier *), Tier *tptr);

void tier_up(const fs::path item, Tier *tptr);

void tier_down(const fs::path item, Tier *tptr);

void print_md5_sum(unsigned char* md);

bool verify_copy(fs::path src, fs::path dst);
