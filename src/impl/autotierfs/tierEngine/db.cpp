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

#include "tierEngine.hpp"
#include "rocksDbHelpers.hpp"
#include "alert.hpp"

void TierEngine::open_db(bool read_only){
	std::string db_path = (run_path_ / "db").string();
	rocksdb::Options options;
	options.create_if_missing = true;
	options.prefix_extractor.reset(l::NewPathSliceTransform());
	rocksdb::Status status;
	if(read_only)
		status = rocksdb::DB::OpenForReadOnly(options, db_path, &db_);
	else
		status = rocksdb::DB::Open(options, db_path, &db_);
	if(!status.ok()){
		Logging::log.error("Failed to open RocksDB database: " + db_path);
		exit(EXIT_FAILURE);
	}
}
