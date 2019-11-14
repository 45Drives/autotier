#include "crawl.hpp"
#include "config.hpp"
#include "error.hpp"

void launch_crawlers(){
  crawl(config.get_fast_tier(), tier_down);
  crawl(config.get_slow_tier(), tier_up);
}

void crawl(const fs::path &src, void (*action)(fs::path)){
  for(fs::directory_iterator itr{src}; itr != fs::directory_iterator{}; *itr++){
    if(is_directory(*itr)){
      crawl(*itr,action);
    }else if(!is_symlink(*itr)){
      action(*itr);
    }
  }
}

void tier_up(fs::path item){
  if((last_write_time(item) - time(NULL)) >= config.get_threshold()) return;
  fs::path symlink = config.get_fast_tier()/relative(item,config.get_slow_tier());
  if(is_symlink(symlink)){
    remove(symlink);
  }
  rename(item,symlink); // move item back to original location
}

void tier_down(fs::path item){
  if((last_write_time(item) - time(NULL)) < config.get_threshold()) return;
  fs::path move_to = config.get_slow_tier()/relative(item,config.get_fast_tier());
  rename(item,move_to); // move item to slow tier
  create_symlink(move_to,item); // create symlink fast/item -> slow/item
}

long get_age(const fs::path &item){
  return 0;
}
