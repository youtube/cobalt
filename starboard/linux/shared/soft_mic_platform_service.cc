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

#include <memory>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/extension/platform_service.h"
#include "starboard/shared/starboard/application.h"
#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/elf_loader/evergreen_config.h"
#endif  // SB_IS(EVERGREEN_COMPATIBLE)

typedef struct CobaltExtensionPlatformServicePrivate {
  void* context;
  ReceiveMessageCallback receive_callback;
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
const char kMicGesture[] = "mic_gesture";
const char kJSONTrue[] = "true";
const char kJSONFalse[] = "false";

bool Has(const char* name) {
  // Check if platform has service name.
  return strcmp(name, "com.google.youtube.tv.SoftMic") == 0;
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
    SB_LOG(INFO) << "Open() service created: " << name;
    service =
        new CobaltExtensionPlatformServicePrivate({context, receive_callback});
  }

#if SB_IS(EVERGREEN_COMPATIBLE)
  const bool is_evergreen = elf_loader::EvergreenConfig::GetInstance() != NULL;
#else
  const bool is_evergreen = false;
#endif  // SB_IS(EVERGREEN_COMPATIBLE)

  // The name parameter memory is allocated in h5vcc_platform_service::Open()
  // with new[] and must be deallocated here.
  // If we are in an Evergreen build, the name parameter must be deallocated
  // with SbMemoryDeallocate(), since new[] will map to SbMemoryAllocate()
  // in an Evergreen build.
  if (is_evergreen) {
    SbMemoryDeallocate((void*)name);  // NOLINT
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

  // This bool flag is set to true if the data argument
  // is successfully parsed as a known web app request,
  // false otherwise. It is then sent as a synchronous
  // response to the web app indicating this success or
  // failure.
  auto valid_message_received = false;

  char message[length + 1];
  std::memcpy(message, data, length);
  message[length] = '\0';

  SB_LOG(INFO) << "Send() message: " << message;

  if (strcmp(message, kGetMicSupport) == 0) {
    // Process "getMicSupport" web app message.
    SB_LOG(INFO) << "Send() kGetMicSupport message received.";

    auto has_hard_mic = false;
    auto has_soft_mic = true;
    auto mic_gesture_hold = false;
    auto mic_gesture_tap = false;

#if !defined(COBALT_BUILD_TYPE_GOLD)
    using shared::starboard::Application;

    // Check for explicit true or false switch value for kHasHardMicSupport,
    // kHasSoftMicSupport, kMicGestureHold, and kMicGestureTap optional target
    // params. Use default values if none are set.
    auto command_line = Application::Get()->GetCommandLine();

    auto hard_mic_switch_value =
        command_line->GetSwitchValue(kHasHardMicSupport);
    if (hard_mic_switch_value == kJSONTrue) {
      has_hard_mic = true;
    } else if (hard_mic_switch_value == kJSONFalse) {
      has_hard_mic = false;
    }

    auto soft_mic_switch_value =
        command_line->GetSwitchValue(kHasSoftMicSupport);
    if (soft_mic_switch_value == kJSONTrue) {
      has_soft_mic = true;
    } else if (soft_mic_switch_value == kJSONFalse) {
      has_soft_mic = false;
    }

    auto mic_gesture_switch_value = command_line->GetSwitchValue(kMicGesture);
    mic_gesture_hold = mic_gesture_switch_value == "hold";
    mic_gesture_tap = mic_gesture_switch_value == "tap";
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

    auto mic_gesture = "null";
    if (mic_gesture_hold)
      mic_gesture = "\"HOLD\"";
    else if (mic_gesture_tap)
      mic_gesture = "\"TAP\"";

    auto response = FormatString(
        "{\"hasHardMicSupport\": %s, \"hasSoftMicSupport\": %s, "
        "\"micGesture\": %s}",
        (has_hard_mic ? kJSONTrue : kJSONFalse),
        (has_soft_mic ? kJSONTrue : kJSONFalse), mic_gesture);

    SB_LOG(INFO) << "Send() kGetMicSupport response: " << response;

    // Here we are synchronously calling the receive_callback() from within
    // Send() which is unnecessary. Implementations should prioritize
    // returning from Send() ASAP to avoid blocking the JavaScript thread.
    static_cast<CobaltExtensionPlatformServicePrivate*>(service)
        ->receive_callback(service->context,
                           static_cast<const void*>(response.c_str()),
                           response.length());

    valid_message_received = true;
  } else if (strcmp(message, kNotifySearchActive) == 0) {
    // Process "notifySearchActive" web app message.
    SB_LOG(INFO) << "Send() kNotifySearchActive message received";
    valid_message_received = true;
  } else if (strcmp(message, kNotifySearchInactive) == 0) {
    // Process "notifySearchInactive" web app message.
    SB_LOG(INFO) << "Send() kNotifySearchInactive message received";
    valid_message_received = true;
  }

  // Synchronous bool response to the web app confirming the message
  // has been successfully parsed by the platform. Response needs
  // to be allocated with SbMemoryAllocate() as the caller of Send()
  // in cobalt/h5vcc/h5vcc_platform_service.cc Send() uses
  // SbMemoryDeallocate().
  bool* ptr = reinterpret_cast<bool*>(SbMemoryAllocate(sizeof(bool)));
  *ptr = valid_message_received;
  *output_length = sizeof(bool);
  return static_cast<void*>(ptr);
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
