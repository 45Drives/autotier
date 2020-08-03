/*
		Copyright (C) 2019-2020 Joshua Boudreau
		
		This file is part of autotier.

		autotier is free software: you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation, either version 3 of the License, or
		(at your option) any later version.

		autotier is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with autotier.	If not, see <https://www.gnu.org/licenses/>.
*/

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

