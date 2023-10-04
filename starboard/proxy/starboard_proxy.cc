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

#include "starboard/proxy/starboard_proxy.h"

#include <dlfcn.h>
#include <stdarg.h>

namespace starboard {
namespace proxy {

SbProxy* GetSbProxy() {
  static auto* const sb_proxy = []() {
    auto* inner_sb_proxy = new SbProxy;
    return inner_sb_proxy;
  }();

  return sb_proxy;
}

SbProxy::SbProxy() {
  // If we keep this demand loading strategy, we may be able to add a new
  // demand loading Starboard API or extension to make it portable. jfoks@
  // pointed out that some of our platforms won't support dlopen/dlsym.
  starboard_handle_ = dlopen("libstarboard_platform_group.so", RTLD_NOW);
}

SbProxy::~SbProxy() {
  dlclose(starboard_handle_);
}

void* SbProxy::LookupSymbol(const char* symbol) {
  return dlsym(starboard_handle_, symbol);
}

void SbProxy::SetFileDelete(file_delete_fn_type file_delete_fn) {
  file_delete_fn_ = file_delete_fn;
}

file_delete_fn_type SbProxy::GetFileDelete() {
  return file_delete_fn_;
}

void SbProxy::SetLogFormat(log_format_fn_type log_format_fn) {
  log_format_fn_ = log_format_fn;
}

log_format_fn_type SbProxy::GetLogFormat() {
  return log_format_fn_;
}

}  // namespace proxy
}  // namespace starboard
