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

#include "cobalt/dom/event_listener.h"

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "cobalt/base/user_log.h"
#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace dom {
namespace {

// This class records the nested events and manages the user log information.
class ScriptEventLog {
 public:
  ScriptEventLog() : current_stack_depth_(0) {
    base::UserLog::Register(base::UserLog::kEventStackLevelCountIndex,
                            "EventLevelCnt", &current_stack_depth_,
                            sizeof(current_stack_depth_));

    memset(event_log_stack_, 0, sizeof(event_log_stack_));
    const int kLogNameBufferSize = 16;
    char log_name_buffer[kLogNameBufferSize];
    for (int i = 0; i < base::UserLog::kEventStackMaxDepth; i++) {
      snprintf(log_name_buffer, kLogNameBufferSize, "EventLevel%d", i);
      base::UserLog::Register(static_cast<base::UserLog::Index>(
                                  base::UserLog::kEventStackMinLevelIndex + i),
                              log_name_buffer, event_log_stack_[i],
                              kLogEntryMaxLength);
    }
  }

  ~ScriptEventLog() {
    base::UserLog::Deregister(base::UserLog::kEventStackLevelCountIndex);
    for (int i = 0; i < base::UserLog::kEventStackMaxDepth; i++) {
      base::UserLog::Deregister(static_cast<base::UserLog::Index>(
          base::UserLog::kEventStackMinLevelIndex + i));
    }
  }

  void PushEvent(Event* event) {
    if (current_stack_depth_ < base::UserLog::kEventStackMaxDepth) {
      snprintf(event_log_stack_[current_stack_depth_], kLogEntryMaxLength,
               "%s@%s", event->type().c_str(),
               event->current_target()->GetDebugName().c_str());
    } else {
      NOTREACHED();
    }
    current_stack_depth_++;
  }

  void PopEvent() {
    DCHECK(current_stack_depth_);
    current_stack_depth_--;
    memset(event_log_stack_[current_stack_depth_], 0, kLogEntryMaxLength);
  }

 private:
  static const size_t kLogEntryMaxLength = 64;
  int current_stack_depth_;
  char event_log_stack_[base::UserLog::kEventStackMaxDepth][kLogEntryMaxLength];

  DISALLOW_COPY_AND_ASSIGN(ScriptEventLog);
};

base::LazyInstance<ScriptEventLog> script_event_log = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void EventListener::HandleEvent(const scoped_refptr<Event>& event,
                                Type listener_type) const {
  TRACE_EVENT1("cobalt::dom", "EventListener::HandleEvent", "Event Name",
               TRACE_STR_COPY(event->type().c_str()));
  script_event_log.Get().PushEvent(event);

  bool had_exception;
  base::optional<bool> result =
      HandleEvent(event->current_target(), event, &had_exception);

  script_event_log.Get().PopEvent();

  if (had_exception) {
    return;
  }
  // EventHandlers (EventListeners set as attributes) may return false rather
  // than call event.preventDefault() in the handler function.
  if (listener_type == kAttribute && result && !result.value()) {
    event->PreventDefault();
  }
}

}  // namespace dom
}  // namespace cobalt
