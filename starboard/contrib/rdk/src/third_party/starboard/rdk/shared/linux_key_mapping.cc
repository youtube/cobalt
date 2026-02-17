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

#include "third_party/starboard/rdk/shared/linux_key_mapping.h"

#include <core/JSON.h>
#include <core/Enumerate.h>

#include <linux/input.h>

#include "starboard/common/once.h"

#include "third_party/starboard/rdk/shared/log_override.h"

using namespace WPEFramework;

namespace WPEFramework {

ENUM_CONVERSION_HANDLER(SbKeyModifiers);
ENUM_CONVERSION_BEGIN(SbKeyModifiers)
  { kSbKeyModifiersAlt, _TXT("alt") },
  { kSbKeyModifiersCtrl, _TXT("ctrl") },
  { kSbKeyModifiersMeta, _TXT("meta") },
  { kSbKeyModifiersShift, _TXT("shift") },
ENUM_CONVERSION_END(SbKeyModifiers);

}

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

namespace {

struct KeymappingsData  : public Core::JSON::Container {
  struct KeymappingData : public Core::JSON::Container {
    struct MappedData   : public Core::JSON::Container {
      MappedData()
        : Core::JSON::Container() {
        Init();
      }

      MappedData(const MappedData& other)
        : Core::JSON::Container()
        , Keycode(other.Keycode)
        , Modifiers(other.Modifiers) {
        Init();
      }

      MappedData& operator=(const MappedData& rhs) {
        Keycode = rhs.Keycode;
        Modifiers = rhs.Modifiers;
        return (*this);
      }

    private:
      void Init() {
        Add(_T("keycode"), &Keycode);
        Add(_T("modifiers"), &Modifiers);
      }

    public:
      Core::JSON::DecUInt16 Keycode;
      Core::JSON::ArrayType<Core::JSON::EnumType<SbKeyModifiers>> Modifiers;
    }; // struct MappedData

    KeymappingData()
      : Core::JSON::Container() {
      Init();
    }

    KeymappingData(const KeymappingData& other)
      : Core::JSON::Container()
      , Keycode(other.Keycode)
      , Modifiers(other.Modifiers)
      , Mapped(other.Mapped) {
      Init();
    }

    KeymappingData& operator=(const KeymappingData& rhs) {
      Keycode = rhs.Keycode;
      Modifiers = rhs.Modifiers;
      Mapped = rhs.Mapped;
      return (*this);
    }

  private:
    void Init() {
      Add(_T("keycode"), &Keycode);
      Add(_T("modifiers"), &Modifiers);
      Add(_T("mapped"), &Mapped);
    }

  public:
    Core::JSON::DecUInt16 Keycode;
    Core::JSON::ArrayType<Core::JSON::EnumType<SbKeyModifiers>> Modifiers;
    MappedData Mapped;
  }; // struct KeymappingData

  KeymappingsData()
    : Core::JSON::Container() {
    Add(_T("keymappings"), &Keymappings);
  }
  KeymappingsData(const KeymappingsData&) = delete;
  KeymappingsData& operator=(const KeymappingsData&) = delete;

  Core::JSON::ArrayType<KeymappingData> Keymappings;
}; // struct KeymappingsData

struct LinuxKeyMappingImpl {

  struct KeyMapping {
    uint16_t key_code;
    SbKeyModifiers modifiers;
    uint16_t mapped_key_code;
    SbKeyModifiers mapped_modifiers;
  };

  // Default mapping for the Lima M1 RDK remote (t4h). It can be overridden at
  // runtime via keymapping.json ($COBALT_CONTENT_DIR/app/cobalt/content/etc/keymapping.json)
  // without rebuilding Cobalt—e.g. for testing or to use a different remote.
  std::vector<KeyMapping> key_mappings_ = {
    { KEY_M, kSbKeyModifiersCtrl, KEY_UNKNOWN,     kSbKeyModifiersNone },  // Menu
    { KEY_G, kSbKeyModifiersCtrl, KEY_UNKNOWN,     kSbKeyModifiersNone },  // Guide
    { KEY_R, kSbKeyModifiersCtrl, KEY_UNKNOWN,     kSbKeyModifiersNone },  // Record
    { KEY_U, kSbKeyModifiersCtrl, KEY_UNKNOWN,     kSbKeyModifiersNone },  // Volume Up
    { KEY_D, kSbKeyModifiersCtrl, KEY_UNKNOWN,     kSbKeyModifiersNone },  // Volume Down
    { KEY_S, kSbKeyModifiersCtrl, KEY_UNKNOWN,     kSbKeyModifiersNone },  // Stop
    { KEY_N, kSbKeyModifiersCtrl, KEY_UNKNOWN,     kSbKeyModifiersNone },  // Favorite
    { KEY_O, kSbKeyModifiersCtrl, KEY_UNKNOWN,     kSbKeyModifiersNone },  // OnDemand
    { KEY_B, kSbKeyModifiersCtrl, KEY_UNKNOWN,     kSbKeyModifiersNone },  // Replay
    { KEY_C, kSbKeyModifiersCtrl, KEY_UNKNOWN,     kSbKeyModifiersNone },  // Search
    { KEY_E, kSbKeyModifiersCtrl, KEY_UNKNOWN,     kSbKeyModifiersNone },  // Exit

    { KEY_I, kSbKeyModifiersCtrl, KEY_INFO,        kSbKeyModifiersNone },
    { KEY_Y, kSbKeyModifiersCtrl, KEY_MUTE,        kSbKeyModifiersNone },
    { KEY_L, kSbKeyModifiersCtrl, KEY_ESC,         kSbKeyModifiersNone },
    { KEY_F, kSbKeyModifiersCtrl, KEY_FASTFORWARD, kSbKeyModifiersNone },
    { KEY_W, kSbKeyModifiersCtrl, KEY_REWIND,      kSbKeyModifiersNone },
    { KEY_P, kSbKeyModifiersCtrl, KEY_PLAYPAUSE,   kSbKeyModifiersNone },
    { KEY_0, kSbKeyModifiersCtrl, KEY_RED,         kSbKeyModifiersNone },
    { KEY_1, kSbKeyModifiersCtrl, KEY_GREEN,       kSbKeyModifiersNone },
    { KEY_2, kSbKeyModifiersCtrl, KEY_YELLOW,      kSbKeyModifiersNone },
    { KEY_3, kSbKeyModifiersCtrl, KEY_BLUE,        kSbKeyModifiersNone },
    { KEY_UP, kSbKeyModifiersCtrl, KEY_CHANNELUP,   kSbKeyModifiersNone },
    { KEY_DOWN, kSbKeyModifiersCtrl, KEY_CHANNELDOWN, kSbKeyModifiersNone },

    { KEY_PAGEDOWN, kSbKeyModifiersNone, KEY_NEXTSONG,   kSbKeyModifiersNone },
    { KEY_PAGEUP, kSbKeyModifiersNone, KEY_PREVIOUSSONG, kSbKeyModifiersNone },
    { KEY_BACKSPACE, kSbKeyModifiersNone, KEY_ESC, kSbKeyModifiersNone },
    { KEY_SELECT, kSbKeyModifiersNone, KEY_ENTER, kSbKeyModifiersNone },
  };

  void UpdateKeyMapping(
    uint16_t key, SbKeyModifiers modifiers,
    uint16_t mapped_key, SbKeyModifiers mapped_modifiers) {

    SB_LOG(INFO)   << "UpdateKeyMapping: "
      "key code: " << key << " (" << modifiers << ") "
      "mapped to: "   << mapped_key << " (" << mapped_modifiers << ")";

    auto it = std::find_if(key_mappings_.begin(), key_mappings_.end(), [&](const KeyMapping & mapping) {
      return mapping.key_code == key && mapping.modifiers == modifiers;
    });

    if (it != key_mappings_.end()) {
      it->mapped_key_code = mapped_key;
      it->mapped_modifiers = mapped_modifiers;
    }
    else {
      key_mappings_.push_back({key, modifiers, mapped_key, mapped_modifiers});
    }
  }

  void UpdateKeyMapping(const KeymappingsData &data) {
    if (!data.Keymappings.IsSet()) {
      SB_LOG(WARNING) << "No key mappings in config";
      return ;
    }

    auto convert_modifiers = [](const Core::JSON::ArrayType<Core::JSON::EnumType<SbKeyModifiers>>& modifiers) {
      uint32_t result = kSbKeyModifiersNone;
      if (modifiers.IsSet() && !modifiers.IsNull()) {
        auto it = modifiers.Elements();
        while(it.Next())
          result |= it.Current().Value();
      }
      return static_cast<SbKeyModifiers>(result);
    };

    auto elms = data.Keymappings.Elements();
    while(elms.Next()) {
      const auto& key_mapping = elms.Current();
      if (!key_mapping.Keycode.IsSet() ||
          !key_mapping.Mapped.IsSet() ||
          !key_mapping.Mapped.Keycode.IsSet()) {
        continue;
      }
      uint16_t key_code = key_mapping.Keycode.Value();
      SbKeyModifiers key_modifiers = convert_modifiers(key_mapping.Modifiers);
      uint16_t mapped_key_code = key_mapping.Mapped.Keycode.Value();
      SbKeyModifiers mapped_key_modifiers = convert_modifiers(key_mapping.Mapped.Modifiers);
      UpdateKeyMapping(key_code, key_modifiers, mapped_key_code, mapped_key_modifiers);
    }
  }

  void ReadFromFile(const std::string &filename) {
    Core::File file{filename};
    if (file.Exists() == false)
      return;
    SB_LOG(INFO) << "Reading keymapping from file: " << filename;
    if (file.Open(true) == false) {
      SB_LOG(WARNING) << "Cannot open key mapping file: " << filename;
      return;
    }
    Core::OptionalType<Core::JSON::Error> error;
    KeymappingsData data;
    if (!Core::JSON::IElement::FromFile(file, data, error)) {
      SB_LOG(ERROR) << "Failed to parse key mapping file(" << filename << "), error: "
                    << (error.IsSet() ? Core::JSON::ErrorDisplayMessage(error.Value()): "Unknown");
      return;
    }
    UpdateKeyMapping(data);
  }

  void MapKeyCodeAndModifiers(uint32_t& key_code, uint32_t& modifiers) {
    auto it = std::find_if(key_mappings_.begin(), key_mappings_.end(), [&](const KeyMapping & mapping) {
      return mapping.key_code == key_code && mapping.modifiers == modifiers;
    });
    if (it != key_mappings_.end()) {
      key_code = it->mapped_key_code;
      modifiers = it->mapped_modifiers;
    }
  }

  LinuxKeyMappingImpl() {
    const int kBufferSize = 256;
    char buffer[kBufferSize];
    if (SbSystemGetPath(kSbSystemPathContentDirectory, buffer, kBufferSize)) {
      ReadFromFile(std::string(buffer).append("/etc/keymapping.json"));
    }
  }
};

SB_ONCE_INITIALIZE_FUNCTION(LinuxKeyMappingImpl, GetLinuxKeyMapping);

}  // namespace

void LinuxKeyMapping::MapKeyCodeAndModifiers(uint32_t& key_code, uint32_t& modifiers) {
  GetLinuxKeyMapping()->MapKeyCodeAndModifiers(key_code, modifiers);
}

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
