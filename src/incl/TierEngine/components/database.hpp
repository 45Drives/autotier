/*
 *    Copyright (C) 2019-2021 Joshua Boudreau <jboudreau@45drives.com>
 *    
 *    This file is part of autotier.
 * 
 *    autotier is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 * 
 *    autotier is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 * 
 *    You should have received a copy of the GNU General Public License
 *    along with autotier.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "base.hpp"

class TierEngineDatabase : virtual public TierEngineBase {
public:
    TierEngineDatabase(const fs::path &config_path, const ConfigOverrides &config_overrides);
    ~TierEngineDatabase(void);
	rocksdb::DB *get_db(void);
	/* Return db_, used in fusePassthrough.cpp for getting file
	 * metadata.
	 */
private:
	void open_db(void);
	/* Opens RocksDB database.
	 */
};
