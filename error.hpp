#pragma once

#define NUM_ERRORS 5
enum Error{LOAD_CONF, TIER_DNE, THRESHOLD_ERR, TIER_ID, NO_FIRST_TIER};

void error(enum Error error);
