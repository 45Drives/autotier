#pragma once

// popularity variables
// y' = (1/DAMPING)*y*(1-(TIME_DIFF/NORMALIZER)*y)
// DAMPING is how slowly popularity changes
// TIME_DIFF is time since last file access
// NORMALIZER is TIME_DIFF at which popularity -> 1.0
// FLOOR ensures y stays above zero for stability
#define DAMPING 50.0
#define NORMALIZER 1000.0
#define FLOOR 0.1
#define CALC_PERIOD 1

#define BUFF_SZ 4096

#include "alert.hpp"
#include "tier.hpp"

class Tier;

#include <utime.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class File{
public:
  double popularity;   // calculated from number of accesses
  long last_atime;
  long size;
  Tier *old_tier;
  struct utimbuf times;
  fs::path symlink_path;
  fs::path old_path;
  fs::path new_path;
  fs::path pinned_to;
  void write_xattrs(void);
  void log_movement(void);
  void move(void);
  void copy_ownership_and_perms(void);
  bool verify_copy(void);
  void calc_popularity(void);
  File(fs::path path_, Tier *tptr){
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
  File(const File &rhs) {
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
  ~File(){
    write_xattrs();
  }
  File &operator=(const File &rhs){
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
};
