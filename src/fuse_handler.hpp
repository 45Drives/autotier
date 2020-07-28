#pragma once
#ifdef USE_FUSE
#include <list>
#include "tier.hpp"

extern std::list<Tier> *tiers_ptr;

extern "C" int mount_autotier(int argc, char **argv);
#endif
