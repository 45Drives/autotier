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

static void tier_up(fs::path from_here, Tier *tptr);
static void tier_down(fs::path from_here, Tier *tptr);

void copy_ownership_and_perms(const fs::path &src, const fs::path &dst);

bool verify_copy(const fs::path &src, const fs::path &dst);

struct utimbuf last_times(const fs::path &file);
