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
#include "third_party/starboard/rdk/shared/platform/platform_interface.h"

#include <starboard/extension/accessibility.h>
#include <starboard/common/media.h>
#include <starboard/media.h>

#if defined(ENABLE_FIREBOLT_API) && ENABLE_FIREBOLT_API
#include "third_party/starboard/rdk/shared/platform/firebolt_interface.h"
#endif
#if defined(ENABLE_RDKSERVICES_API) && ENABLE_RDKSERVICES_API
#include "third_party/starboard/rdk/shared/rdkservices.h"
#endif
#include "third_party/starboard/rdk/shared/log_override.h"

#include <memory>
#include <mutex>
#include <vector>
#include <functional>
#include <cstring>

namespace third_party::starboard::rdk::shared::platform {

std::ostream& operator<<(std::ostream& out, const HDRFormat& v) {
  return out << "{\"hdr10\":" << v.hdr10
             << ",\"hdr10Plus\":" << v.hdr10Plus
             << ",\"dolbyVision\":" << v.dolbyVision
             << ",\"hlg\":" << v.hlg << '}';
}

std::ostream& operator<<(std::ostream& out, const Resolution& v) {
  return out << '[' << v.width << ", " << v.height << ']';
}

std::ostream& operator<<(std::ostream& out, const SbMediaAudioConnector& v) {
  return out << ::starboard::GetMediaAudioConnectorName(v);
}

std::ostream& operator<<(std::ostream& out,
                         const SbMediaAudioConfiguration& v) {
  return out << "{\"connector\":" << v.connector
             << ",\"latency\":" << v.latency
             << ",\"coding_type\":" << v.coding_type
             << ",\"number_of_channels\":" << v.number_of_channels << '}';
}

std::ostream& operator<<(std::ostream& out, const SbAccessibilityDisplaySettings& s) {
  return out << "{\"has_high_contrast_text_setting\": " << s.has_high_contrast_text_setting
             << ",\"is_high_contrast_text_enabled\": " << s.is_high_contrast_text_enabled << '}';
}

std::ostream& operator<<(std::ostream& out, const SbAccessibilityCaptionSettings& s) {
  return out << "{\"is_enabled\":" << s.is_enabled
             << ",\"supports_is_enabled\":" << s.supports_is_enabled
             << ",\"supports_set_enabled\":" << s.supports_set_enabled << '}';
}

std::ostream& operator<<(std::ostream& out, const Ifa& v) {
  return out << "{\"ifa\":\"" << v.ifa
             << "\",\"ifa_type\":\"" << v.ifa_type
             << "\",\"lmt\":\"" << v.lmt << "\"}";
}

template<typename T>
std::ostream& operator<<(std::ostream& out, const std::optional<T>& v) {
  if (v.has_value())
    return out << *v;
  return out << "(not set)";
}


#define TRACE_INFO() SB_LOG(INFO) << std::boolalpha

class TracePlatformInterface final : public PlatformInterface {

  struct TraceDevice final : public IDevice {
    IDevice& device_;

    TraceDevice(IDevice& device) : device_(device) {}

    std::optional<Resolution> video_resolution() override {
      auto ret = device_.video_resolution();
      TRACE_INFO() << "device.video_resolution: " << ret;
      return ret;
    }

    std::optional<float> diagonal_size_in_inches() override {
      auto ret = device_.diagonal_size_in_inches();
      TRACE_INFO() << "device.diagonal_size_in_inches: " << ret;
      return ret;
    }

    std::optional<HDRFormat> hdr() override {
      auto ret = device_.hdr();
      TRACE_INFO() << "device.hdr: " << ret;
      return ret;
    }

    std::optional<bool> audio_configuration(int index, SbMediaAudioConfiguration* out) override {
      auto ret = device_.audio_configuration(index, out);
      if (ret.value_or(false) && out) {
        TRACE_INFO() << "device.audio_configuration: " << ret
                     << ", index = " << index
                     << ", out_audio_configuration = " << *out;
      } else if (out) {
        TRACE_INFO() << "device.audio_configuration: " << ret
                     << ", index = " << index
                     << ", out_audio_configuration = (uninitialized)";
      } else {
        TRACE_INFO() << "device.audio_configuration: " << ret
                     << ", index = " << index
                     << ", out_audio_configuration = (nil)";
      }
      return ret;
    }

    std::optional<std::string> brand_name() override {
      auto ret = device_.brand_name();
      TRACE_INFO() << "device.brand_name: " << ret;
      return ret;
    }

    std::optional<std::string> chipset() override {
      auto ret = device_.chipset();
      TRACE_INFO() << "device.chipset: " << ret;
      return ret;
    }

    std::optional<std::string> device_type() override {
      auto ret = device_.device_type();
      TRACE_INFO() << "device.device_type: " << ret;
      return ret;
    }

    std::optional<std::string> firmware_version() override {
      auto ret = device_.firmware_version();
      TRACE_INFO() << "device.firmware_version: " << ret;
      return ret;
    }

    std::optional<bool> is_connection_type_wireless() override {
      auto ret = device_.is_connection_type_wireless();
      TRACE_INFO() << "device.is_connection_type_wireless: " << ret;
      return ret;
    }

    std::optional<bool> is_disconnected() override {
      auto ret = device_.is_disconnected();
      TRACE_INFO() << "device.is_disconnected: " << ret;
      return ret;
    }
  };

  struct TraceTextToSpeech final : public ITextToSpeech {
    ITextToSpeech& tts_;

    TraceTextToSpeech(ITextToSpeech& tts) : tts_(tts) {}

    std::optional<bool> cancel() override {
      auto ret = tts_.cancel();
      TRACE_INFO() << "tts.cancel: " << ret;
      return ret;
    }

    std::optional<bool> speak(const std::string& text) override {
      auto ret = tts_.speak(text);
      TRACE_INFO() << "tts.speak: text = '" << text << "', " << ret;
      return ret;
    }

    std::optional<bool> is_available() override {
      auto ret = tts_.is_available();
      TRACE_INFO() << "tts.is_available: " << ret;
      return ret;
    }

    std::optional<bool> is_enabled() override {
      auto ret = tts_.is_enabled();
      TRACE_INFO() << "tts.is_enabled: " << ret;
      return ret;
    }
  };

  struct TraceAccessibility final : public IAccessibility {
    IAccessibility& accessibility_;
    TraceAccessibility(IAccessibility& accessibility)
      : accessibility_(accessibility) {
    }
    std::optional<bool> display_settings(SbAccessibilityDisplaySettings& out) override {
      auto ret = accessibility_.display_settings(out);
      TRACE_INFO() << "accessibility.display_settings: " << ret << ", " << out;
      return ret;
    }
    std::optional<bool> caption_settings(SbAccessibilityCaptionSettings& out) override {
      auto ret = accessibility_.caption_settings(out);
      TRACE_INFO() << "accessibility.caption_settings: " << ret << ", " << out;
      return ret;
    }
  };

  struct TraceAdvertising final : public IAdvertising {
    IAdvertising& advertising_;
    TraceAdvertising(IAdvertising& advertising)
      : advertising_(advertising) {
    }
    std::optional<Ifa> advertising_id() override {
      auto ret = advertising_.advertising_id();
      TRACE_INFO() << "advertising.advertising_id: " << ret;
      return ret;
    }
  };

  std::unique_ptr<PlatformInterface> api_;
  TraceDevice device_{api_->device()};
  TraceTextToSpeech text_to_speech_{api_->text_to_speech()};
  TraceAccessibility accessibility_{api_->accessibility()};
  TraceAdvertising advertising_{api_->advertising()};

 public:
  TracePlatformInterface(std::unique_ptr<PlatformInterface>&& api) : api_(std::move(api)) {}

  void teardown() override { api_->teardown(); }
  void suspend() override { api_->suspend(); }
  void resume() override { api_->resume(); }

  IDevice& device() override { return device_; }
  ITextToSpeech& text_to_speech() override { return text_to_speech_; }
  IAccessibility& accessibility() override { return accessibility_; }
  IAdvertising& advertising() override { return advertising_; }
};
#undef TRACE_INFO

class CompositeInterface final : public PlatformInterface {
  struct CompositeDevice final : public IDevice {
    CompositeInterface& parent_;

    CompositeDevice(CompositeInterface &parent)
        : parent_(parent) {}

    std::optional<Resolution> video_resolution() override {
      return parent_.forAnyDevice<Resolution>([](IDevice& device) {
        return device.video_resolution();
      });
    }

    std::optional<float> diagonal_size_in_inches() override {
      return parent_.forAnyDevice<float>([](IDevice& device) {
        return device.diagonal_size_in_inches();
      });
    }

    std::optional<HDRFormat> hdr() override {
      return parent_.forAnyDevice<HDRFormat>([](IDevice& device) {
        return device.hdr();
      });
    }

    std::optional<bool> audio_configuration(
        int index,
        SbMediaAudioConfiguration* out_audio_configuration) override {
      return parent_.forAnyDevice<bool>([&](IDevice& device) {
        return device.audio_configuration(index, out_audio_configuration);
      });
    }

    std::optional<std::string> brand_name() override {
      return parent_.forAnyDevice<std::string>([](IDevice& device) {
        return device.brand_name();
      });
    }

    std::optional<std::string> chipset() override {
      return parent_.forAnyDevice<std::string>([](IDevice& device) {
        return device.chipset();
      });
    }

    std::optional<std::string> device_type() override {
      return parent_.forAnyDevice<std::string>([](IDevice& device) {
        return device.device_type();
      });
    }

    std::optional<std::string> firmware_version() override {
      return parent_.forAnyDevice<std::string>([](IDevice& device) {
        return device.firmware_version();
      });
    }

    std::optional<bool> is_connection_type_wireless() override {
      return parent_.forAnyDevice<bool>([](IDevice& device) {
        return device.is_connection_type_wireless();
      });
    }

    std::optional<bool> is_disconnected() override {
      return parent_.forAnyDevice<bool>([](IDevice& device) {
        return device.is_disconnected();
      });
    }
  };

  struct CompositeTextToSpeech final : public ITextToSpeech {
    CompositeInterface& parent_;

    CompositeTextToSpeech(CompositeInterface &parent)
        : parent_(parent) {}

    std::optional<bool> cancel() override {
      return parent_.forAnyTTS([](ITextToSpeech& tts) {
        return tts.cancel();
      });
    }

    std::optional<bool> speak(const std::string& text) override {
      return parent_.forAnyTTS([&text](ITextToSpeech& tts) {
        return tts.speak(text);
      });
    }

    std::optional<bool> is_available() override {
      return parent_.forAnyTTS([](ITextToSpeech& tts) {
        return tts.is_available();
      });
    }

    std::optional<bool> is_enabled() override {
      return parent_.forAnyTTS([](ITextToSpeech& tts) {
        return tts.is_enabled();
      });
    }
  };

  struct CompositeAccessibility final : public IAccessibility {
    CompositeInterface& parent_;

    CompositeAccessibility(CompositeInterface &parent)
        : parent_(parent) {}

    std::optional<bool> display_settings(SbAccessibilityDisplaySettings& out) override {
      return parent_.forAnyAccessibility([&out](IAccessibility& accessibility) {
        return accessibility.display_settings(out);
      });
    }

    std::optional<bool> caption_settings(SbAccessibilityCaptionSettings& out) override {
      return parent_.forAnyAccessibility([&out](IAccessibility& accessibility) {
        return accessibility.caption_settings(out);
      });
    }
  };

  struct CompositeAdvertising final : public IAdvertising {
    CompositeInterface& parent_;

    CompositeAdvertising(CompositeInterface &parent)
        : parent_(parent) {}

    std::optional<Ifa> advertising_id() override {
      return parent_.forAnyAdvertising<Ifa>([](IAdvertising& advertising) {
        return advertising.advertising_id();
      });
    }
  };

  template<typename T>
  std::optional<T> forAnyDevice(std::function<std::optional<T>(IDevice&)>&& fn) {
    for (auto &i : interfaces_) {
      if (auto ret = fn(i->device()); ret.has_value())
        return ret;
    }
    return { };
  }

  std::optional<bool> forAnyTTS(std::function<std::optional<bool>(ITextToSpeech&)>&& fn) {
    for (auto &i : interfaces_) {
      if (auto ret = fn(i->text_to_speech()); ret.has_value())
        return ret;
    }
    return { };
  }

  std::optional<bool> forAnyAccessibility(std::function<std::optional<bool>(IAccessibility&)>&& fn) {
    for (auto &i : interfaces_) {
      if (auto ret = fn(i->accessibility()); ret.has_value())
        return ret;
    }
    return { };
  }

  template<typename T>
  std::optional<T> forAnyAdvertising(std::function<std::optional<T>(IAdvertising&)>&& fn) {
    for (auto &i : interfaces_) {
      if (auto ret = fn(i->advertising()); ret.has_value())
        return ret;
    }
    return { };
  }

  void forEachInterface(std::function<void(PlatformInterface&)>&& fn) {
    for (auto &i : interfaces_) {
      fn(*i);
    }
  }

  const std::vector<std::unique_ptr<PlatformInterface>> interfaces_;
  CompositeDevice device_{ *this };
  CompositeTextToSpeech text_to_speech_{ *this };
  CompositeAccessibility accessibility_{ *this };
  CompositeAdvertising advertising_{ *this };

public:
  CompositeInterface(std::vector<std::unique_ptr<PlatformInterface>> &&interfaces)
    : interfaces_(std::move(interfaces)) {
  }

  void teardown() override {
    forEachInterface([](auto& iface) {
      iface.teardown();
    });
  }

  void suspend() override {
    forEachInterface([](auto& iface) {
      iface.suspend();
    });
  }

  void resume() override {
    forEachInterface([](auto& iface) {
      iface.resume();
    });
  }

  IDevice& device() override {
    return device_;
  }

  ITextToSpeech& text_to_speech() override {
    return text_to_speech_;
  }

  IAccessibility& accessibility() override {
    return accessibility_;
  }

  IAdvertising& advertising() override {
    return advertising_;
  }
};

PlatformInterface& PlatformInterface::get() {
  static std::unique_ptr<PlatformInterface> g_instance;
  static std::once_flag flag;
  std::call_once(flag, [] {
    std::vector<std::unique_ptr<PlatformInterface>> interfaces;

#if defined(ENABLE_FIREBOLT_API) && ENABLE_FIREBOLT_API
    if (FireboltInterface::is_available()) {
      interfaces.emplace_back(std::make_unique<FireboltInterface>());
    }
#endif

#if defined(ENABLE_RDKSERVICES_API) && ENABLE_RDKSERVICES_API
    if (RDKServicesInterface::is_available()) {
      interfaces.emplace_back(std::make_unique<RDKServicesInterface>());
    }
#endif

    if (interfaces.size() == 1) {
      g_instance = std::move(interfaces[0]);
    }
    else {
      g_instance = std::make_unique<CompositeInterface>(std::move(interfaces));
    }

#if !defined(COBALT_BUILD_TYPE_GOLD)
    g_instance = std::make_unique<TracePlatformInterface>(std::move(g_instance));
#endif
  });
  return *g_instance;
}

}  // namespace third_party::starboard::rdk::shared::platform
