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

/**
 * @brief TierEngine component for dealing with the rocksdb database.
 *
 */
class TierEngineDatabase : virtual public TierEngineBase {
public:
	/**
	 * @brief Construct a new Tier Engine Database object
	 * Calls open_db().
	 *
	 * @param config_path
	 * @param config_overrides
	 */
	TierEngineDatabase(const fs::path &config_path, const ConfigOverrides &config_overrides);
	/**
	 * @brief Destroy the Tier Engine Database object
	 * Deletes (closes) the rocksdb database.
	 *
	 */
	~TierEngineDatabase(void);
	/**
	 * @brief Get the database pointer.
	 * Used in fusePassthrough.cpp for getting file.
	 *
	 * @return rocksdb::DB* Pointer to database
	 */
	rocksdb::DB *get_db(void);
private:
	/**
	 * @brief Opens RocksDB database.
	 * Calls TierEngineTiering::exit() (virtual TierEngineBase method) if it fails.
	 *
	 */
	void open_db(void);
};
