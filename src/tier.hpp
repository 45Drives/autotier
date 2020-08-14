/*
		Copyright (C) 2019-2020 Joshua Boudreau <jboudreau@45drives.com>
		
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

#include "file.hpp"

class File;

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class Tier{
public:
	unsigned long long watermark_bytes;
  /* number of bytes at which to stop filling tier
   */
  unsigned long long pinned_files_size;
  /* size of files pinned to tier in bytes
   */
	int watermark;
  /* percent of tier capacity at which to stop filling tier
   */
	fs::path dir;
  /* backend path to tier
   */
	std::string id;
  /* name of tier read in from configuration file
   */
	std::list<File *> incoming_files;
  /* list of files to be moved into tier
   */
	Tier(std::string id_){
		id = id_;
	}
	unsigned long long get_capacity();
  /* returns capacity of tier in bytes
   */
	unsigned long long get_usage();
  /* returns current tier fullness in bytes
   */
	void cleanup(void){
		incoming_files.erase(incoming_files.begin(), incoming_files.end());
	}
  /* empties list at end of tiering cycle
   */
};
