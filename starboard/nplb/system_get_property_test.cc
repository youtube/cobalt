// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <string.h>

#include "starboard/common/device_type.h"
#include "starboard/common/string.h"
#include "starboard/common/system_property.h"
#include "starboard/memory.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Size of appropriate value buffer.
const size_t kValueSize = 1024;

#if SB_API_VERSION < 15
bool IsCEDevice(SbSystemDeviceType device_type) {
  switch (device_type) {
    case kSbSystemDeviceTypeBlueRayDiskPlayer:
    case kSbSystemDeviceTypeGameConsole:
    case kSbSystemDeviceTypeOverTheTopBox:
    case kSbSystemDeviceTypeSetTopBox:
    case kSbSystemDeviceTypeTV:
      return true;
    case kSbSystemDeviceTypeDesktopPC:
    case kSbSystemDeviceTypeUnknown:
    default:
      return false;
  }
}
#endif
bool IsCEDevice(std::string device_type) {
  if (device_type == kSystemDeviceTypeBlueRayDiskPlayer ||
      device_type == kSystemDeviceTypeGameConsole ||
      device_type == kSystemDeviceTypeOverTheTopBox ||
      device_type == kSystemDeviceTypeSetTopBox ||
      device_type == kSystemDeviceTypeTV) {
    return true;
  }
  return false;
}

void BasicTest(SbSystemPropertyId id,
               bool expect_result,
               bool expected_result,
               int line) {
#define LOCAL_CONTEXT "Context : id=" << id << ", line=" << line;
  char value[kValueSize] = {0};
  memset(value, 0xCD, kValueSize);
  bool result = SbSystemGetProperty(id, value, kValueSize);
  if (expect_result) {
    EXPECT_EQ(expected_result, result) << LOCAL_CONTEXT;
  }
  if (result) {
    EXPECT_NE('\xCD', value[0]) << LOCAL_CONTEXT;
    int len = static_cast<int>(strlen(value));
    EXPECT_GT(len, 0) << LOCAL_CONTEXT;
  } else {
    EXPECT_EQ('\xCD', value[0]) << LOCAL_CONTEXT;
  }
#undef LOCAL_CONTEXT
}

void UnmodifiedOnFailureTest(SbSystemPropertyId id, int line) {
  char value[kValueSize] = {0};
  memset(value, 0xCD, kValueSize);
  for (size_t i = 0; i <= kValueSize; ++i) {
    if (SbSystemGetProperty(id, value, i)) {
      return;
    }
    for (auto ch : value) {
      ASSERT_EQ('\xCD', ch) << "Context : id=" << id << ", line=" << line;
    }
  }
}

TEST(SbSystemGetPropertyTest, ReturnsRequired) {
  BasicTest(kSbSystemPropertyFriendlyName, true, true, __LINE__);
  BasicTest(kSbSystemPropertyPlatformName, true, true, __LINE__);

  BasicTest(kSbSystemPropertyChipsetModelNumber, false, true, __LINE__);
  BasicTest(kSbSystemPropertyFirmwareVersion, false, true, __LINE__);
  BasicTest(kSbSystemPropertySystemIntegratorName, false, true, __LINE__);
  BasicTest(kSbSystemPropertySpeechApiKey, false, true, __LINE__);
#if SB_API_VERSION < 15
  if (IsCEDevice(SbSystemGetDeviceType())) {
    BasicTest(kSbSystemPropertyBrandName, true, true, __LINE__);
    BasicTest(kSbSystemPropertyModelName, true, true, __LINE__);
    BasicTest(kSbSystemPropertyModelYear, false, true, __LINE__);
  } else {
    BasicTest(kSbSystemPropertyBrandName, false, true, __LINE__);
    BasicTest(kSbSystemPropertyModelName, false, true, __LINE__);
    BasicTest(kSbSystemPropertyModelYear, false, true, __LINE__);
  }
#else
  const size_t kSystemPropertyMaxLength = 1024;
  char value[kSystemPropertyMaxLength];
  bool result;
  result = SbSystemGetProperty(kSbSystemPropertyDeviceType, value,
                               kSystemPropertyMaxLength);
  if (result && IsCEDevice(std::string(value))) {
    BasicTest(kSbSystemPropertyBrandName, true, true, __LINE__);
    BasicTest(kSbSystemPropertyModelName, true, true, __LINE__);
    BasicTest(kSbSystemPropertyModelYear, false, true, __LINE__);
  } else {
    BasicTest(kSbSystemPropertyBrandName, false, true, __LINE__);
    BasicTest(kSbSystemPropertyModelName, false, true, __LINE__);
    BasicTest(kSbSystemPropertyModelYear, false, true, __LINE__);
  }
#endif
}

TEST(SbSystemGetPropertyTest, FailsGracefullyZeroBufferLength) {
  char value[kValueSize] = {0};
  EXPECT_FALSE(SbSystemGetProperty(kSbSystemPropertyFriendlyName, value, 0));
}

TEST(SbSystemGetPropertyTest, FailsGracefullyNullBuffer) {
  EXPECT_FALSE(
      SbSystemGetProperty(kSbSystemPropertyFriendlyName, NULL, kValueSize));
}

TEST(SbSystemGetPropertyTest, FailsGracefullyNullBufferAndZeroLength) {
  EXPECT_FALSE(SbSystemGetProperty(kSbSystemPropertyFriendlyName, NULL, 0));
}

TEST(SbSystemGetPropertyTest, FailsGracefullyBogusId) {
  BasicTest(static_cast<SbSystemPropertyId>(99999), true, false, __LINE__);
}

TEST(SbSystemGetPropertyTest, DoesNotTouchOutputBufferOnFailureForDefinedIds) {
  UnmodifiedOnFailureTest(kSbSystemPropertyChipsetModelNumber, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPropertyFirmwareVersion, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPropertyFriendlyName, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPropertyManufacturerName, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPropertyBrandName, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPropertyModelName, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPropertyModelYear, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPropertySystemIntegratorName, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPropertyPlatformName, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPropertySpeechApiKey, __LINE__);
  UnmodifiedOnFailureTest(kSbSystemPropertyUserAgentAuxField, __LINE__);
}

TEST(SbSystemGetPropertyTest, SpeechApiKeyNotLeaked) {
  static const size_t kSize = 512;
  char speech_api_key[kSize] = {0};
  bool has_speech_key =
      SbSystemGetProperty(kSbSystemPropertySpeechApiKey, speech_api_key, kSize);

  if (!has_speech_key) {
    EXPECT_EQ(0, strlen(speech_api_key));
    return;
  }

  SbSystemPropertyId enum_values[] = {
      kSbSystemPropertyChipsetModelNumber,
      kSbSystemPropertyFirmwareVersion,
      kSbSystemPropertyFriendlyName,
      kSbSystemPropertyManufacturerName,
      kSbSystemPropertyModelName,
      kSbSystemPropertyModelYear,
      kSbSystemPropertySystemIntegratorName,
      kSbSystemPropertyPlatformName,
  };

  for (SbSystemPropertyId val : enum_values) {
    char value[kSize] = {0};

    if (SbSystemGetProperty(val, value, kSize)) {
      ASSERT_FALSE(strstr(value, speech_api_key));
    }
  }
}

#if SB_API_VERSION >= 15
TEST(SbSystemGetPropertyTest, DeviceTypeAllowed) {
  std::string device_type =
      GetSystemPropertyString(kSbSystemPropertyDeviceType);

  std::string device_type_values[] = {
      kSystemDeviceTypeBlueRayDiskPlayer,
      kSystemDeviceTypeGameConsole,
      kSystemDeviceTypeOverTheTopBox,
      kSystemDeviceTypeSetTopBox,
      kSystemDeviceTypeTV,
      kSystemDeviceTypeAndroidTV,
      kSystemDeviceTypeDesktopPC,
      kSystemDeviceTypeVideoProjector,
      kSystemDeviceTypeMultimediaDevices,
      kSystemDeviceTypeMonitor,
      kSystemDeviceTypeAuto,
      kSystemDeviceTypeSoundBar,
      kSystemDeviceTypeHospitality,
      kSystemDeviceTypeUnknown,
  };
  bool result = false;

  for (std::string val : device_type_values) {
    if (val == device_type) {
      result = true;
      break;
    }
  }
  ASSERT_TRUE(result);
}
#endif

}  // namespace
}  // namespace nplb
}  // namespace starboard
