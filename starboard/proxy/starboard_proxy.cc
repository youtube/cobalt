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

#include "starboard/common/log.h"
#include "starboard/extension/runtime_linking.h"
#include "starboard/system.h"

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
  runtime_linking_extension_ =
      static_cast<const CobaltExtensionRuntimeLinkingApi*>(
          SbSystemGetRealExtension(kCobaltExtensionRuntimeLinkingName));
  if (!runtime_linking_extension_) {
    SB_LOG(ERROR) << "No CobaltExtensionRuntimeLinkingApi impl found!";
    return;
  }
  starboard_handle_ =
      runtime_linking_extension_->OpenLibrary("libstarboard_platform_group.so");
}

SbProxy::~SbProxy() {
  runtime_linking_extension_->CloseLibrary(starboard_handle_);
}

void* SbProxy::LookupSymbol(const char* symbol) {
  return runtime_linking_extension_->LookupSymbol(starboard_handle_, symbol);
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

void SbProxy::SetSystemGetExtension(
    system_get_extension_fn_type system_get_extension_fn) {
  system_get_extension_fn_ = system_get_extension_fn;
}

system_get_extension_fn_type SbProxy::GetSystemGetExtension() {
  return system_get_extension_fn_;
}

}  // namespace proxy
}  // namespace starboard
