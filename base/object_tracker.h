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

#include <algorithm>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "lb_memory_debug.h"
#include "lb_memory_manager.h"

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
  typedef void LogCB(const std::string&);

  static void Dump(LogCB log_cb) {
    AutoLock guard(lock_.Get());
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
      log_cb(base::StringPrintf("Skipped %d items\n", skipped));
    }
    log_cb("============================= end =============================\n");
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

    AutoLock guard(lock_.Get());
    next_ = head_;
    if (next_) {
      next_->prev_ = this;
    }
    head_ = this;
  }

  ~ObjectTracker() {
    AutoLock guard(lock_.Get());
    if (next_) {
      next_->prev_ = prev_;
    }
    if (prev_) {
      prev_->next_ = next_;
    } else {
      head_ = next_;
    }
  }

  bool ShouldDump() { return true; }
  std::string DumpSpecificInfo() { return ""; }

 private:
  static ObjectTracker* head_;
  static LazyInstance<Lock> lock_;

  ObjectTracker* prev_;
  ObjectTracker* next_;
  uintptr_t back_trace_[BacktraceLevel];
};

// static
template <typename Object, uint32 BacktraceLevel>
ObjectTracker<Object, BacktraceLevel>*
    ObjectTracker<Object, BacktraceLevel>::head_;
// static
template <typename Object, uint32 BacktraceLevel>
LazyInstance<Lock> ObjectTracker<Object, BacktraceLevel>::lock_ =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace base

#endif  // BASE_OBJECT_TRACKER_H_
