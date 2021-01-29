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

/* Simple concurrent queue implementation. No guard against popping from empty queue.
 * Check empty() before popping. Intended only for multiple producer, single consumer use.
 */


#pragma once

#include <queue>
#include <mutex>

template<class T>
class ConcurrentQueue{
private:
	std::mutex mt_;
	std::queue<T> queue_;
public:
	ConcurrentQueue() : mt_(), queue_() {}
	bool empty(void) const{
		return queue_.empty();
	}
	void push(const T &val){
		std::lock_guard<std::mutex> lk(mt_);
		queue_.push(val);
	}
	void emplace(const T &val){
		std::lock_guard<std::mutex> lk(mt_);
		queue_.emplace(val);
	}
	T pop(void){
		std::lock_guard<std::mutex> lk(mt_);
		T val = queue_.front();
		queue_.pop();
		return val;
	}
};
