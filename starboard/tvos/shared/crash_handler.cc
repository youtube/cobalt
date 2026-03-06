// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/tvos/shared/crash_handler.h"

#include <algorithm>
#include <array>
#include <deque>
#include <string>
#include <string_view>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "components/crash/core/common/crash_key.h"
#include "starboard/common/log.h"
#include "starboard/extension/crash_handler.h"

namespace starboard {
namespace {

constexpr size_t kMaxCrashValueSize =
    1024;  // Including the NUL byte at the end.

// A convenient wrapper around a crash key and its name.
// Lifted from android_webview/browser.
class CrashKeyWithName {
 public:
  explicit CrashKeyWithName(std::string name)
      : name_(std::move(name)), crash_key_(name_.c_str()) {}
  CrashKeyWithName(const CrashKeyWithName&) = delete;
  CrashKeyWithName& operator=(const CrashKeyWithName&) = delete;
  CrashKeyWithName(CrashKeyWithName&&) = delete;
  CrashKeyWithName& operator=(CrashKeyWithName&&) = delete;
  ~CrashKeyWithName() = delete;

  std::string_view name() const { return name_; }

  void Clear() { crash_key_.Clear(); }
  void Set(std::string_view value) { crash_key_.Set(value); }

 private:
  std::string name_;
  crash_reporter::CrashKeyString<kMaxCrashValueSize> crash_key_;
};

base::Lock& GetSetStringLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

bool OverrideCrashpadAnnotations(CrashpadAnnotations* crashpad_annotations) {
  return false;  // Deprecated
}

bool SetString(const char* key, const char* value) {
  // These keys are not supposed to be overridable.
  static constexpr std::array<std::string_view, 2> kDisallowedKeys = {
      "prod",
      "ver",
  };

  if (std::find(kDisallowedKeys.begin(), kDisallowedKeys.end(), key) !=
      kDisallowedKeys.end()) {
    return false;
  }

  if (std::string_view(value).size() >= kMaxCrashValueSize) {
    SB_LOG(ERROR) << "Crash value is too large. Ignoring.";
    return false;
  }

  {
    base::AutoLock lock(GetSetStringLock());

    static base::NoDestructor<std::deque<CrashKeyWithName>> runtime_crash_keys;
    auto it =
        std::find_if(runtime_crash_keys->begin(), runtime_crash_keys->end(),
                     [key](const auto& crash_key_string) {
                       return crash_key_string.name() == key;
                     });
    if (it != runtime_crash_keys->end()) {
      it->Set(value);
    } else {
      runtime_crash_keys->emplace_back(key).Set(value);
    }
  }

  return true;
}

const CobaltExtensionCrashHandlerApi kCrashHandlerApi = {
    kCobaltExtensionCrashHandlerName,
    2,
    &OverrideCrashpadAnnotations,
    &SetString,
};

}  // namespace

const void* GetCrashHandlerApi() {
  return &kCrashHandlerApi;
}

}  // namespace starboard
