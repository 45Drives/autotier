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

#include <queue>
#include <mutex>

template<class T>
class ConcurrentQueue : std::queue<T>{
private:
	std::mutex mt_;
public:
	ConcurrentQueue() : std::queue<T>(), mt_() {}
	void push(const T &val){
		std::lock_guard<std::mutex> lk(mt_);
		std::queue<T>::push(val);
	}
	void emplace(const T &val){
		std::lock_guard<std::mutex> lk(mt_);
		std::queue<T>::emplace(val);
	}
	T pop(void){
		std::lock_guard<std::mutex> lk(mt_);
		T val = std::queue<T>::front();
		std::queue<T>::pop();
		return val;
	}
};
