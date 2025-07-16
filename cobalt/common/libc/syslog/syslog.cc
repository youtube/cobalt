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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {
char g_ident[128];
}

void openlog(const char* ident, int option, int facility) {
  if (ident) {
    strncpy(g_ident, ident, sizeof(g_ident) - 1);
    g_ident[sizeof(g_ident) - 1] = '\0';
  } else {
    g_ident[0] = '\0';
  }
}

void syslog(int priority, const char* format, ...) {
  char syslog_message[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(syslog_message, sizeof(syslog_message), format, args);
  va_end(args);

  if (g_ident[0] != '\0') {
    printf("%s: %s\n", g_ident, syslog_message);
  } else {
    printf("%s\n", syslog_message);
  }
}

void closelog(void) {
  g_ident[0] = '\0';
}
