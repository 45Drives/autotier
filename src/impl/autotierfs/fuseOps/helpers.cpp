/*
 *    Copyright (C) 2019-2021 Joshua Boudreau <jboudreau@45drives.com>
 * 
 *    Original FUSE passthrough_fh.c authors:
 *    Copyright (C) 2001-2007 Miklos Szeredi <miklos@szeredi.hu>
 *    Copyright (C) 2011      Sebastian Pipping <sebastian@pipping.org>
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

#include "fuseOps.hpp"
#include "tier.hpp"
#include "rocksDbHelpers.hpp"

namespace l{
	int is_directory(const fs::path &relative_path){
		FusePriv *priv = (FusePriv *)fuse_get_context()->private_data;
		fs::file_status status;
		try{
			status = fs::symlink_status(priv->tiers_.front()->path() / relative_path);
		}catch(const boost::system::error_code &ec){
			errno = ec.value();
			return -1;
		}
		return fs::is_directory(status);
	}
	
	Tier *fullpath_to_tier(fs::path fullpath){
		FusePriv *priv = (FusePriv *)fuse_get_context()->private_data;
		for(Tier *tptr : priv->tiers_){
			if(std::equal(tptr->path().string().begin(), tptr->path().string().end(), fullpath.string().begin())){
				return tptr;
			}
		}
		return nullptr;
	}
	
	intmax_t file_size(int fd){
		struct stat st = {};
		int res = fstat(fd, &st);
		if(res == -1)
			return res;
		return st.st_size;
	}
	
	intmax_t file_size(const fs::path &path){
		struct stat st = {};
		int res = lstat(path.c_str(), &st);
		if(res == -1)
			return res;
		return st.st_size;
	}
	
	void update_keys_in_directory(
		std::string old_directory,
		std::string new_directory,
		::rocksdb::DB *db
	){
		// Remove leading /.
		if(old_directory.front() == '/'){
			old_directory = old_directory.substr(1, std::string::npos);
		}
		if(new_directory.front() == '/'){
			new_directory = new_directory.substr(1, std::string::npos);
		}
		
		/* Ensure that only paths containing exclusively the changed directory are updated.
		 * EX: subdir and subdir2 exist. If subdir is updated, test should check for "subdir/"
		 * to avoid changing subdir2 aswell.
		 */
		if(old_directory.back() != '/'){
			old_directory += '/';
		}
		if(new_directory.back() != '/'){
			new_directory += '/';
		}
		
		// Batch changes to atomically update keys
		::rocksdb::WriteBatch batch;
		
		::rocksdb::ReadOptions read_options;
		::rocksdb::Iterator *itr = db->NewIterator(read_options);
		for(itr->Seek(old_directory); itr->Valid() && itr->key().starts_with(old_directory); itr->Next()){
			std::string old_path = itr->key().ToString();
			fs::path new_path = new_directory / fs::relative(old_path, old_directory);
			batch.Delete(itr->key());
			batch.Put(new_path.string(), itr->value());
		}
		delete itr;
		{
			std::lock_guard<std::mutex> lk(l::rocksdb::global_lock_);
			db->Write(::rocksdb::WriteOptions(), &batch);
		}
	}
}
