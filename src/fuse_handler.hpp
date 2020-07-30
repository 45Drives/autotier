#pragma once
#ifdef USE_FUSE
#include "tier.hpp"

#include <list>
#include <sqlite3.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class FusePassthrough{
private:
	sqlite3 *db;
	std::list<Tier> *tiers_ptr;
	void open_db(void);
public:
	FusePassthrough(std::list<Tier> *tp_);
	~FusePassthrough(void);
	void mount(fs::path mountpoint);
};

extern "C" int mount_autotier(int argc, char **argv);
#endif
