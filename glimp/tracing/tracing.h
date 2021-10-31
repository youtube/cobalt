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

#ifndef GLIMP_TRACING_TRACING_H_
#define GLIMP_TRACING_TRACING_H_

#if !defined(ENABLE_GLIMP_TRACING)

#define GLIMP_TRACE_EVENT0(event)

#else  // !defined(ENABLE_GLIMP_TRACING)

#define GLIMP_TRACE_EVENT0(event) \
  glimp::ScopedTraceEvent profileScope##__LINE__(event)

namespace glimp {

class ScopedTraceEvent {
 public:
  explicit ScopedTraceEvent(const char* name);
  ~ScopedTraceEvent();

 private:
  const char* name_;
};

// Clients should implement TraceEventImpl so that the desired activities occur
// when we start and end a trace.  For example, this might get hooked up to
// TRACE_EVENT_BEGIN0() and TRACE_EVENT_END0() if Chromium's base trace_event
// system is available.  Then, SetTraceEventImplementation() should be called
// by the client to make install the client's TraceEventImpl.
class TraceEventImpl {
 public:
  virtual void BeginTrace(const char* name) = 0;
  virtual void EndTrace(const char* name) = 0;
};

void SetTraceEventImplementation(TraceEventImpl* impl);

}  // namespace glimp

#endif  // !defined(ENABLE_GLIMP_TRACING)

#endif  // GLIMP_TRACING_TRACING_H_
