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

#include "components/tiering.hpp"
#include "components/adhoc.hpp"

/**
 * @brief Main TierEngine object to construct for setting up autotier.
 * See other components for functionality.
 * 
 */
class TierEngine : public TierEngineTiering {
public:
    /**
     * @brief Construct a new Tier Engine object
     * 
     * @param config_path Path to config file
     * @param config_overrides Config overrides from cli args
     */
	TierEngine(const fs::path &config_path, const ConfigOverrides &config_overrides);
    /**
     * @brief Destroy the Tier Engine object
     * 
     */
    ~TierEngine(void);
private:
};
