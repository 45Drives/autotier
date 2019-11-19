#include "crawl.hpp"
#include "config.hpp"
#include "error.hpp"
#include "xxhash64.h"
#include <iomanip>
#include <regex>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

Tier *highest_tier = NULL;
Tier *lowest_tier = NULL;

void launch_crawlers(){
  for(Tier *tptr = highest_tier; tptr->lower != NULL; tptr=tptr->lower){
    tptr->crawl(tptr->dir, tier_down);
  }
  for(Tier *tptr = lowest_tier; tptr->higher != NULL; tptr=tptr->higher){
    tptr->crawl(tptr->dir, tier_up);
  }
}

void Tier::crawl(fs::path dir, void (*action)(fs::path, Tier *)){
  for(fs::directory_iterator itr{dir}; itr != fs::directory_iterator{}; *itr++){
    if(is_directory(*itr)){
      this->crawl(*itr, action);
    }else if(!is_symlink(*itr) &&
    !regex_match((*itr).path().filename().string(), std::regex("(^\\..*(\\.swp)$|^(\\.~lock\\.).*#$)"))){
      action(*itr, this);
    }
  }
}

static void tier_up(fs::path from_here, Tier *tptr){
  time_t mtime = last_write_time(from_here);
  if((time(NULL) - mtime) >= tptr->higher->expires) return;
  std::cout << "Tiering up" << std::endl;
  fs::path to_here = tptr->higher->dir/relative(from_here, tptr->dir);
  if(is_symlink(to_here)){
    remove(to_here);
  }
  copy_file(from_here, to_here); // move item back to original location
  copy_ownership_and_perms(from_here, to_here);
  last_write_time(to_here, mtime);
  if(verify_copy(from_here, to_here)){
    std::cout << "Copy succeeded. " << std::endl;
    remove(from_here);
  }else{
    std::cout << "Copy failed. " << std::endl;
  }
}

static void tier_down(fs::path from_here, Tier *tptr){
  time_t mtime = last_write_time(from_here);
  if((time(NULL) - mtime) < tptr->expires) return;
  std::cout << "Tiering down" << std::endl;
  fs::path to_here = tptr->lower->dir/relative(from_here, tptr->dir);
  if(!is_directory(to_here.parent_path()))
    create_directories(to_here.parent_path());
  copy_file(from_here, to_here); // move item to slow tier
  copy_ownership_and_perms(from_here, to_here);
  last_write_time(to_here, mtime); // keep old mtime
  if(verify_copy(from_here, to_here)){
    std::cout << "Copy succeeded. " << std::endl;
    remove(from_here);
    create_symlink(to_here, from_here); // create symlink fast/item -> slow/item
    copy_ownership_and_perms(to_here, from_here); // copy original ownership to symlink
  }else{
    std::cout << "Copy failed. " << std::endl;
  }
}

void copy_ownership_and_perms(fs::path src, fs::path dst){
  struct stat info;
  stat(src.c_str(), &info);
  if(is_symlink(dst)){
    lchown(dst.c_str(), info.st_uid, info.st_gid);
  }else{
    chown(dst.c_str(), info.st_uid, info.st_gid);
    chmod(dst.c_str(), info.st_mode);
  }
}

bool verify_copy(fs::path src, fs::path dst){
  char *src_buffer = new char[4096];
  char *dst_buffer = new char[4096];
  
  int srcf = open(src.c_str(),O_RDONLY);
  int dstf = open(dst.c_str(),O_RDONLY);
  
  XXHash64 src_hash(0);
  XXHash64 dst_hash(0);
  
  while(read(srcf,src_buffer,sizeof(char[4096]))){
    src_hash.add(src_buffer,sizeof(char[4096]));
  }
  while(read(dstf,dst_buffer,sizeof(char[4096]))){
    dst_hash.add(dst_buffer,sizeof(char[4096]));
  }
  
  close(srcf);
  close(dstf);
  delete [] src_buffer;
  delete [] dst_buffer;
  
  uint64_t src_result = src_hash.hash();
  uint64_t dst_result = dst_hash.hash();
  
  std::cout << "SRC HASH: " << std::hex << src_result << std::endl;
  std::cout << "DST HASH: " << std::hex << dst_result << std::endl;
  
  return (src_result == dst_result);
}
