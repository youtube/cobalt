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

#ifndef STARBOARD_PROXY_STARBOARD_PROXY_H_
#define STARBOARD_PROXY_STARBOARD_PROXY_H_

#include <stdarg.h>

#include "starboard/file.h"
#include "starboard/log.h"

namespace starboard {
namespace proxy {

typedef bool (*file_delete_fn_type)(const char*);
typedef void (*log_format_fn_type)(const char*, va_list);

class SbProxy {
 public:
  SbProxy();
  ~SbProxy();

  void* LookupSymbol(const char* symbol);

  // SetFoo configures the proxy's implementation of SbFoo to delegate to the
  // provided test double instead of the platform's real SbFoo implementation.
  // NULL can be passed to SetFoo to reset the proxy so that its SbFoo once
  // again delegates to the platform's real SbFoo.
  void SetFileDelete(file_delete_fn_type file_delete_fn);

  // GetFoo returns the test double that's been registered in place of the
  // platform's real SbFoo implementation. NULL is returned if no test double is
  // currently registered.
  file_delete_fn_type GetFileDelete();

  void SetLogFormat(log_format_fn_type);
  log_format_fn_type GetLogFormat();

 private:
  void* starboard_handle_;

  file_delete_fn_type file_delete_fn_ = NULL;
  log_format_fn_type log_format_fn_ = NULL;
};

SbProxy* GetSbProxy();

}  // namespace proxy
}  // namespace starboard

#endif  // STARBOARD_PROXY_STARBOARD_PROXY_H_
