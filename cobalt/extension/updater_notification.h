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

#ifndef COBALT_EXTENSION_UPDATER_NOTIFICATION_H_
#define COBALT_EXTENSION_UPDATER_NOTIFICATION_H_

#if SB_API_VERSION <= 14
#include "starboard/extension/updater_notification.h"
#else
#error "Extensions have moved, please see Starboard CHANGELOG for details."
#endif

#endif  // COBALT_EXTENSION_UPDATER_NOTIFICATION_H_
