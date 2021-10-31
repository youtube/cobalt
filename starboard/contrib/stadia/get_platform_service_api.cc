// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/contrib/stadia/get_platform_service_api.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

#include "cobalt/extension/platform_service.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/string.h"
#include "starboard/contrib/stadia/stadia_interface.h"
#include "starboard/event.h"
#include "starboard/memory.h"
#include "starboard/window.h"

namespace starboard {
namespace contrib {
namespace stadia {

namespace {

namespace {
// Encapsulates a channel name and data to be sent on that channel.
struct StadiaPluginSendToData {
  StadiaPlugin* plugin;
  std::vector<uint8_t> data;
  StadiaPluginSendToData(StadiaPlugin* plugin, const std::vector<uint8_t>& data)
      : plugin(plugin), data(data) {}
};
}  // namespace

bool HasPlatformService(const char* name) {
  return g_stadia_interface->StadiaPluginHas(name);
}

CobaltExtensionPlatformService OpenPlatformService(
    void* context,
    const char* name_c_str,
    ReceiveMessageCallback receive_callback) {
  // name_c_str is allocated by Cobalt, but must be freed here.
  std::unique_ptr<const char[]> service_name(name_c_str);

  SB_DCHECK(context);
  SB_LOG(INFO) << "Open " << service_name.get();

  if (!g_stadia_interface->StadiaPluginHas(&service_name[0])) {
    SB_LOG(ERROR) << "Cannot open service. Service not found. "
                  << service_name.get();
    return kCobaltExtensionPlatformServiceInvalid;
  }

  auto std_callback = std::make_unique<
      std::function<void(const std::vector<uint8_t>& message)>>(
      [receive_callback, context](const std::vector<uint8_t>& message) -> void {

        receive_callback(context, message.data(), message.size());

      });

  StadiaPlugin* plugin = g_stadia_interface->StadiaPluginOpen(
      name_c_str,

      [](const uint8_t* const message, size_t length, void* user_data) -> void {

        auto callback = static_cast<
            const std::function<void(const std::vector<uint8_t>& message)>*>(
            user_data);
        std::vector<uint8_t> data(message, message + length);
        (*callback)(data);
      },
      std_callback.release());

  return reinterpret_cast<CobaltExtensionPlatformService>(plugin);
}

void ClosePlatformService(CobaltExtensionPlatformService service) {
  SB_DCHECK(service);
  auto plugin = reinterpret_cast<StadiaPlugin*>(service);
  g_stadia_interface->StadiaPluginClose(plugin);
}

void* SendToPlatformService(CobaltExtensionPlatformService service,
                            void* data,
                            uint64_t length,
                            uint64_t* output_length,
                            bool* invalid_state) {
  SB_DCHECK(service);
  SB_DCHECK(data);
  SB_DCHECK(output_length);
  SB_DCHECK(invalid_state);

  auto plugin = reinterpret_cast<StadiaPlugin*>(service);
  std::vector<uint8_t> buffer(static_cast<uint8_t*>(data),
                              static_cast<uint8_t*>(data) + length);

  auto send_to_plugin_data =
      std::make_unique<StadiaPluginSendToData>(plugin, buffer);

  // Use the main thread.
  SbEventSchedule(
      [](void* context) -> void {
        auto internal_data = std::unique_ptr<StadiaPluginSendToData>(
            static_cast<StadiaPluginSendToData*>(context));
        std::vector<uint8_t> plugin_data(internal_data->data);

        g_stadia_interface->StadiaPluginSendTo(
            internal_data->plugin,
            reinterpret_cast<const char*>(plugin_data.data()),
            plugin_data.size());
      },
      send_to_plugin_data.release(), 0);

  std::vector<uint8_t> response = std::vector<uint8_t>();
  return response.data();
}

const CobaltExtensionPlatformServiceApi kPlatformServiceApi = {
    kCobaltExtensionPlatformServiceName,
    // API version that's implemented.
    1, &HasPlatformService, &OpenPlatformService, &ClosePlatformService,
    &SendToPlatformService,
};
}  // namespace

const void* GetPlatformServiceApi() {
  return g_stadia_interface == nullptr ? nullptr : &kPlatformServiceApi;
}

}  // namespace stadia
}  // namespace contrib
}  // namespace starboard
