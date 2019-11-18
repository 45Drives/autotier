#include "crawl.hpp"
#include "config.hpp"
#include "error.hpp"
#include <openssl/md5.h>
#include <iomanip>
#include <regex>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>

Tier *highest_tier;
Tier *lowest_tier;

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
    !regex_match((*itr).path().filename().string(), std::regex("^\\..*(.swp)$"))){
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

void print_md5_sum(unsigned char* md){
  for(unsigned i = 0; i < MD5_DIGEST_LENGTH; i++){
    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md[i]);
  }
}

bool verify_copy(fs::path src, fs::path dst){
  int j = 0;
  unsigned char digest[2][MD5_DIGEST_LENGTH] = {{},{}};
  for(fs::path i : {src, dst}){
    std::ifstream file(i.string(), std::ios::binary);
    MD5_CTX ctx;
    MD5_Init(&ctx);
    char file_buffer[4096];
    while (file.read(file_buffer, sizeof(file_buffer)) || file.gcount()) {
      MD5_Update(&ctx, file_buffer, file.gcount());
    }
    MD5_Final(digest[j++], &ctx);
  }
  
  std::cout << "SRC MD5: ";
  print_md5_sum(digest[0]);
  std::cout << std::endl;
  std::cout << "DST MD5: ";
  print_md5_sum(digest[1]);
  std::cout << std::endl;
  
  for(unsigned char i = 0; i < MD5_DIGEST_LENGTH; i++)
    if(digest[0][i] != digest[1][i])
      return false;
  
  return true;
}
