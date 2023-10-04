// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/log.h"

#include <stdarg.h>

#include "starboard/proxy/starboard_proxy.h"

void SbLogFormat(const char* format, va_list arguments) {
  ::starboard::proxy::log_format_fn_type log_format_test_double =
      ::starboard::proxy::GetSbProxy()->GetLogFormat();
  if (log_format_test_double != NULL) {
    return log_format_test_double(format, arguments);
  } else {
    void* p = ::starboard::proxy::GetSbProxy()->LookupSymbol("SbLogFormat");
    typedef void (*funcptr)(const char*, va_list);
    funcptr sb_log_format = funcptr(p);
    return sb_log_format(format, arguments);
  }
}
