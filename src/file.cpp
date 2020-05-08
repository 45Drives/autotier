#include "file.hpp"
#include "alert.hpp"
#include "xxhash64.h"

#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/xattr.h>

void File::log_movement(){
  Log("OldPath: " + old_path.string(),3);
  Log("NewPath: " + new_path.string(),3);
  Log("SymLink: " + symlink_path.string(),3);
  Log("UserPin: " + pinned_to.string(),3);
  Log("",3);
}

void File::move(){
  if(old_path == new_path) return;
  if(!is_directory(new_path.parent_path()))
    create_directories(new_path.parent_path());
  Log("Copying " + old_path.string() + " to " + new_path.string(),2);
  copy_file(old_path, new_path); // move item to slow tier
  copy_ownership_and_perms();
  if(verify_copy()){
    Log("Copy succeeded.\n",2);
    remove(old_path);
  }else{
    Log("Copy failed!\n",0);
    /*
     * TODO: put in place protocol for what to do when this happens
     */
  }
  utime(new_path.c_str(), &times); // overwrite mtime and atime with previous times
}

void File::copy_ownership_and_perms(){
  struct stat info;
  stat(old_path.c_str(), &info);
  chown(new_path.c_str(), info.st_uid, info.st_gid);
  chmod(new_path.c_str(), info.st_mode);
}

bool File::verify_copy(){
  /*
   * TODO: more efficient error checking than this? Also make
   * optional in global configuration?
   */
  int bytes_read;
  char *src_buffer = new char[4096];
  char *dst_buffer = new char[4096];
  
  int srcf = open(old_path.c_str(),O_RDONLY);
  int dstf = open(new_path.c_str(),O_RDONLY);
  
  XXHash64 src_hash(0);
  XXHash64 dst_hash(0);
  
  while((bytes_read = read(srcf,src_buffer,sizeof(char[4096]))) > 0){
    src_hash.add(src_buffer,bytes_read);
  }
  while((bytes_read = read(dstf,dst_buffer,sizeof(char[4096]))) > 0){
    dst_hash.add(dst_buffer,bytes_read);
  }
  
  close(srcf);
  close(dstf);
  delete [] src_buffer;
  delete [] dst_buffer;
  
  uint64_t src_result = src_hash.hash();
  uint64_t dst_result = dst_hash.hash();
  
  std::stringstream ss;
  
  ss << "SRC HASH: 0x" << std::hex << src_result << std::endl;
  ss << "DST HASH: 0x" << std::hex << dst_result;
  
  Log(ss.str(),2);
  
  return (src_result == dst_result);
}

void File::calc_popularity(){
  double diff;
  if(times.actime > last_atime){
    // increase popularity
    diff = times.actime - last_atime;
  }else{
    // decrease popularity
    diff = time(NULL) - last_atime;
  }
  if(diff < 1) diff = 1;
  double delta = (popularity / DAMPING) * (1.0 - (diff / NORMALIZER) * popularity);
  double delta_cap = -1.0*popularity/2.0; // limit change to half of current val
  if(delta < delta_cap)
    delta = delta_cap;
  popularity += delta;
  if(popularity < FLOOR) // ensure val is positive else unstable (-inf)
    popularity = FLOOR;
}

void File::write_xattrs(){
  if(setxattr(new_path.c_str(),"user.autotier_last_atime",&last_atime,sizeof(last_atime),0)==ERR)
    error(SETX);
  if(setxattr(new_path.c_str(),"user.autotier_popularity",&popularity,sizeof(popularity),0)==ERR)
    error(SETX);
  if(setxattr(new_path.c_str(),"user.autotier_pin",pinned_to.c_str(),strlen(pinned_to.c_str()),0)==ERR)
    error(SETX);
}

File::File(fs::path path_, Tier *tptr){
  char strbuff[BUFF_SZ];
  ssize_t attr_len;
  new_path = old_path = path_;
  old_tier = tptr;
  struct stat info;
  stat(old_path.c_str(), &info);
  size = (long)info.st_size;
  times.actime = info.st_atime;
  times.modtime = info.st_mtime;
  if((attr_len = getxattr(old_path.c_str(),"user.autotier_pin",strbuff,sizeof(strbuff))) != ERR){
    strbuff[attr_len] = '\0'; // c-string
    pinned_to = fs::path(strbuff);
  }
  if(getxattr(old_path.c_str(),"user.autotier_last_atime",&last_atime,sizeof(last_atime)) <= 0){
    last_atime = times.actime;
  }
  if(getxattr(old_path.c_str(),"user.autotier_popularity",&popularity,sizeof(popularity)) <= 0){
    // initialize
    popularity = 1.0;
  }
  last_atime = times.actime;
}

File::File(const File &rhs){
  //priority = rhs.priority;
  popularity = rhs.popularity;
  last_atime = rhs.last_atime;
  size = rhs.size;
  times.modtime = rhs.times.modtime;
  times.actime = rhs.times.actime;
  symlink_path = rhs.symlink_path;
  old_path = rhs.old_path;
  new_path = rhs.new_path;
  pinned_to = rhs.pinned_to;
}

File::~File(){
  write_xattrs();
}

File &File::operator=(const File &rhs){
  //priority = rhs.priority;
  popularity = rhs.popularity;
  last_atime = rhs.last_atime;
  size = rhs.size;
  times.modtime = rhs.times.modtime;
  times.actime = rhs.times.actime;
  symlink_path = rhs.symlink_path;
  old_path = rhs.old_path;
  new_path = rhs.new_path;
  pinned_to = rhs.pinned_to;
  return *this;
}
