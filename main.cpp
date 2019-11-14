#include "config.hpp"
#include "crawl.hpp"

inline bool config_passed(int argc, char *argv[]){
  return (argc >= 3 && (strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--config") == 0));
}

int main(int argc, char *argv[]){
  fs::path config_path = (config_passed(argc, argv))? argv[2] : DEFAULT_CONFIG_PATH;
  config.load(config_path);
  
  return 0;
}
