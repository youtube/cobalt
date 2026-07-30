// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_ANDROID_ANDROIDTV_FLAG_MAP_H_
#define COBALT_ANDROID_ANDROIDTV_FLAG_MAP_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"

namespace base {
class CommandLine;
}  // namespace base

namespace cobalt {

// Android TV flag values handed over from Java (see
// cobalt/android/androidtv_flags.h for the keys).
//
// Immutable value type: built once in the native initialization hook from
// JavaSwitches.takeNativeKeyValues() and passed to ExpandAndroidTvFlags(); it
// owns no long-lived resources. Not thread-safe; only used on the startup
// thread before base::FeatureList is created.
class AndroidTvFlagMap {
 public:
  AndroidTvFlagMap();
  explicit AndroidTvFlagMap(base::flat_map<std::string, std::string> values);
  AndroidTvFlagMap(const AndroidTvFlagMap&);
  AndroidTvFlagMap& operator=(const AndroidTvFlagMap&);
  AndroidTvFlagMap(AndroidTvFlagMap&&);
  AndroidTvFlagMap& operator=(AndroidTvFlagMap&&);
  ~AndroidTvFlagMap();

  // Builds a map from a flat [key0, value0, key1, value1, ...] list, as
  // returned by JavaSwitches.takeNativeKeyValues(). A trailing key without a
  // value maps to an empty string.
  static AndroidTvFlagMap FromKeyValues(
      const std::vector<std::string>& key_values);

  // Returns whether `key` is present, regardless of its value.
  bool Has(std::string_view key) const;

  // Returns the digits of the value for `key`, or nullopt if `key` is absent or
  // its value has no digits.
  std::optional<std::string> GetNumeric(std::string_view key) const;

 private:
  base::flat_map<std::string, std::string> values_;
};

// Expands the Android TV experiment keys in `flag_map` into the Chromium/Cobalt
// switches and features they control. When `flag_map` is empty (no
// experiments, or crash-streak safe mode), only the experiment-controlled
// defaults are applied.
//
// Precedence:
// - Plain switches are only set if absent, so user-provided switches win.
// - Enabled and disabled features are appended after existing
//   --enable-features / --disable-features entries (base::FeatureList is
//   first-wins, so metadata and user features win over experiments).
// - V8 flags are placed before existing --js-flags entries (V8 is last-wins).
//
// Must run before CommandLinePreprocessor::ApplyDefaults().
void ExpandAndroidTvFlags(const AndroidTvFlagMap& flag_map,
                          base::CommandLine* cmd_line);

}  // namespace cobalt

#endif  // COBALT_ANDROID_ANDROIDTV_FLAG_MAP_H_
