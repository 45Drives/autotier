#include "crawl.hpp"
#include "config.hpp"
#include "error.hpp"
#include <openssl/md5.h>
#include <iomanip>

Tier *highest_tier;
Tier *lowest_tier;

void launch_crawlers(){
  for(Tier *tptr = highest_tier; tptr->lower != NULL; tptr=tptr->lower){
    crawl(tptr->dir, tier_down, tptr);
  }
  for(Tier *tptr = lowest_tier; tptr->higher != NULL; tptr=tptr->higher){
    crawl(tptr->dir, tier_up, tptr);
  }
}

void crawl(const fs::path &src, void (*action)(fs::path, Tier *), Tier *tptr){
  for(fs::directory_iterator itr{src}; itr != fs::directory_iterator{}; *itr++){
    if(is_directory(*itr)){
      crawl(*itr, action, tptr);
    }else if(!is_symlink(*itr)){
      action(*itr, tptr);
    }
  }
}

void tier_up(fs::path item, Tier *tptr){
  time_t mtime = last_write_time(item);
  if((time(NULL) - mtime) >= tptr->higher->expires) return;
  std::cout << "Tiering up" << std::endl;
  fs::path symlink = tptr->higher->dir/relative(item,tptr->dir);
  if(is_symlink(symlink)){
    remove(symlink);
  }
  copy_file(item,symlink); // move item back to original location
  last_write_time(symlink, mtime);
  if(verify_copy(item,symlink)){
    std::cout << "Copy succeeded. " << std::endl;
    remove(item);
  }else{
    std::cout << "Copy failed. " << std::endl;
  }
}

void tier_down(fs::path item, Tier *tptr){
  time_t mtime = last_write_time(item);
  if((time(NULL) - mtime) < tptr->expires) return;
  std::cout << "Tiering down" << std::endl;
  fs::path move_to = tptr->lower->dir/relative(item,tptr->dir);
  if(!is_directory(move_to.parent_path()))
    create_directories(move_to.parent_path());
  copy_file(item,move_to); // move item to slow tier
  last_write_time(move_to, mtime); // keep old mtime
  if(verify_copy(item, move_to)){
    std::cout << "Copy succeeded. " << std::endl;
    remove(item);
    create_symlink(move_to,item); // create symlink fast/item -> slow/item
  }else{
    std::cout << "Copy failed. " << std::endl;
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
