/*
		Copyright (C) 2019-2020 Joshua Boudreau
		
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

#include <sstream>
#include <sqlite3.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

struct FileInfo{
	long last_atime;
	double popularity;
	fs::path relative_path;
	fs::path current_tier;
};

class Database{
private:
	sqlite3 *db;
	std::stringstream sql;
	int callback(void *param);
public:
	Database(fs::path db_path);
	~Database(void);
	int get_file_info(FileInfo &buff, const fs::path &relative_path);
	int put_file_info(const FileInfo &buff);
	int add_file(const fs::path &relative_path);
	int remove_file(const fs::path &relative_path);
};
