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

#ifndef STARBOARD_LINUX_SHARED_TIME_ZONE_H_
#define STARBOARD_LINUX_SHARED_TIME_ZONE_H_

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace shared {

const void* GetTimeZoneApi();

}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_LINUX_SHARED_TIME_ZONE_H_
