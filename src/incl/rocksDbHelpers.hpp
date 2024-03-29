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

#include <mutex>
#include <rocksdb/db.h>
#include <rocksdb/slice_transform.h>
#include <string>

/**
 * @brief Local namespace
 *
 */
namespace l {
	/**
	 * @brief Used to make rocksdb lookups faster, groups keys by first dir in path
	 *
	 */
	class PathSliceTransform : public ::rocksdb::SliceTransform {
	public:
		const char *Name() const {
			return "Path Slice Transform";
		}
		::rocksdb::Slice Transform(const ::rocksdb::Slice &key) const {
			std::string key_str = key.ToString();
			return ::rocksdb::Slice(key_str.substr(0, key_str.find('/')));
		}
		bool InDomain(const ::rocksdb::Slice &key) const {
			return key.ToString().find('/') != std::string::npos;
		}
		bool InRange(const ::rocksdb::Slice & /*dst*/) const {
			return false;
		}
		bool FullLengthEnabled(size_t * /*len*/) const {
			return false;
		}
		virtual bool SameResultWhenAppended(const ::rocksdb::Slice & /*prefix*/) const {
			return false;
		}
	};

	/**
	 * @brief Construct new PathSliceTransform on heap and return pointer
	 *
	 * @return const PathSliceTransform*
	 */
	extern const PathSliceTransform *NewPathSliceTransform();

	/**
	 * @brief rocksdb namespace inside l:: namespace to hold mutex
	 *
	 */
	namespace rocksdb {
		/**
		 * @brief Mutex lock to synchronize updating of RocksDB database
		 *
		 */
		extern std::mutex global_lock_;
	} // namespace rocksdb
} // namespace l
