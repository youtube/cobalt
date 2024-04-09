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

#include "starboard/linux/shared/platform_service.h"

#include <map>
#include <memory>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/extension/platform_service.h"
#include "starboard/linux/shared/soft_mic_platform_service.h"
#include "starboard/shared/starboard/application.h"
#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/elf_loader/evergreen_config.h"
#endif  // SB_IS(EVERGREEN_COMPATIBLE)

typedef struct CobaltExtensionPlatformServicePrivate {
  void* context;
  ReceiveMessageCallback receive_callback;
  const char* name;
  CobaltExtensionPlatformServiceApi* cobalt_service;
} CobaltExtensionPlatformServicePrivate;

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace shared {

namespace {

struct cmp_str {
  bool operator()(char const* a, char const* b) const {
    return std::strcmp(a, b) < 0;
  }
};

std::map<const char*, void*, cmp_str> platform_service_registry = {
    {"com.google.youtube.tv.SoftMic",
     const_cast<void*>(GetSoftMicPlatformServiceApi())}};

bool Has(const char* name) {
  // Check if Cobalt platform service registry has service name registered.
  if (platform_service_registry.find(name) != platform_service_registry.end()) {
    return true;
  }
  return false;
}

CobaltExtensionPlatformService Open(void* context,
                                    const char* name,
                                    ReceiveMessageCallback receive_callback) {
  SB_DCHECK(context);

  CobaltExtensionPlatformService service;

  if (!Has(name)) {
    SB_LOG(ERROR) << "Open() service name does not exist: " << name;
    service = kCobaltExtensionPlatformServiceInvalid;
  } else {
    service = new CobaltExtensionPlatformServicePrivate(
        {context, receive_callback, name});
    SB_LOG(INFO) << "Open() service created: " << name;

    if (platform_service_registry.find(name) !=
        platform_service_registry.end()) {
      service->cobalt_service = const_cast<CobaltExtensionPlatformServiceApi*>(
          reinterpret_cast<const CobaltExtensionPlatformServiceApi*>(
              platform_service_registry.at(name)));
    }
  }

#if SB_IS(EVERGREEN_COMPATIBLE)
  const bool is_evergreen = elf_loader::EvergreenConfig::GetInstance() != NULL;
#else
  const bool is_evergreen = false;
#endif  // SB_IS(EVERGREEN_COMPATIBLE)

  // The name parameter memory is allocated in h5vcc_platform_service::Open()
  // with new[] and must be deallocated here.
  // If we are in an Evergreen build, the name parameter must be deallocated
  // with free (), since new[] will map to malloc()
  // in an Evergreen build.
  if (is_evergreen) {
    free((void*)name);  // NOLINT
  } else {
    delete[] name;
  }

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
  SB_DCHECK(service);
  SB_DCHECK(data);
  SB_DCHECK(output_length);

  return service->cobalt_service->Send(service, data, length, output_length,
                                       invalid_state);
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

}  // namespace shared
}  // namespace starboard
