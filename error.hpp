#pragma once

#define NUM_ERRORS 4
enum Error{LOAD_CONF, FAST_TIER_DNE, SLOW_TIER_DNE, THRESHOLD_ERR};

void error(enum Error error);
