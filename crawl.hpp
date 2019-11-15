#pragma once

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

void launch_crawlers(void);

void crawl(const fs::path &src, void (*action)(fs::path));

void tier_up(const fs::path item);

void tier_down(const fs::path item);

void print_md5_sum(unsigned char* md);

bool verify_copy(fs::path src, fs::path dst);
