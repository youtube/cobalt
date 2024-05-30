// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/record_loader_app_status.h"

#include <cstring>

#include "starboard/system.h"

namespace starboard {
namespace loader_app {

void RecordSlotSelectionStatus(SlotSelectionStatus status) {
  auto metrics_extension =
      static_cast<const StarboardExtensionLoaderAppMetricsApi*>(
          SbSystemGetExtension(kStarboardExtensionLoaderAppMetricsName));
  if (metrics_extension &&
      strcmp(metrics_extension->name,
             kStarboardExtensionLoaderAppMetricsName) == 0 &&
      metrics_extension->version >= 3) {
    metrics_extension->SetSlotSelectionStatus(status);
  }
}

}  // namespace loader_app
}  // namespace starboard
