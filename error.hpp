#pragma once

#define NUM_ERRORS 8
enum Error{LOAD_CONF, TIER_DNE, THRESHOLD_ERR, TIER_ID, NO_FIRST_TIER, NO_TIERS, ONE_TIER, WATERMARK_ERR};

void error(enum Error error);
