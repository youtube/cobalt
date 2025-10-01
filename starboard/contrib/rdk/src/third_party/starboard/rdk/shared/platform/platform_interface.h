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

#pragma once

#include "starboard/configuration.h"

#include <cstdint>
#include <string>
#include <utility>
#include <optional>

struct SbAccessibilityCaptionSettings;
struct SbAccessibilityDisplaySettings;
struct SbMediaAudioConfiguration;

namespace third_party::starboard::rdk::shared::platform {

struct HDRFormat {
  bool hdr10{false};
  bool hdr10Plus{false};
  bool dolbyVision{false};
  bool hlg{false};
};

struct Resolution {
  Resolution() = default;
  Resolution(int32_t w, int32_t h) : width(w), height(h) {}
  int32_t width{1920};
  int32_t height{1080};
};

// Interface for querying device configuration and its properties.
class IDevice {
public:
  virtual ~IDevice() = default;

  // Get max supported video resolution of the currently connected device and
  // display.
  virtual std::optional<Resolution> video_resolution() = 0;

  // Get the diagonal size in inches of the currently connected display.
  virtual std::optional<float> diagonal_size_in_inches() = 0;

  // Get HDR formats of the currently connected device and display.
  virtual std::optional<HDRFormat> hdr() = 0;

  // Get audio output configuration at specified index.
  virtual std::optional<bool> audio_configuration(
    int index,
    SbMediaAudioConfiguration* out_audio_configuration) = 0;

  // Get device brand name.
  virtual std::optional<std::string> brand_name() = 0;

  // Get device chipset name.
  virtual std::optional<std::string> chipset() = 0;

  // Get the device type (TV, STB, OTT, etc).
  virtual std::optional<std::string> device_type() = 0;

  // Get the firmware version of the device.
  virtual std::optional<std::string> firmware_version() = 0;

  // Check if the device has a wireless connection.
  virtual std::optional<bool> is_connection_type_wireless() = 0;

  // Check if the device is currently disconnected.
  virtual std::optional<bool> is_disconnected() = 0;
};

// Interface for voice guidance.
class ITextToSpeech {
public:
  virtual ~ITextToSpeech() = default;

  // Cancel most recent speech request.
  virtual std::optional<bool> cancel() = 0;

  // Speak given text.
  virtual std::optional<bool> speak(const std::string& text) = 0;

  // Returns true if device has TTS service available.
  virtual std::optional<bool> is_available() = 0;

  // Returns true if device has TTS enabled.
  virtual std::optional<bool> is_enabled() = 0;
};

// Interface for querying accessibility settings.
class IAccessibility {
public:
  virtual ~IAccessibility() = default;

  // Get display accessibility settings.
  virtual std::optional<bool> display_settings(SbAccessibilityDisplaySettings& out) = 0;

  // Get caption accessibility settings.
  virtual std::optional<bool> caption_settings(SbAccessibilityCaptionSettings& out) = 0;
};

class PlatformInterface {
 protected:
  PlatformInterface() = default;
 public:
  static PlatformInterface& get();

  virtual ~PlatformInterface() = default;
  virtual void teardown() = 0;
  virtual void suspend() = 0;
  virtual void resume() = 0;
  virtual IDevice& device() = 0;
  virtual ITextToSpeech& text_to_speech() = 0;
  virtual IAccessibility& accessibility() = 0;
};

inline IDevice& device() {
  return PlatformInterface::get().device();
}

inline ITextToSpeech& text_to_speech() {
  return PlatformInterface::get().text_to_speech();
}

inline IAccessibility& accessibility() {
  return PlatformInterface::get().accessibility();
}

}  // namespace third_party::starboard::rdk::shared::platform

namespace starboard {
namespace platform = ::third_party::starboard::rdk::shared::platform;
}  // namespace starboard
