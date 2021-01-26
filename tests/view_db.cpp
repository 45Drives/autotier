
#include "../src/file.hpp"
#include <rocksdb/db.h>
#include <iostream>
#include <algorithm>

int main(int argc, char *argv[]){
	std::string db_path = "/run/autotier/" + std::to_string(std::hash<std::string>{}("/etc/autotier.conf")) + "/db";
	rocksdb::DB *db;
	rocksdb::Status status = rocksdb::DB::OpenForReadOnly(rocksdb::Options(), db_path, &db);
	assert(status.ok());
	
	rocksdb::Iterator *it = db->NewIterator(rocksdb::ReadOptions());
	
	for(it->SeekToFirst(); it->Valid(); it->Next()){
		std::string rel_path = it->key().ToString();
		Metadata f(rel_path.c_str(), db);
		std::cout << rel_path << ":" << std::endl;
		std::cout << f.dump_stats() << std::endl;
	}
	
	delete db;
	return 0;
}
