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

#include "metadata.hpp"
#include "tier.hpp"
#include "rocksDbHelpers.hpp"
#include <sstream>

Metadata::Metadata(void) :
	access_count_(0),
	popularity_(0.0),
	not_found_(false),
	pinned_(false),
	tier_path_(""){
	
}

Metadata::Metadata(const std::string &serialized){
	std::stringstream ss(serialized);
	boost::archive::text_iarchive ia(ss);
	this->serialize(ia, 0);
}

Metadata::Metadata(const Metadata &other) :
	access_count_(other.access_count_),
	popularity_(other.popularity_),
	not_found_(other.not_found_),
	pinned_(other.pinned_),
	tier_path_(other.tier_path_){
	
}

#ifndef BAREBONES_METADATA

Metadata::Metadata(const char *path, rocksdb::DB *db, Tier *tptr){
	std::string str;
	if(path[0] == '/') path++;
	rocksdb::Status s = db->Get(rocksdb::ReadOptions(), path, &str);
	if(s.ok()){
		std::stringstream ss(str);
		boost::archive::text_iarchive ia(ss);
		this->serialize(ia, 0);
	}else if(tptr){
		tier_path_ = tptr->path().string();
		update(path, db);
	}else{
		not_found_ = true;
	}
}

void Metadata::update(std::string relative_path, rocksdb::DB *db, std::string *old_key){
	if(relative_path.front() == '/')
		relative_path = relative_path.substr(1, std::string::npos);
	std::stringstream ss;
	{
		boost::archive::text_oarchive oa(ss);
		this->serialize(oa, 0);
	}
	{
		std::lock_guard<std::mutex> lk(l::rocksdb::global_lock_);
		rocksdb::WriteBatch batch;
		if(old_key){
			if(old_key->front() == '/')
				*old_key = old_key->substr(1, std::string::npos);
			batch.Delete(*old_key);
		}
		batch.Put(relative_path, ss.str());
		db->Write(rocksdb::WriteOptions(), &batch);
	}
}

void Metadata::touch(void){
	access_count_++;
}

std::string Metadata::tier_path(void) const{
	return tier_path_;
}

void Metadata::tier_path(const std::string &path){
	tier_path_ = path;
}

bool Metadata::pinned(void) const{
	return pinned_;
}

void Metadata::pinned(bool val){
	pinned_ = val;
}

double Metadata::popularity(void) const{
	return popularity_;
}

bool Metadata::not_found(void) const{
	return not_found_;
}

std::string Metadata::dump_stats(void) const{
	std::stringstream ss;
	ss << "tier_path_: " << tier_path_ << std::endl;
	ss << "access_count_: " << access_count_ << std::endl;
	ss << "popularity_: " << popularity_ << std::endl;
	ss << "pinned: " << std::boolalpha << pinned_ << std::noboolalpha << std::endl;
	return ss.str();
}

#endif
