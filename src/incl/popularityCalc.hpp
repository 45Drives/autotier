/*
 *    Copyright (C) 2019-2021 Joshua Boudreau <jboudreau@45drives.com>
 *    
 *    This file is part of autotier.
 * 
 *    autotier is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 * 
 *    autotier is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 * 
 *    You should have received a copy of the GNU General Public License
 *    along with autotier.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#define MINUTE 60.0
#define HOUR (60.0 * MINUTE)
#define DAY (24.0 * HOUR)
#define WEEK (7.0 * DAY)

/* popularity calculation
 * y[n] = MULTIPLIER * x / DAMPING + (1.0 - 1.0 / DAMPING) * y[n-1]
 * where x is file usage frequency
 */
#define START_DAMPING  50000.0
#define DAMPING      1000000.0
#define MULTIPLIER      3600.0
#define REACH_FULL_DAMPING_AFTER (1.0 * WEEK)
#define SLOPE ((DAMPING - START_DAMPING) / REACH_FULL_DAMPING_AFTER)
/* DAMPING is how slowly popularity changes.
 * MULTIPLIER is to scale values to accesses per hour.
 */

#define AVG_USAGE 0.238 // 40hr/(7days * 24hr/day)
/* for calculating initial popularity for new files
 * Unit: accesses per second.
 */

