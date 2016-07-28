/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/console.h"

#include "base/logging.h"

namespace cobalt {
namespace dom {

namespace {
const char kDebugString[] = "debug";
const char kErrorString[] = "error";
const char kInfoString[] = "info";
const char kLogString[] = "log";
const char kWarningString[] = "warning";
}  // namespace

Console::Listener::Listener(Console* console)
    : console_(console->GetWeakPtr()) {
  DCHECK(console_);
  console_->AddListener(this);
}

Console::Listener::~Listener() {
  if (console_) {
    console_->RemoveListener(this);
  }
}

Console::Console(script::ExecutionState* execution_state)
    : execution_state_(execution_state),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {}

void Console::Log(const std::string& text) const {
  LOG(INFO) << "[console.log()] " << text;
  NotifyListeners(text, kLog);
}

void Console::Trace() const { LOG(INFO) << execution_state_->GetStackTrace(); }

// static
const char* Console::GetLevelAsString(Level level) {
  switch (level) {
    case kDebug:
      return kDebugString;
    case kError:
      return kErrorString;
    case kInfo:
      return kInfoString;
    case kLog:
      return kLogString;
    case kWarning:
      return kWarningString;
    default:
      NOTREACHED();
      return "";
  }
}

void Console::AddListener(Listener* listener) { listeners_.insert(listener); }

void Console::RemoveListener(Listener* listener) { listeners_.erase(listener); }

void Console::NotifyListeners(const std::string& message, Level level) const {
  for (ListenerSet::const_iterator it = listeners_.begin();
       it != listeners_.end(); ++it) {
    (*it)->OnMessage(message, level);
  }
}

}  // namespace dom
}  // namespace cobalt
