// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_BASE_STOP_WATCH_H_
#define COBALT_BASE_STOP_WATCH_H_

#include "base/time.h"

namespace base {

class StopWatchOwner;

class StopWatch {
 public:
  enum AutoStart {
    kAutoStartOn,
    kAutoStartOff,
  };

  StopWatch(int id, AutoStart auto_start, StopWatchOwner* owner);
  ~StopWatch();

  void Start();
  void Stop();

  bool IsCounting() const;

 private:
  int id_;
  StopWatchOwner* owner_;

  base::TimeTicks start_time_;
};

class StopWatchOwner {
 protected:
  virtual ~StopWatchOwner() {}

 private:
  virtual bool IsStopWatchEnabled(int id) const = 0;
  virtual void OnStopWatchStopped(int id, base::TimeDelta time_elapsed) = 0;

  friend class StopWatch;
};

}  // namespace base

#endif  // COBALT_BASE_STOP_WATCH_H_
