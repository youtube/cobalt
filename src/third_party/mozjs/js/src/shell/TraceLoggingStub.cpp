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

#include "TraceLogging.h"

namespace js {

namespace {

TraceLogging shellTraceLogging;

}  // namespace

// Stub TraceLogging functionality
TraceLogging::TraceLogging() {
}

TraceLogging::~TraceLogging() {
}

// static
TraceLogging* TraceLogging::defaultLogger() {
  return &shellTraceLogging;
}

// static
void TraceLogging::releaseDefaultLogger() {
  // Do nothing. Only one global object exists.
}

void TraceLogging::log(Type /* type */,
                       const char* /* filename */,
                       unsigned int /* line */) {
}

void TraceLogging::log(Type /* type */, JSScript* /* script */) {
}

void TraceLogging::log(const char* /* log */) {
}

void TraceLogging::log(Type /* type */) {
}

void TraceLogging::flush() {
}

// Helper functions declared in TraceLogging.h
void TraceLog(TraceLogging* /* logger */,
              TraceLogging::Type /* type */,
              JSScript* /* script */) {
}

void TraceLog(TraceLogging* /* logger */, const char* /* log */) {
}

void TraceLog(TraceLogging* /* logger */, TraceLogging::Type /* type */) {
}

}  // namespace js
