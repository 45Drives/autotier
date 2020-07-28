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

#include "db.hpp"
#include "alert.hpp"

#include <iostream>
#include <sstream>
#include <sqlite3.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#define DB_PATH "/run/autotier/db.sqlite"

Database::Database(fs::path db_path){
	int res = sqlite3_open(DB_PATH, &db);
	if(res){
		error(OPEN_DB);
		std::cerr << sqlite3_errmsg(db);
		exit(res);
	}else{
		Log("Opened database successfully", 2);
	}
	
	sql.str(
		"IF NOT EXISTS (SELECT * FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = N'Files')"\
		"BEGIN"\
		"CREATE TABLE Files("\
		"ID INT PRIMARY KEY NOT NULL,"\
		"RELATIVE_PATH TEXT NOT NULL,"\
		"CURRENT_TIER TEXT NOT NULL,"\
		"POPULARITY REAL NOT NULL,"\
		"LAST_ACCESS INT NOT NULL);"\
		"END"
	);
	
}

Database::~Database(void){
	sqlite3_close(db);
}

int Database::get_file_info(FileInfo &buff, const fs::path &relative_path){
	
}

int Database::put_file_info(const FileInfo &buff){
	
}

int Database::add_file(const fs::path &relative_path){
	
}

int Database::remove_file(const fs::path &relative_path){
	
}



