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
#include "third_party/crashpad/crashpad/client/annotation.h"

namespace starboard {
namespace {

// This value is somewhat arbitrary. In Breakpad, the maximum value was 2550
// bytes (see doc comment in breakpad/src/common/long_string_dictionary.h), and
// in Crashpad it is 20480 bytes (see crashpad::Annotation::kValueMaxSize).
// Stick to the Breakpad maximum since that is what the tvOS port used up to
// Cobalt 25.
constexpr size_t kMaxCrashValueSize =
    2550;  // Including the NUL byte at the end.

// A convenient wrapper around a crash key and its name.
// Lifted from android_webview/browser.
class CrashKeyWithName {
 public:
  explicit CrashKeyWithName(std::string&& name)
      : name_(std::move(name)), crash_key_(name_.c_str()) {}
  CrashKeyWithName(const CrashKeyWithName&) = delete;
  CrashKeyWithName& operator=(const CrashKeyWithName&) = delete;
  CrashKeyWithName(CrashKeyWithName&&) = delete;
  CrashKeyWithName& operator=(CrashKeyWithName&&) = delete;

  // This class is supposed to be used with base::NoDestructor in
  // SetStringImpl(). It is never supposed to be destroyed since it contains
  // annotations that must be valid for the entire lifetime of the process.
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

bool SetStringImpl(std::string_view key, std::string_view value) {
  // These keys are not supposed to be overridable.
  static constexpr std::array<std::string_view, 2> kDisallowedKeys = {
      "prod",
      "ver",
  };

  if (std::find(kDisallowedKeys.begin(), kDisallowedKeys.end(), key) !=
      kDisallowedKeys.end()) {
    SB_LOG(ERROR) << "Ignoring disallowed key " << key;
    return false;
  }

  // We use >= rather than just > in the comparisons below to account for the
  // NUL byte at the end of the C strings that Crashpad will ultimately use.
  if (key.size() >= crashpad::Annotation::kNameMaxLength) {
    SB_LOG(ERROR) << "Crash key " << key << " is too long. Ignoring.";
    return false;
  }
  if (value.size() >= kMaxCrashValueSize) {
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
      runtime_crash_keys->emplace_back(std::string(key)).Set(value);
    }
  }

  return true;
}

bool SetString(const char* key, const char* value) {
  SB_CHECK(key);
  SB_CHECK(value);
  return SetStringImpl(key, value);
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
