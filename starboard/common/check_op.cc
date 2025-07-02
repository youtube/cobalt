// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/check_op.h"

#include <string.h>

#include <cstdio>
#include <cstdlib>
#include <sstream>

// Copied from base/check_op.cc.

namespace starboard::logging {

char* CheckOpValueStr(int v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%d", v);
  return strdup(buf);
}

char* CheckOpValueStr(unsigned v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%u", v);
  return strdup(buf);
}

char* CheckOpValueStr(long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%ld", v);
  return strdup(buf);
}

char* CheckOpValueStr(unsigned long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%lu", v);
  return strdup(buf);
}

char* CheckOpValueStr(long long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%lld", v);
  return strdup(buf);
}

char* CheckOpValueStr(unsigned long long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%llu", v);
  return strdup(buf);
}

char* CheckOpValueStr(const void* v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%p", v);
  return strdup(buf);
}

char* CheckOpValueStr(std::nullptr_t v) {
  return strdup("nullptr");
}

char* CheckOpValueStr(const std::string& v) {
  return strdup(v.c_str());
}

char* CheckOpValueStr(double v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%.6lf", v);
  return strdup(buf);
}

char* StreamValToStr(const void* v,
                     void (*stream_func)(std::ostream&, const void*)) {
  std::stringstream ss;
  stream_func(ss, v);
  return strdup(ss.str().c_str());
}

// Copied from base/check.cc
LogMessage* CheckOpResult::CreateLogMessage(const char* file,
                                            int line,
                                            const char* expr_str,
                                            char* v1_str,
                                            char* v2_str) {
  LogMessage* const log_message = new LogMessage(file, line, SB_LOG_FATAL);
  log_message->stream() << "Check failed: " << expr_str << " (" << v1_str
                        << " vs. " << v2_str << ")";
  free(v1_str);
  free(v2_str);
  return log_message;
}

}  // namespace starboard::logging
