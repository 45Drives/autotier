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

#include "openFiles.hpp"

#include <mutex>

namespace OpenFiles {
	std::mutex open_files_mt_;
	std::unordered_set<std::string> open_files_;
	/* Holds paths to all currently open files.
	 */
} // namespace OpenFiles

void OpenFiles::register_open_file(const std::string &path) {
	std::lock_guard<std::mutex> lk(open_files_mt_);
	open_files_.emplace(path);
}

void OpenFiles::release_open_file(const std::string &path) {
	std::lock_guard<std::mutex> lk(open_files_mt_);
	open_files_.erase(path);
}

bool OpenFiles::is_open(const std::string &path) {
	return open_files_.find(path) != open_files_.end();
}
