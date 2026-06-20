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

#include "build/build_config.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/linux/shared/platform_service.h"
#include "starboard/shared/starboard/feature_list.h"
#include "starboard/shared/starboard/features.h"

namespace starboard {

namespace {
typedef struct SoftMicPlatformServiceImpl : public PlatformServiceImpl {
  // Define additional data field.
  // variable_1, variable_2,...
  SoftMicPlatformServiceImpl(void* context,
                             ReceiveMessageCallback receive_callback)
      : PlatformServiceImpl(context, receive_callback) {}

  // Default constructor.
  SoftMicPlatformServiceImpl() = default;

} SoftMicPlatformServiceImpl;

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
  return strcmp(name, kSoftMicPlatformServiceName) == 0;
}

PlatformServiceImpl* Open(void* context,
                          ReceiveMessageCallback receive_callback) {
  SB_DCHECK(context);
  SB_LOG(INFO) << "Open() service created: " << kSoftMicPlatformServiceName;

  return new SoftMicPlatformServiceImpl(context, receive_callback);
}

void Close(PlatformServiceImpl* service) {
  // Function Close shouldn't manually delete PlatformServiceImpl pointer,
  // because it is managed by unique_ptr in Platform Service.
  SB_LOG(INFO) << "Perform actions before gracefully shutting down the service";
}

void* Send(PlatformServiceImpl* service,
           const void* data,
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

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
    has_hard_mic =
        features::FeatureList::IsEnabled(features::kHasHardMicSupport);
    has_soft_mic =
        features::FeatureList::IsEnabled(features::kHasSoftMicSupport);

    std::string mic_gesture_str = features::kMicGesture.Get();
    mic_gesture_hold = mic_gesture_str == "hold";
    mic_gesture_tap = mic_gesture_str == "tap";
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

    auto mic_gesture = "null";
    if (mic_gesture_hold) {
      mic_gesture = "\"HOLD\"";
    } else if (mic_gesture_tap) {
      mic_gesture = "\"TAP\"";
    }

    auto response = FormatString(
        "{\"hasHardMicSupport\": %s, \"hasSoftMicSupport\": %s, "
        "\"micGesture\": %s}",
        (has_hard_mic ? kJSONTrue : kJSONFalse),
        (has_soft_mic ? kJSONTrue : kJSONFalse), mic_gesture);

    SB_LOG(INFO) << "Send() kGetMicSupport response: " << response;

    // Here we are synchronously calling the receive_callback() from within
    // Send() which is unnecessary. Implementations should prioritize
    // returning from Send() ASAP to avoid blocking the JavaScript thread.
    static_cast<PlatformServiceImpl*>(service)->receive_callback(
        service->context, static_cast<const void*>(response.c_str()),
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
  // to be allocated with malloc() as the caller of Send()
  // in cobalt/h5vcc/h5vcc_platform_service.cc Send() uses
  // free().
  bool* ptr = reinterpret_cast<bool*>(malloc(sizeof(bool)));
  *ptr = valid_message_received;
  *output_length = sizeof(bool);
  return static_cast<void*>(ptr);
}

const CobaltPlatformServiceApi kSoftMicPlatformServiceApi = {
    kSoftMicPlatformServiceName,
    1,  // API version that's implemented.
    &Has,
    &Open,
    &Close,
    &Send};

}  // namespace

const void* GetSoftMicPlatformServiceApi() {
  return &kSoftMicPlatformServiceApi;
}

}  // namespace starboard
