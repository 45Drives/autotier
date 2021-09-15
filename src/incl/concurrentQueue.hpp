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

/**
 * @brief Single-consumer multiple-producer concurrent FIFO queue.
 * 
 * @tparam T Type to store.
 */
template<class T>
class ConcurrentQueue{
public:
	/**
	 * @brief Construct a new Concurrent Queue object
	 * 
	 */
	ConcurrentQueue() : mt_(), queue_() {}
	/**
	 * @brief Destroy the Concurrent Queue object
	 * 
	 */
	~ConcurrentQueue() = default;
	/**
	 * @brief Check if queue is empty. Call this first before trying to pop().
	 * 
	 * @return true Queue is empty, do not pop().
	 * @return false Queue is not empty, safe to pop().
	 */
	bool empty(void) const{
		return queue_.empty();
	}
	/**
	 * @brief Insert into queue with std::queue<T>::push().
	 * 
	 * @param val Value to insert.
	 */
	void push(const T &val){
		std::lock_guard<std::mutex> lk(mt_);
		queue_.push(val);
	}
	/**
	 * @brief Emplace into queue with std::queue<T>::emplace().
	 * 
	 * @tparam Args args to expand into constructor of T
	 * @param args args to expand into constructor of T
	 */
	template<typename... Args>
	void emplace(Args&&... args){
		std::lock_guard<std::mutex> lk(mt_);
		queue_.emplace(args...);
	}
	/**
	 * @brief Pop value from queue. Check empty() first!
	 * 
	 * @return T Object returned from queue
	 */
	T pop(void){
		std::lock_guard<std::mutex> lk(mt_);
		T val = queue_.front();
		queue_.pop();
		return val;
	}
private:
	std::mutex mt_; ///< Mutex for synchronization
	std::queue<T> queue_; ///< Underlying queue to store objects in
};
