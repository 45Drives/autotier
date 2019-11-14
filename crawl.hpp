#pragma once

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

void launch_crawlers(void);
void crawl(const fs::path &src, void (*action)(fs::path));

// thread functions
void tier_up(const fs::path item);
void tier_down(const fs::path item);
