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

#include "cobalt/browser/hang_watcher_delegate_impl.h"

#include <cstdint>
#include <optional>

#include "base/logging.h"
#include "cobalt/browser/global_features.h"

namespace cobalt {
namespace browser {

bool CobaltHangWatcherDelegate::IsHangReportingEnabled() {
  std::optional<cobalt::GlobalFeatures::SettingValue> setting =
      cobalt::GlobalFeatures::GetInstance()->GetSetting("EnableHangReporting");
  if (setting.has_value()) {
    if (const auto* val = std::get_if<int64_t>(&setting.value())) {
      return *val != 0;
    }
  }
  return false;
}

}  // namespace browser
}  // namespace cobalt
