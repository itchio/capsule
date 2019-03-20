
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

namespace capsule {

template <typename T> class LockingQueue {
public:
  void Push(T const &data) {
    {
      std::lock_guard<std::mutex> lock(guard_);
      queue_.push(data);
    }
    signal_.notify_one();
  }

  bool Empty() const {
    std::lock_guard<std::mutex> lock(guard_);
    return queue_.empty();
  }

  bool TryPop(T &value) {
    std::lock_guard<std::mutex> lock(guard_);
    if (queue_.empty()) {
      return false;
    }

    value = queue_.front();
    queue_.pop();
    return true;
  }

  void WaitAndPop(T &value) {
    std::unique_lock<std::mutex> lock(guard_);
    while (queue_.empty()) {
      signal_.wait(lock);
    }

    value = queue_.front();
    queue_.pop();
  }

  bool TryWaitAndPop(T &value, int milli) {
    std::unique_lock<std::mutex> lock(guard_);
    while (queue_.empty()) {
      signal_.wait_for(lock, std::chrono::milliseconds(milli));
      return false;
    }

    value = queue_.front();
    queue_.pop();
    return true;
  }

private:
  std::queue<T> queue_;
  mutable std::mutex guard_;
  std::condition_variable signal_;
};

} // namespace capsule
