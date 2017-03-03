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

#include "third_party/mozjs/js/src/TraceLogging.h"

#include "base/debug/trace_event.h"
#include "base/memory/singleton.h"

namespace js {

namespace {

// Container class to transform the 3rd-party-declared TraceLogging class
// into our Singleton type.
class TraceLoggingContainer {
 public:
  TraceLoggingContainer() {}

  static TraceLoggingContainer* GetInstance() {
    return Singleton<TraceLoggingContainer,
      StaticMemorySingletonTraits<TraceLoggingContainer> >::get();
  }

  TraceLogging* GetLogger() {
    return &logger_;
  }

 private:
  TraceLogging logger_;

  DISALLOW_COPY_AND_ASSIGN(TraceLoggingContainer);
};

const char kTraceLoggingCategory[] = "JavaScript";

}  // namespace

const char* const TraceLogging::type_name[] = {
  "IonCompile",
  "IonCompile",
  "IonCannon",
  "IonCannon",
  "IonCannon",
  "IonSideCannon",
  "IonSideCannon",
  "IonSideCannon",
  "YarrJIT",
  "YarrJIT",
  "JMSafepoint",
  "JMSafepoint",
  "JMNormal",
  "JMNormal",
  "JMCompile",
  "JMCompile",
  "GarbageCollect",
  "GarbageCollect",
  "Interpreter",
  "Interpreter",
  "Info",
};

// None of the member variables will be needed. Instead, logging will be
// funneled to the trace event system.
TraceLogging::TraceLogging() {
}

TraceLogging::~TraceLogging() {
}

// static
TraceLogging* TraceLogging::defaultLogger() {
  return TraceLoggingContainer::GetInstance()->GetLogger();
}

// static
void TraceLogging::releaseDefaultLogger() {
  // Do nothing. Lifetime is managed by the TraceLoggingContainer singleton.
}

void TraceLogging::log(Type type, const char* filename, unsigned int line) {
  // Unfortunately, we don't have access to the enum, so can't declare a
  // "count"-type enum. Instead, INFO is assumed to be the last enumeration.
  COMPILE_ASSERT(ARRAYSIZE_UNSAFE(type_name) - 1 == INFO, array_mismatch);

  switch (type) {
    // "Start" types
    case ION_COMPILE_START:
    case ION_CANNON_START:
    case ION_SIDE_CANNON_START:
    case YARR_JIT_START:
    case JM_SAFEPOINT_START:
    case JM_START:
    case JM_COMPILE_START:
    case GC_START:
    case INTERPRETER_START:
      if (filename != NULL) {
        TRACE_EVENT_BEGIN2(kTraceLoggingCategory, type_name[type],
                          "file", TRACE_STR_COPY(filename),
                          "line", line);
      } else {
        TRACE_EVENT_BEGIN0(kTraceLoggingCategory, type_name[type]);
      }
      break;

    // Ignored types
    case INFO:
      break;

    // "Stop" types
    default:
      TRACE_EVENT_END0(kTraceLoggingCategory, type_name[type]);
      break;
  }
}

void TraceLogging::log(Type type, JSScript* script) {
  log(type, script->filename(), script->lineno);
}

void TraceLogging::log(const char* log) {
  UNREFERENCED_PARAMETER(log);
}

void TraceLogging::log(Type type) {
  log(type, NULL, 0);
}

void TraceLogging::flush() {
}

// Helper functions declared in TraceLogging.h
void TraceLog(TraceLogging* logger, TraceLogging::Type type, JSScript* script) {
  logger->log(type, script);
}

void TraceLog(TraceLogging* logger, const char* log) {
  logger->log(log);
}

void TraceLog(TraceLogging* logger, TraceLogging::Type type) {
  logger->log(type);
}

}  // namespace js
