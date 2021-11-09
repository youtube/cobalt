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

#include "starboard/linux/shared/soft_mic_platform_service.h"

#include <string>

#include "cobalt/extension/platform_service.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/shared/starboard/application.h"

typedef struct CobaltExtensionPlatformServicePrivate {
  void* context;
  ReceiveMessageCallback receive_callback;
  const char* name;

  ~CobaltExtensionPlatformServicePrivate() {
    if (name) {
      delete[] name;
    }
  }
} CobaltExtensionPlatformServicePrivate;

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace shared {

namespace {

const char kGetMicSupport[] = "\"getMicSupport\"";
const char kNotifySearchActive[] = "\"notifySearchActive\"";
const char kNotifySearchInactive[] = "\"notifySearchInactive\"";
const char kHasHardMicSupport[] = "has_hard_mic_support";
const char kHasSoftMicSupport[] = "has_soft_mic_support";

// Helper method for constructing JSON response string.
inline const char* boolToChar(bool value) {
  return value ? "true" : "false";
}

bool Has(const char* name) {
  // Check if platform has service name.
  SB_LOG(INFO) << "Has(): " << name;
  return strcmp(name, "com.google.youtube.tv.SoftMic") == 0;
}

CobaltExtensionPlatformService Open(void* context,
                                    const char* name,
                                    ReceiveMessageCallback receive_callback) {
  SB_DCHECK(context);
  if (!Has(name)) {
    SB_LOG(ERROR) << "Cannot open service, does not exist: " << name;
    return kCobaltExtensionPlatformServiceInvalid;
  }

  CobaltExtensionPlatformService service =
      new CobaltExtensionPlatformServicePrivate(
          {context, receive_callback, name});
  SB_LOG(INFO) << "Open() Service created: " << name;
  return service;
}

void Close(CobaltExtensionPlatformService service) {
  SB_LOG(INFO) << "Close() Service";
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

  char* message = new char[length + 1];
  for (auto i = 0; i < length; i++)
    message[i] = *(static_cast<char*>(data) + i);
  message[length] = '\0';

  SB_LOG(INFO) << "Send() message: " << message;

  if (strcmp(message, kGetMicSupport) == 0) {
    // Process "getMicSupport" web app message.
    SB_LOG(INFO) << "Send() kGetMicSupport message received";

    auto has_hard_mic = false;
    auto has_soft_mic = true;

#if !defined(COBALT_BUILD_TYPE_GOLD)
    using shared::starboard::Application;

    // Check for explicit true or false switch value for kHasHardMicSupport and
    // kHasSoftMicSupport optional target params. If neither are set use
    // defaults.
    auto command_line = Application::Get()->GetCommandLine();

    auto hard_mic_switch_value =
        command_line->GetSwitchValue(kHasHardMicSupport);
    if (strcmp("true", hard_mic_switch_value.c_str()) == 0) {
      has_hard_mic = true;
    } else if (strcmp("false", hard_mic_switch_value.c_str()) == 0) {
      has_hard_mic = false;
    }

    auto soft_mic_switch_value =
        command_line->GetSwitchValue(kHasSoftMicSupport);
    if (strcmp("true", soft_mic_switch_value.c_str()) == 0) {
      has_soft_mic = true;
    } else if (strcmp("false", soft_mic_switch_value.c_str()) == 0) {
      has_soft_mic = false;
    }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

    auto response = FormatString(
        "{\"hasHardMicSupport\": %s, \"hasSoftMicSupport\": %s}",
        (has_hard_mic ? "true" : "false"), (has_soft_mic ? "true" : "false"));

    // Here we are synchronously calling the receive_callback() from within
    // Send() which is unnecessary. Implementations should prioritize
    // returning from Send() ASAP to avoid blocking the JavaScript thread.
    static_cast<CobaltExtensionPlatformServicePrivate*>(service)
        ->receive_callback(service->context, response.c_str(),
                           response.length());
  } else if (strcmp(message, kNotifySearchActive) == 0) {
    // Process "notifySearchActive" web app message.
    SB_LOG(INFO) << "Send() kNotifySearchActive message received";
  } else if (strcmp(message, kNotifySearchInactive) == 0) {
    // Process "notifySearchInactive" web app message.
    SB_LOG(INFO) << "Send() kNotifySearchInactive message received";
  }

  delete[] message;
  // The web app does not expect a synchronous response.
  return nullptr;
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
