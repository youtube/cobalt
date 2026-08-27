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
// SPDX-License-Identifier: Apache-2.0

#ifndef THIRD_PARTY_STARBOARD_RDK_SHARED_RDKSERVICES_H_
#define THIRD_PARTY_STARBOARD_RDK_SHARED_RDKSERVICES_H_

#include "starboard/configuration.h"
#include "third_party/starboard/rdk/shared/platform/platform_interface.h"

#include <string>

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

class Accessibility {
public:
  static void SetSettings(const std::string& json, bool notify_app);
  static bool GetSettings(std::string& out_json);
};

namespace platform {

class RDKServicesInterface final : public PlatformInterface {
private:
  struct RDKDevice final : public IDevice {
    std::optional<Resolution> video_resolution() override;
    std::optional<float> diagonal_size_in_inches() override;
    std::optional<HDRFormat> hdr() override;
    std::optional<bool> audio_configuration(int index, SbMediaAudioConfiguration* out) override;
    std::optional<std::string> brand_name() override;
    std::optional<std::string> chipset() override;
    std::optional<std::string> device_type() override;
    std::optional<std::string> firmware_version() override;
    std::optional<bool> is_connection_type_wireless() override;
    std::optional<bool> is_disconnected() override;
  };

  struct RDKTextToSpeech final : public ITextToSpeech {
    std::optional<bool> cancel() override;
    std::optional<bool> speak(const std::string& text) override;
    std::optional<bool> is_available() override;
    std::optional<bool> is_enabled() override;
  };

  struct RDKAccessibility final : public IAccessibility {
    std::optional<bool> display_settings(SbAccessibilityDisplaySettings& out) override;
    std::optional<bool> caption_settings(SbAccessibilityCaptionSettings& out) override;
  };

  struct RDKAdvertising final : public IAdvertising {
    std::optional<Ifa> advertising_id() override;
  };
public:
  RDKServicesInterface();

  void teardown() override;
  void suspend() override;
  void resume() override;

  IDevice& device() override { return device_; }
  ITextToSpeech& text_to_speech() override { return text_to_speech_; }
  IAccessibility& accessibility() override { return accessibility_; }
  IAdvertising& advertising() override { return advertising_; }

  static bool is_available();
private:
  RDKDevice device_;
  RDKTextToSpeech text_to_speech_;
  RDKAccessibility accessibility_;
  RDKAdvertising advertising_;
};

}  // namespace platform
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

namespace starboard {
using ::third_party::starboard::rdk::shared::Accessibility;
namespace platform = ::third_party::starboard::rdk::shared::platform;
}  // namespace starboard

#endif  // THIRD_PARTY_STARBOARD_RDK_SHARED_RDKSERVICES_H_
