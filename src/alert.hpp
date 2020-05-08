/*
    Copyright (C) 2019-2020 Joshua Boudreau
    
    This file is part of autotier.

    autotier is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    autotier is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with autotier.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <string>

#define ERR -1

extern int log_lvl;

#define NUM_ERRORS 10
enum Error{LOAD_CONF, TIER_DNE, NO_FIRST_TIER, NO_TIERS, ONE_TIER, WATERMARK_ERR, SETX, LOG_LVL, PERIOD, GET_MUTEX_NAME};

void error(enum Error error);

void Log(std::string msg, int lvl);
