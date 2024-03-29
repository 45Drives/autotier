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

#include "TierEngine/components/database.hpp"

#include "alert.hpp"
#include "rocksDbHelpers.hpp"

auto rocksdb_deleter = [](rocksdb::DB *db) {
	Logging::log.message("Deleting db", Logger::DEBUG);
	delete db;
};

TierEngineDatabase::TierEngineDatabase(const fs::path &config_path,
									   const ConfigOverrides &config_overrides)
	: TierEngineBase(config_path, config_overrides) {}

TierEngineDatabase::~TierEngineDatabase() {}

std::shared_ptr<rocksdb::DB> TierEngineDatabase::get_db(void) {
	if (!db_)
		open_db();
	return db_;
}

void TierEngineDatabase::open_db(void) {
	std::string db_path = (run_path_ / "db").string();
	rocksdb::Options options;
	options.create_if_missing = true;
	options.prefix_extractor.reset(l::NewPathSliceTransform());
	rocksdb::Status status;
	rocksdb::DB *db_ptr;
	status = rocksdb::DB::Open(options, db_path, &db_ptr);
	db_ = std::shared_ptr<rocksdb::DB>{ db_ptr, rocksdb_deleter };
	if (!status.ok()) {
		Logging::log.error("Failed to open RocksDB database: " + db_path);
		exit(EXIT_FAILURE);
	}
}
