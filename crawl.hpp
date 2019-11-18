#pragma once

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class Tier{
public:
  long expires;
  fs::path dir;
  std::string id;
  Tier *higher;
  Tier *lower;
  void crawl(fs::path dir, void (*action)(fs::path, Tier *));
};

extern Tier *highest_tier;
extern Tier *lowest_tier;

void launch_crawlers(void);

static void tier_up(fs::path item, Tier *tptr);
static void tier_down(fs::path item, Tier *tptr);

void print_md5_sum(unsigned char* md);

bool verify_copy(fs::path src, fs::path dst);
