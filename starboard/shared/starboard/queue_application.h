// Copyright 2015 Google Inc. All Rights Reserved.
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

// A simple queue-based application implementation.

#ifndef STARBOARD_SHARED_STARBOARD_QUEUE_APPLICATION_H_
#define STARBOARD_SHARED_STARBOARD_QUEUE_APPLICATION_H_

#include "starboard/condition_variable.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {

// An application implementation that uses a signaling thread-safe queue to
// manage event dispatching.
class QueueApplication : public Application {
 public:
  QueueApplication() {}
  ~QueueApplication() SB_OVERRIDE {}

 protected:
  // Polls the queue for the next event, returning NULL if there is nothing to
  // execute.
  Event* PollNextEvent();

  // --- Application overrides ---
  Event* GetNextEvent() SB_OVERRIDE;
  void Inject(Event* event) SB_OVERRIDE;

 private:
  // The queue of events that have not yet been dispatched.
  Queue<Event*> event_queue_;
};

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_QUEUE_APPLICATION_H_
