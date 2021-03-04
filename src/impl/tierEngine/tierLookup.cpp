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

#include "tierEngine.hpp"

Tier *TierEngine::tier_lookup(fs::path p){
	for(std::list<Tier>::iterator t = tiers_.begin(); t != tiers_.end(); ++t){
		if(t->path() == p)
			return &(*t);
	}
	return nullptr;
}

Tier *TierEngine::tier_lookup(std::string id){
	for(std::list<Tier>::iterator t = tiers_.begin(); t != tiers_.end(); ++t){
		if(t->id() == id)
			return &(*t);
	}
	return nullptr;
}
