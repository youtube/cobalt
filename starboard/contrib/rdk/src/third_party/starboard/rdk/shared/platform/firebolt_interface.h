//
// Copyright 2025 Comcast Cable Communications Management, LLC
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

#pragma once

#include "starboard/configuration.h"
#include "third_party/starboard/rdk/shared/platform/platform_interface.h"

#include <string>
#include <mutex>
#include <optional>
#include <condition_variable>

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace platform {

class FireboltInterface final : public PlatformInterface {
private:
  struct FireboltDevice final : public IDevice {
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

    void init();
    void unsubscribe();
  private:
    void set_hdr_format(HDRFormat hdr_format);

    std::mutex mutex_;
    std::optional<std::string> device_type_;
    std::optional<std::string> chipset_id_;
    std::optional<HDRFormat> hdr_format_;
    std::optional<uint64_t> on_hdr_format_changed_id_;
  };

  struct FireboltTextToSpeech final : public ITextToSpeech {
    std::optional<bool> cancel() override;
    std::optional<bool> speak(const std::string& text) override;
    std::optional<bool> is_available() override;
    std::optional<bool> is_enabled() override;

    void init();
    void unsubscribe();
  private:
    void set_is_enabled(bool v);

    std::mutex mutex_;
    std::optional<uint64_t> on_speech_complete_id_;
    std::optional<uint64_t> on_vg_settings_changed_id_;
    std::optional<int32_t> speech_id_;
    std::optional<bool> is_available_;
    std::optional<bool> is_enabled_;
  };

  struct FireboltAccessibility final : public IAccessibility {
    std::optional<bool> display_settings(SbAccessibilityDisplaySettings& out) override;
    std::optional<bool> caption_settings(SbAccessibilityCaptionSettings& out) override;

    void unsubscribe();
  private:
    void lazy_init(std::unique_lock<std::mutex>& lock);
    void set_high_contrast_ui(bool enabled);
    void set_cc_enabled(bool enabled);

    std::mutex mutex_;
    bool did_init_ { false };
    bool is_high_contrast_text_enabled_ { false };
    bool is_cc_enabled_ { false };

    std::optional<uint64_t> on_high_contrast_ui_changed_id_;
    std::optional<uint64_t> on_cc_settings_changed_id_;
  };

  struct FireboltAdvertising final : public IAdvertising {
    std::optional<Ifa> advertising_id() override;

  private:
    std::mutex mutex_;
    bool did_init_ { false };
    std::optional<Ifa> cached_ifa_;
  };

public:
  FireboltInterface();

  void teardown() override;
  void suspend() override;
  void resume() override;

  IDevice& device() override;
  ITextToSpeech& text_to_speech() override;
  IAccessibility& accessibility() override;
  IAdvertising& advertising() override;

  static bool is_available();

private:
  void lazy_init();

  std::mutex mutex_;
  std::condition_variable cv_;
  std::optional<bool> connected_;
  FireboltDevice device_;
  FireboltTextToSpeech text_to_speech_;
  FireboltAccessibility accessibility_;
  FireboltAdvertising advertising_;
};

}  // namespace platform
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
