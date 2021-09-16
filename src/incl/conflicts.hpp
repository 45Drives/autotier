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

/**
 * @brief Check if there were any conflicts while tiering. Returns true if there were
 * conflicts. Conflicting files are placed in the string vector.
 * 
 * @param conflicts List of conflicting files returned by reference
 * @param run_path Location to place conflict log file
 * @return true There are existing conflicting files
 * @return false There are no existing conflicting files
 */
bool check_conflicts(std::vector<std::string> &conflicts, const fs::path &run_path);

/**
 * @brief Add entry to conflicts log file.
 * 
 * @param path Path to file conflict relative to mount point
 * @param run_path Location to place conflict log file
 */
void add_conflict(const std::string &path, const fs::path &run_path);
