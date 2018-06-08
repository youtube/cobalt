// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/script_event_log.h"

#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace dom {

ScriptEventLog::ScriptEventLog() : current_stack_depth_(0) {
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

ScriptEventLog::~ScriptEventLog() {
  base::UserLog::Deregister(base::UserLog::kEventStackLevelCountIndex);
  for (int i = 0; i < base::UserLog::kEventStackMaxDepth; i++) {
    base::UserLog::Deregister(static_cast<base::UserLog::Index>(
        base::UserLog::kEventStackMinLevelIndex + i));
  }
}

void ScriptEventLog::PushEvent(Event* event) {
  if (current_stack_depth_ < base::UserLog::kEventStackMaxDepth) {
    snprintf(event_log_stack_[current_stack_depth_], kLogEntryMaxLength,
             "%s@%s", event->type().c_str(),
             event->current_target()->GetDebugName().c_str());
  } else if (current_stack_depth_ == base::UserLog::kEventStackMaxDepth) {
    DLOG(WARNING) << "Reached maximum depth of " << kLogEntryMaxLength
                  << ". Subsequent events will not be logged.";
  }
  current_stack_depth_++;
}

void ScriptEventLog::PopEvent() {
  DCHECK(current_stack_depth_);
  current_stack_depth_--;
  if (current_stack_depth_ < base::UserLog::kEventStackMaxDepth) {
    memset(event_log_stack_[current_stack_depth_], 0, kLogEntryMaxLength);
  }
}

base::LazyInstance<ScriptEventLog> script_event_log = LAZY_INSTANCE_INITIALIZER;

}  // namespace dom
}  // namespace cobalt
