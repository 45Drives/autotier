#include "error.hpp"
#include <iostream>

std::string errors[NUM_ERRORS] = {
  "Error loading configuration file.",
  "FAST_TIER does not exist. Please check config.",
  "SLOW_TIER does not exist. Please check config.",
  "THRESHOLD error. Must be a positive integer. Please check config."
};

void error(enum Error error){
  std::cerr << errors[error] << std::endl;
}
