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

#include "file.hpp"
#include "alert.hpp"

#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

File::File(fs::path path_, Tier *tptr, sqlite3 *db_){
	db = db_;
	rel_path = (path_.is_absolute())? fs::relative(path_, fs::path("/")) : path_;
	ID = std::hash<std::string>{}(rel_path.string());
	get_info(db);
	char strbuff[BUFF_SZ];
	ssize_t attr_len;
	current_tier = tptr->dir;
	new_path = old_path = current_tier / path_;
	old_tier = tptr;
	struct stat info;
	stat(old_path.c_str(), &info);
	size = (long)info.st_size;
	times.actime = info.st_atime;
	times.modtime = info.st_mtime;
	last_atime = times.actime;
}

File::File(fs::path path_, sqlite3 *db_){
	db = db_;
	rel_path = (path_.is_absolute())? fs::relative(path_, fs::path("/")) : path_;
	ID = std::hash<std::string>{}(rel_path.string());
	get_info(db);
	char strbuff[BUFF_SZ];
	ssize_t attr_len;
	new_path = old_path = current_tier / path_;
	old_tier = NULL;
	struct stat info;
	stat(old_path.c_str(), &info);
	size = (long)info.st_size;
	times.actime = info.st_atime;
	times.modtime = info.st_mtime;
	get_info(db);
	last_atime = times.actime;
}

File::~File(){
	put_info(db);
}

void File::log_movement(){
	Log("OldPath: " + old_path.string(),3);
	Log("NewPath: " + new_path.string(),3);
	Log("UserPin: " + pinned_to.string(),3);
	Log("",3);
}

void File::move(){
	if(old_path == new_path) return;
	if(is_open()){
		std::cerr << "File is open by another process." << std::endl;
		new_path = old_path;
		return;
	}
	if(!is_directory(new_path.parent_path()))
		create_directories(new_path.parent_path());
	Log("Copying " + old_path.string() + " to " + new_path.string(),2);
	bool copy_success = true;
	try{
		copy_file(old_path, new_path); // move item to slow tier
	}catch(boost::filesystem::filesystem_error const & e){
		copy_success = false;
		std::cerr << "Copy failed: " << e.what() << std::endl;
		if(e.code() == boost::system::errc::file_exists){
			std::cerr << "User intervention required to delete duplicate file" << std::endl;
		}else if(e.code() == boost::system::errc::no_such_file_or_directory){
			deleted = true;
			std::cerr << "No action required." << std::endl;
		}
	}
	if(copy_success){
		copy_ownership_and_perms();
		Log("Copy succeeded.\n",2);
		remove(old_path);
	}
	utime(new_path.c_str(), &times); // overwrite mtime and atime with previous times
}

void File::cache(void){
	if(old_path == new_path) return;
}

void File::uncache(void){
	if(old_path == new_path) return;
}

void File::copy_ownership_and_perms(){
	struct stat info;
	stat(old_path.c_str(), &info);
	chown(new_path.c_str(), info.st_uid, info.st_gid);
	chmod(new_path.c_str(), info.st_mode);
}

void File::calc_popularity(){
	/* popularity is moving average of the inverse of
	 * (now - last access time)
	 */
	double diff = time(NULL) - last_atime;
	popularity = MULTIPLIER / (DAMPING * (diff + 1.0))
						 + (1.0 - 1.0 / DAMPING) * popularity;
}

bool File::is_open(void){
	pid_t pid;
	int status;
	int fd;
	int stdout_copy;
	int stderr_copy;
	
	pid = fork();
	switch(pid){
	case -1:
		std::cerr << "Error forking while checking if file is open!" << std::endl;
		break;
	case 0:
		// child
		pid = getpid();
		
		// redirect output to /dev/null
		stdout_copy = dup(STDOUT_FILENO);
		stderr_copy = dup(STDERR_FILENO);
		fd = open("/dev/null", O_WRONLY);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		close(fd);
		
		execlp("lsof", "lsof", old_path.c_str(), (char *)NULL);
		
		// undo redirect
		dup2(stdout_copy, STDOUT_FILENO);
		close(stdout_copy);
		dup2(stderr_copy, STDERR_FILENO);
		close(stderr_copy);
		
		std::cerr << "Error executing lsof! Is it installed?" << std::endl;
		exit(127);
	default:
		// parent
		if((waitpid(pid, &status, 0)) < 0)
			std::cerr << "Error waiting for lsof to exit." << std::endl;
		break;
	}
	
	if(WIFEXITED(status)){
		switch(WEXITSTATUS(status)){
		case 0:
			return true;
		case 1:
			return false;
		default:
			std::cerr << "Unexpected lsof exit status! Exit status: " << WEXITSTATUS(status) << std::endl;
			return false;
		}
	}else{
		std::cerr << "Error reading lsof exit status!" << std::endl;
		return false;
	}
}

static int c_callback_file(void *param, int count, char *data[], char *cols[]){
	File *file = reinterpret_cast<File *>(param);
	return file->callback(count, data, cols);
}

int File::get_info(sqlite3 *db){
	char *errMsg = 0;
	
	std::string sql =
	"SELECT * FROM Files WHERE ID='" + std::to_string(this->ID) + "';";
	
	int res = sqlite3_exec(db, sql.c_str(), c_callback_file, this, &errMsg);
	
	if(res != SQLITE_OK){
		std::cerr << "SQL Error: " << errMsg;
		sqlite3_free(errMsg);
	}
	return res;
}

int File::put_info(sqlite3 *db){
	char *errMsg = 0;
	
	std::string sql =
	"REPLACE INTO Files (ID, RELATIVE_PATH, CURRENT_TIER, PIN, POPULARITY, LAST_ACCESS)"
	"VALUES("
		+ std::to_string(this->ID) + ","
		"'" + this->rel_path.string() + "',"
		"'" + this->current_tier.string() + "',"
		"'" + this->pinned_to.string() + "',"
		+ std::to_string(this->popularity) + ","
		+ std::to_string(this->last_atime) +
	");";
	int res = sqlite3_exec(db, sql.c_str(), NULL, 0, &errMsg);
	
	if(res != SQLITE_OK){
		std::cerr << "SQL Error: " << sqlite3_errmsg(db);
		sqlite3_free(errMsg);
	}
	return res;
}

int File::callback(int count, char *data[], char *cols[]){
	// process returned values
	for(int i = 0; i < count; i++){
		//std::cout << "col: " << cols[i] << " data: " << data[i] << std::endl;
		if(data[i] && strcmp(cols[i], "CURRENT_TIER") == 0)
			this->current_tier = fs::path(data[i]);
		else if(data[i] && strcmp(cols[i], "PIN") == 0 && strlen(data[i]))
			this->pinned_to = fs::path(data[i]);
		else if(data[i] && strcmp(cols[i], "POPULARITY") == 0)
			this->popularity = std::stod(data[i]);
		else if(data[i] && strcmp(cols[i], "LAST_ACCESS") == 0)
			this->last_atime = std::stol(data[i]);
	}
	return 0;
}
