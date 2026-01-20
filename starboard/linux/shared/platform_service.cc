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
#include "starboard/linux/shared/echo_service.h"
#include "starboard/linux/shared/pre_app_recommendation_service.h"
#include "starboard/linux/shared/soft_mic_platform_service.h"
#include "starboard/shared/starboard/application.h"
#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/elf_loader/evergreen_config.h"
#endif  // SB_IS(EVERGREEN_COMPATIBLE)

namespace starboard {

namespace {

struct cmp_str {
  bool operator()(char const* a, char const* b) const {
    return std::strcmp(a, b) < 0;
  }
};

std::map<const char*, const void* (*)(), cmp_str> platform_service_registry = {
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
    {kEchoServiceName, GetEchoServiceApi},
#endif
    {kSoftMicPlatformServiceName, GetSoftMicPlatformServiceApi},
    {kPreappRecommendationServiceName, GetPreappRecommendationServiceApi}};

bool Has(const char* name) {
  // Checks whether Cobalt platform service registry has service name
  // registered.
  if (platform_service_registry.find(name) == platform_service_registry.end()) {
    SB_LOG(ERROR) << "service name isn't registered: " << name;
    return false;
  }

  const CobaltPlatformServiceApi* api =
      reinterpret_cast<const CobaltPlatformServiceApi*>(
          platform_service_registry.at(name)());

  return api->Has(name);
}

CobaltExtensionPlatformService Open(void* context,
                                    const char* name,
                                    ReceiveMessageCallback receive_callback) {
  SB_DCHECK(context);

  CobaltExtensionPlatformService service;

  if (!Has(name)) {
    SB_LOG(ERROR) << "Open() failed in service: " << name;
    service = kCobaltExtensionPlatformServiceInvalid;
  } else {
    const CobaltPlatformServiceApi* api =
        reinterpret_cast<const CobaltPlatformServiceApi*>(
            platform_service_registry.at(name)());

    service = new CobaltExtensionPlatformServicePrivate(
        {context, receive_callback, (std::string)name});

    service->platform_service_impl = std::unique_ptr<PlatformServiceImpl>(
        api->Open(context, receive_callback));
    SB_LOG(INFO) << "Open() created service: " << name;
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

  const CobaltPlatformServiceApi* api =
      reinterpret_cast<const CobaltPlatformServiceApi*>(
          platform_service_registry.at(service->name.c_str())());
  api->Close(service->platform_service_impl.get());

  delete static_cast<CobaltExtensionPlatformServicePrivate*>(service);
}
void* Send(CobaltExtensionPlatformService service,
           void* data,
           uint64_t length,
           uint64_t* output_length,
           bool* invalid_state) {
  SB_DCHECK(service);
  SB_DCHECK(output_length);

  const CobaltPlatformServiceApi* api =
      reinterpret_cast<const CobaltPlatformServiceApi*>(
          platform_service_registry.at(service->name.c_str())());
  return api->Send(service->platform_service_impl.get(), data, length,
                   output_length, invalid_state);
}

const CobaltExtensionPlatformServiceApi kPlatformServiceApi = {
    kCobaltExtensionPlatformServiceName,
    1,  // API version that's implemented.
    &Has,
    &Open,
    &Close,
    &Send};

}  // namespace

const void* GetPlatformServiceApiLinux() {
  return &kPlatformServiceApi;
}

}  // namespace starboard
