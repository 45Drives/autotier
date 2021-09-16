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

#include <unordered_set>
#include <string>

/**
 * @brief Keeping track of open files
 * 
 */
namespace OpenFiles{
	/**
	 * @brief Insert path into OpenFiles::open_files_.
	 * 
	 * @param path 
	 */
	void register_open_file(const std::string &path);
	/**
	 * @brief Erase path from OpenFiles::open_files_.
	 * 
	 * @param path 
	 */
	void release_open_file(const std::string &path);
	/**
	 * @brief Return true if path is contained in OpenFiles::open_files_.
	 * 
	 * @param path 
	 * @return true 
	 * @return false 
	 */
	bool is_open(const std::string &path);
}
