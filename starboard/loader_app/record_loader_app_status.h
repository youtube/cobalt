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

#ifndef STARBOARD_LOADER_APP_RECORD_LOADER_APP_STATUS_H_
#define STARBOARD_LOADER_APP_RECORD_LOADER_APP_STATUS_H_

#include "starboard/extension/loader_app_metrics.h"

namespace loader_app {

// Persist the slot selection status so that it can be read in the Cobalt layer
void RecordSlotSelectionStatus(SlotSelectionStatus status);

}  // namespace loader_app

#endif  // STARBOARD_LOADER_APP_RECORD_LOADER_APP_STATUS_H_
