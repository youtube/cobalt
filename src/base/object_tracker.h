/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BASE_OBJECT_TRACKER_H_
#define BASE_OBJECT_TRACKER_H_

#include <assert.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/stringprintf.h"
#include "lb_memory_debug.h"
#include "lb_memory_manager.h"

#if defined(__LB_SHELL__)
#include "lb_mutex.h"
#else  // defined(__LB_SHELL__)
#error "Have to include platform header for mutex"
#endif  // defined(__LB_SHELL__)

namespace base {

// This class can be used to track and dump all objects belongs to a certain
// type.  For example, we can use the following steps to dump all Nodes:
// 1. Make Node inherit from ObjectTracker<Node>.
// 2. Call "Node::Dump(log_cb);" on a particular event (like a key press).
// We can also return false in ShouldDump() to skip certain objects and
// implement DumpSpecificInfo() to dump information specific to the object like
// ref count, etc..
// This class is meant to be used for debugging only.
template <typename Object, uint32 BacktraceLevel = 10>
class ObjectTracker {
 public:
#if defined(__LB_SHELL__)
  // We implement our own Lock class to remove the dependency on base::Lock, so
  // we can track base::Lock.
  class Lock {
   public:
    Lock() {
      int rv = lb_shell_mutex_init(&mutex_);
      assert(rv == 0);
      UNREFERENCED_PARAMETER(rv);
    }
    ~Lock() {
      int rv = lb_shell_mutex_destroy(&mutex_);
      assert(rv == 0);
      UNREFERENCED_PARAMETER(rv);
    }
    void Acquire() {
      int rv = lb_shell_mutex_lock(&mutex_);
      assert(rv == 0);
      UNREFERENCED_PARAMETER(rv);
    }
    void Release() {
      int rv = lb_shell_mutex_unlock(&mutex_);
      assert(rv == 0);
      UNREFERENCED_PARAMETER(rv);
    }

   private:
    lb_shell_mutex_t mutex_;
  };
#endif  // defined(__LB_SHELL__)

  typedef void LogCB(const std::string&);

  static void Dump(LogCB log_cb) {
    lock().Acquire();
    log_cb("============================ start ============================\n");
    // Sort the objects based on their addresses.
    std::vector<ObjectTracker*> trackers;
    trackers.reserve(16 * 1024);
    ObjectTracker* current = head_;
    while (current) {
      trackers.push_back(current);
      current = current->next_;
    }
    std::sort(trackers.begin(), trackers.end());

    int skipped = 0;

    for (size_t i = 0; i < trackers.size(); ++i) {
      std::string line;
      current = trackers[i];
      if ((static_cast<Object*>(current))->ShouldDump()) {
        line = base::StringPrintf("%" PRIxPTR " ",
                                  reinterpret_cast<intptr_t>(current));
        line += (static_cast<Object*>(current))->DumpSpecificInfo();
        for (uint32 j = 0; j < BacktraceLevel; ++j) {
          base::StringAppendF(&line, " %" PRIxPTR, current->back_trace_[j]);
        }
        line += "\n";
        log_cb(line);
      } else {
        ++skipped;
      }
    }
    if (skipped != 0) {
      log_cb(base::StringPrintf("Dumped %d items, skipped %d items\n",
                                static_cast<int>(trackers.size()), skipped));
    } else {
      log_cb(base::StringPrintf("Dumped %d items\n",
                                static_cast<int>(trackers.size())));
    }
    log_cb("============================= end =============================\n");
    lock().Release();
  }

 protected:
  ObjectTracker() : next_(NULL), prev_(NULL) {
    int valid_trace_count = 0;
    if (LB::Memory::GetBacktraceEnabled()) {
      valid_trace_count =
          LB::Memory::Backtrace(0, BacktraceLevel, back_trace_, NULL);
    }
    if (valid_trace_count != BacktraceLevel) {
      memset(back_trace_ + valid_trace_count, 0,
             (BacktraceLevel - valid_trace_count) * sizeof(back_trace_[0]));
    }

    lock().Acquire();
    next_ = head_;
    if (next_) {
      next_->prev_ = this;
    }
    head_ = this;
    lock().Release();
  }

  ~ObjectTracker() {
    lock().Acquire();
    if (next_) {
      next_->prev_ = prev_;
    }
    if (prev_) {
      prev_->next_ = next_;
    } else {
      head_ = next_;
    }
    lock().Release();
  }

  bool ShouldDump() { return true; }
  std::string DumpSpecificInfo() { return ""; }

 private:
  static ObjectTracker* head_;
  static Lock& lock() {
    static Lock* lock;
    // This isn't thread safe.  But it should be fine as ObjectTracker is
    // supposed to be used only for debugging and this removed its dependency
    // on LazyInstance which is in turn depended on AtExitManager.
    if (lock == NULL) {
      lock = new Lock;
    }
    return *lock;
  }

  ObjectTracker* prev_;
  ObjectTracker* next_;
  uintptr_t back_trace_[BacktraceLevel];
};

// static
template <typename Object, uint32 BacktraceLevel>
ObjectTracker<Object, BacktraceLevel>*
    ObjectTracker<Object, BacktraceLevel>::head_;

}  // namespace base

#endif  // BASE_OBJECT_TRACKER_H_
