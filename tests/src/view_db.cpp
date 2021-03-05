#include "metadata.hpp"
#include <rocksdb/db.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

struct Row{
	std::string key = "";
	std::string tier_path = "";
	int acount = 0;
	double popularity = 0.0;
	bool pinned = false;
};

class MetadataViewer{
public:
	Row get_row(const Metadata &f, std::string key_ = ""){
		Row row;
		row.key = key_;
		row.tier_path = f.tier_path_;
		row.acount = f.access_count_;
		row.popularity = f.popularity_;
		row.pinned = f.pinned_;
		return row;
	}
};

int main(int argc, char *argv[]){
	std::string db_path = "/var/lib/autotier/" + std::to_string(std::hash<std::string>{}("/etc/autotier.conf")) + "/db";
	rocksdb::DB *db;
	rocksdb::Status status = rocksdb::DB::OpenForReadOnly(rocksdb::Options(), db_path, &db);
	assert(status.ok());
	
	MetadataViewer viewer;
	
	std::string key_header = "Key";
	std::string tier_header = "Tier";
	std::string acount_header = "Access Count";
	std::string pop_header = "Popularity";
	std::string pinned_header = "Pinned";
	
	std::vector<Row> vec;
	size_t key_len_ = key_header.length();
	size_t tpath_len_ = tier_header.length();
	size_t acnt_len_ = acount_header.length();
	size_t pop_len_ = pop_header.length();
	size_t bool_len_ = std::max(int(pinned_header.length()), 5);
	
	rocksdb::Iterator *it = db->NewIterator(rocksdb::ReadOptions());
	
	for(it->SeekToFirst(); it->Valid(); it->Next()){
		size_t key_len = it->key().ToString().length();
		if(key_len > key_len_)
			key_len_ = key_len;
		
		Metadata f(it->value().ToString());
		
		Row row = viewer.get_row(f, it->key().ToString());
		
		size_t tpath_len = row.tier_path.length();
		if(tpath_len > tpath_len_)
			tpath_len_ = tpath_len;
		size_t acnt_len = std::to_string(row.acount).length();
		if(acnt_len > acnt_len_)
			acnt_len_ = acnt_len;
		size_t pop_len = std::to_string(row.popularity).length();
		if(pop_len > pop_len_)
			pop_len_ = pop_len;
		
		vec.push_back(row);
	}
	
	std::stringstream header;
	header << std::setw(key_len_) << std::left << key_header << " | ";
	header << std::setw(tpath_len_) << std::left << tier_header << "  ";
	header << std::setw(acnt_len_) << acount_header << "  ";
	header << std::setw(pop_len_) << pop_header << "  ";
	header << std::setw(bool_len_) << pinned_header;
	header << std::endl;
	
	std::cout << header.str();
	std::stringstream intersect;
	intersect << std::setfill('-') << std::setw(key_len_ + 2) << "+";
	std::cout << std::setfill('-') << std::setw(header.str().length()) << std::left << intersect.str() << std::endl;
	
	std::cout.fill(' ');
	
	for(const Row &row : vec){
		std::cout << std::setw(key_len_) << std::left << row.key << " | ";
		std::cout << std::setw(tpath_len_) << std::left << row.tier_path<< "  ";
		std::cout << std::setw(acnt_len_) << row.acount << "  ";
		std::cout << std::setw(pop_len_) << row.popularity << "  ";
		std::cout << std::setw(bool_len_) << std::boolalpha << row.pinned;
		std::cout << std::endl;
	}
	
	delete db;
	return 0;
}
