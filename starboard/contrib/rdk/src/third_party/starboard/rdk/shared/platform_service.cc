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

#include "third_party/starboard/rdk/shared/platform_service.h"

#include <cstring>
#include <memory>
#include <string>

#include "starboard/configuration.h"
#include "starboard/extension/platform_service.h"

#include "third_party/starboard/rdk/shared/log_override.h"

const char kCobaltExtensionContentEntitlementName[] = "com.google.youtube.tv.ContentEntitlement";

typedef struct CobaltExtensionPlatformServicePrivate {
  void* context;
  ReceiveMessageCallback receive_callback;
  CobaltExtensionPlatformServicePrivate(void* context, ReceiveMessageCallback receive_callback)
    : context(context), receive_callback(receive_callback) {};
  virtual ~CobaltExtensionPlatformServicePrivate() = default;
  virtual void* Send(void* data, uint64_t data_length, uint64_t* output_length, bool* invalid_state) = 0;
} CobaltExtensionPlatformServicePrivate;

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

namespace {

struct ContentEntitlementCobaltExtensionPlatformService : public CobaltExtensionPlatformServicePrivate {

  ContentEntitlementCobaltExtensionPlatformService(void* context, ReceiveMessageCallback callback)
    : CobaltExtensionPlatformServicePrivate(context, callback) {
  }

  void* Send(void* data, uint64_t length, uint64_t* output_length, bool* invalid_state) override {
    bool result = false;
    bool* ptr = reinterpret_cast<bool*>(malloc(sizeof(bool)));
    *ptr = result;
    return static_cast<void*>(ptr);
  }
};

bool Has(const char* name) {
  // Check if platform has service name.
  bool result =  false && strcmp(name, kCobaltExtensionContentEntitlementName) == 0;
  SB_LOG(INFO) << "Entitlement Has service called " << name << " result = " << result;
  return result;
}

CobaltExtensionPlatformService Open(void* context,
                                    const char* name,
                                    ReceiveMessageCallback receive_callback) {
  SB_DCHECK(context);

  CobaltExtensionPlatformService service;

  if (false && strcmp(name, kCobaltExtensionContentEntitlementName) == 0) {
    SB_LOG(INFO) << "Open() service created: " << name;
    service =
      new ContentEntitlementCobaltExtensionPlatformService(context, receive_callback);
  } else {
    SB_LOG(ERROR) << "Open() service name does not exist: " << name;
    service = kCobaltExtensionPlatformServiceInvalid;
  }

  delete[] name;

  return service;
}

void Close(CobaltExtensionPlatformService service) {
  SB_LOG(INFO) << "Close() Service.";
  delete static_cast<CobaltExtensionPlatformServicePrivate*>(service);
}

void* Send(CobaltExtensionPlatformService service,
           void* data,
           uint64_t length,
           uint64_t* output_length,
           bool* invalid_state) {
  SB_DCHECK(data);
  SB_DCHECK(length);
  SB_DCHECK(output_length);
  SB_DCHECK(invalid_state);
  return static_cast<CobaltExtensionPlatformServicePrivate*>(service)->Send(data, length, output_length, invalid_state);
}

const CobaltExtensionPlatformServiceApi kPlatformServiceApi = {
  kCobaltExtensionPlatformServiceName,
  1,  // API version that's implemented.
  &Has,
  &Open,
  &Close,
  &Send
};

}  // namespace

const void* GetPlatformServiceApi() {
  SB_LOG(INFO) << "GetContentEntitlementPlatformServiceApi return ";
  return &kPlatformServiceApi;
}

}  // namespace thirdpary
}  // namespace starboard
}  // namespace rdk
}  // namespace shared
