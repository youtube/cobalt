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

#include "starboard/android/shared/platform_service.h"

#import <UIKit/UIKit.h>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/extension/platform_service.h"
#import "starboard/tvos/shared/starboard_application.h"

typedef struct CobaltExtensionPlatformServicePrivate {
  void* Send(void* data,
             uint64_t length,
             uint64_t* output_length,
             bool* invalid_state) {
    std::string string = starboard::FormatString(
        "MaximumFramesPerSecond:%ld;DisplayRefreshRate:%f;",
        UIScreen.mainScreen.maximumFramesPerSecond,
        SBDGetApplication().displayRefreshRate);
    *output_length = string.length() + 1;
    char* output = new char[*output_length];
    strcpy(output, string.c_str());
    return output;
  }

  void* context;
  ReceiveMessageCallback receive_callback;
} CobaltExtensionPlatformServicePrivate;

namespace starboard {
namespace shared {
namespace uikit {

namespace {

const char* kClientLogInfoServiceName = "dev.cobalt.coat.clientloginfo";

bool Has(const char* name) {
  if (!name) {
    return false;
  }

  if (strcmp(name, kClientLogInfoServiceName) == 0) {
    return true;
  }

  return false;
}

CobaltExtensionPlatformService Open(void* context,
                                    const char* name,
                                    ReceiveMessageCallback receive_callback) {
  SB_DCHECK(context);

  if (!Has(name)) {
    SB_LOG(ERROR) << "Can't open Service " << name;
    return kCobaltExtensionPlatformServiceInvalid;
  }

  CobaltExtensionPlatformService service =
      new CobaltExtensionPlatformServicePrivate({context, receive_callback});
  return service;
}

void Close(CobaltExtensionPlatformService service) {
  if (service == kCobaltExtensionPlatformServiceInvalid) {
    return;
  }

  delete static_cast<CobaltExtensionPlatformServicePrivate*>(service);
}

void* Send(CobaltExtensionPlatformService service,
           void* data,
           uint64_t length,
           uint64_t* output_length,
           bool* invalid_state) {
  SB_DCHECK(data);
  SB_DCHECK(output_length);
  SB_DCHECK(invalid_state);

  return static_cast<CobaltExtensionPlatformServicePrivate*>(service)->Send(
      data, length, output_length, invalid_state);
}

const CobaltExtensionPlatformServiceApi kPlatformServiceApi = {
    kCobaltExtensionPlatformServiceName,
    1,  // API version that's implemented.
    &Has,
    &Open,
    &Close,
    &Send};

}  // namespace

const void* GetPlatformServiceApi() {
  return &kPlatformServiceApi;
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
