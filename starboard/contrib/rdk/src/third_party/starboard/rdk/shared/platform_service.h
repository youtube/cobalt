//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0//
// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_STARBOARD_RDK_SHARED_PLATFORM_SERVICE_H_
#define THIRD_PARTY_STARBOARD_RDK_SHARED_PLATFORM_SERVICE_H_

#include <memory>
#include <string>

#include "starboard/extension/platform_service.h"

typedef struct PlatformServiceImpl {
  void* context;
  ReceiveMessageCallback receive_callback;

  PlatformServiceImpl(void* context, ReceiveMessageCallback receive_callback)
      : context(context), receive_callback(receive_callback) {}

  PlatformServiceImpl() = default;
} PlatformServiceImpl;

typedef struct CobaltPlatformServiceApi {
  const char* name;
  uint32_t version;
  bool (*Has)(const char* name);
  PlatformServiceImpl* (*Open)(void* context,
                               ReceiveMessageCallback receive_callback);
  void (*Close)(PlatformServiceImpl* service);
  void* (*Send)(PlatformServiceImpl* service,
                const void* data,
                uint64_t length,
                uint64_t* output_length,
                bool* invalid_state);
} CobaltPlatformServiceApi;

namespace starboard {

const void* GetPlatformServiceApi();

}  // namespace starboard

#endif  // THIRD_PARTY_STARBOARD_RDK_SHARED_PLATFORM_SERVICE_H_
