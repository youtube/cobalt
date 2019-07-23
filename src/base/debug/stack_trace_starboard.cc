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

#include "base/debug/stack_trace.h"

#include <algorithm>
#include <ostream>

#include "starboard/common/log.h"
#include "starboard/system.h"

namespace base {
namespace debug {

namespace {

class BacktraceOutputHandler {
 public:
  virtual void HandleOutput(const char* output) = 0;

 protected:
  virtual ~BacktraceOutputHandler() {}
};

class PrintBacktraceOutputHandler : public BacktraceOutputHandler {
 public:
  PrintBacktraceOutputHandler() {}

  virtual void HandleOutput(const char* output) {
    // NOTE: This code MUST be async-signal safe (it's used by in-process
    // stack dumping signal handler). NO malloc or stdio is allowed here.
    SbLogRaw(output);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintBacktraceOutputHandler);
};

class StreamBacktraceOutputHandler : public BacktraceOutputHandler {
 public:
  StreamBacktraceOutputHandler(std::ostream* os) : os_(os) {}

  virtual void HandleOutput(const char* output) { (*os_) << output; }

 private:
  std::ostream* os_;

  DISALLOW_COPY_AND_ASSIGN(StreamBacktraceOutputHandler);
};

// NOTE: code from sandbox/linux/seccomp-bpf/demo.cc.
char* itoa_r(intptr_t i, char* buf, size_t sz, int base) {
  // Make sure we can write at least one NUL byte.
  size_t n = 1;
  if (n > sz)
    return NULL;

  if (base < 2 || base > 16) {
    buf[0] = '\000';
    return NULL;
  }

  char* start = buf;

  uintptr_t j = i;

  // Handle negative numbers (only for base 10).
  if (i < 0 && base == 10) {
    j = -i;

    // Make sure we can write the '-' character.
    if (++n > sz) {
      buf[0] = '\000';
      return NULL;
    }
    *start++ = '-';
  }

  // Loop until we have converted the entire number. Output at least one
  // character (i.e. '0').
  char* ptr = start;
  do {
    // Make sure there is still enough space left in our output buffer.
    if (++n > sz) {
      buf[0] = '\000';
      return NULL;
    }

    // Output the next digit.
    *ptr++ = "0123456789abcdef"[j % base];
    j /= base;
  } while (j);

  // Terminate the output with a NUL character.
  *ptr = '\000';

  // Conversion to ASCII actually resulted in the digits being in reverse
  // order. We can't easily generate them in forward order, as we can't tell
  // the number of characters needed until we are done converting.
  // So, now, we reverse the string (except for the possible "-" sign).
  while (--ptr > start) {
    char ch = *ptr;
    *ptr = *start;
    *start++ = ch;
  }
  return buf;
}

void OutputPointer(void* pointer, BacktraceOutputHandler* handler) {
  char buf[1024] = {'\0'};
  handler->HandleOutput(" [0x");
  itoa_r(reinterpret_cast<intptr_t>(pointer), buf, sizeof(buf), 16);
  handler->HandleOutput(buf);
  handler->HandleOutput("]");
}

void ProcessBacktrace(void* const* trace,
                      int size,
                      const char* prefix_string,
                      BacktraceOutputHandler* handler) {
  for (int i = 0; i < size; ++i) {
    if (prefix_string)
      handler->HandleOutput(prefix_string);
    handler->HandleOutput("\t");

    char buf[1024] = {'\0'};

    // Subtract by one as return address of function may be in the next
    // function when a function is annotated as noreturn.
    void* address = static_cast<char*>(trace[i]) - 1;
    if (SbSystemSymbolize(address, buf, sizeof(buf))) {
      handler->HandleOutput(buf);
    } else {
      handler->HandleOutput("<unknown>");
    }

    OutputPointer(trace[i], handler);
    handler->HandleOutput("\n");
  }
}

}  // namespace

StackTrace::StackTrace(size_t count) {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.

  // Though the SbSystemGetStack API documentation does not specify any possible
  // negative return values, we take no chance.
  count_ = std::max(SbSystemGetStack(trace_, count), 0);
  if (count_ < 1) {
    return;
  }

  // We can remove this call from the stack trace, since we know it is always
  // going to be in it.
  for (int i = 1; i < count_; ++i) {
    trace_[i - 1] = trace_[i];
  }
}

void StackTrace::PrintWithPrefix(const char* prefix_string) const {
  // NOTE: This code MUST be async-signal safe (it's used by in-process
  // stack dumping signal handler). NO malloc or stdio is allowed here.
  PrintBacktraceOutputHandler handler;
  ProcessBacktrace(trace_, count_, prefix_string, &handler);
}

void StackTrace::OutputToStreamWithPrefix(std::ostream* os,
                                          const char* prefix_string) const {
  StreamBacktraceOutputHandler handler(os);
  ProcessBacktrace(trace_, count_, prefix_string, &handler);
}

bool EnableInProcessStackDumping() {
  return true;
}

}  // namespace debug
}  // namespace base
