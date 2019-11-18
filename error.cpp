#include "error.hpp"
#include <iostream>

std::string errors[NUM_ERRORS] = {
  "Error loading configuration file.",
  "Tier directory does not exist. Please check config.",
  "THRESHOLD error. Must be a positive integer. Please check config.",
  "Error reading tier ID ([Tier <n>]). <n> must be positive integer.",
  "First tier must be specified with '[Tier 1]' before any other config options."
};

void error(enum Error error){
  std::cerr << errors[error] << std::endl;
}
