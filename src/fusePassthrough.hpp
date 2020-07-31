#pragma once

#define FUSE_USE_VERSION 30

#include "tier.hpp"
#include <list>
#include <sqlite3.h>
#include <fuse.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class FusePassthrough{
private:
	void open_db(void);
public:
	FusePassthrough(std::list<Tier> &tiers_);
	~FusePassthrough(void);
	int mount_fs(fs::path mountpoint);
};

