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

#include "alert.hpp"
#include "file.hpp"
#include <queue>
#include <rocksdb/db.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class File;

class Tier{
private:
	int watermark_ = -1;
	uintmax_t watermark_bytes_;
	uintmax_t capacity_;
	uintmax_t usage_;
	std::string id_;
	fs::path path_;
	void get_capacity_and_usage(void);
	void copy_ownership_and_perms(const fs::path &old_path, const fs::path &new_path) const;
	void copy_file(const fs::path &old_path, const fs::path &new_path) const;
	std::vector<File *> incoming_files_;
public:
	Tier(std::string id);
	~Tier() = default;
	void add_file_size(uintmax_t size);
	void subtract_file_size(uintmax_t size);
	void watermark(int watermark);
	void path(const fs::path &path);
	const std::string &id(void) const;
	int watermark(void) const;
	uintmax_t watermark_bytes(void) const;
	const fs::path &path(void) const;
	bool full_test(const File &file) const;
	void enqueue_file_ptr(File *fptr);
	void transfer_files(void);
	double usage_percent(void) const;
	uintmax_t usage_bytes(void) const;
};
