// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_name_util.h"

#include <memory>
#include <set>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class DeviceNameUtilTest : public testing::Test {
 public:
  DeviceNameUtilTest() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  syncer::TestSyncService test_sync_service_;
};

static std::unique_ptr<DeviceInfo> CreateFakeDeviceInfo(
    const std::string& id,
    const std::string& name,
    DeviceInfo::OsType os_type,
    const std::string& manufacturer_name,
    const std::string& model_name,
    std::optional<DeviceInfo::DeviceType> device_type = std::nullopt,
    std::optional<DeviceInfo::FormFactor> form_factor = std::nullopt) {
  TestDeviceInfoBuilder builder(os_type);
  builder.WithGuid(id)
      .WithClientName(name)
      .WithManufacturerName(manufacturer_name)
      .WithModelName(model_name);
  if (device_type) {
    builder.WithDeviceType(*device_type);
  }
  if (form_factor) {
    builder.WithFormFactor(*form_factor);
  }
  return builder.Build();
}

}  // namespace

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_AppleDevices_SigninOnly) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "MacbookPro1,1", DeviceInfo::OsType::kMac,
                           "Apple Inc.", "MacbookPro1,1");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("MacbookPro1,1", candidates.fallback_full_name);
  EXPECT_EQ("MacbookPro", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_AppleDevices_FullySynced) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "Bobs-iMac", DeviceInfo::OsType::kMac,
                           "Apple Inc.", "MacbookPro1,1");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("Bobs-iMac", candidates.fallback_full_name);
  EXPECT_EQ("Bobs-iMac", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_IOS_GenericName) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "iPhone", DeviceInfo::OsType::kIOS, "Apple Inc.", "iPhone14,5");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("iPhone14,5", candidates.fallback_full_name);
  EXPECT_EQ("iPhone", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_IOS_CustomName) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "John's iPhone", DeviceInfo::OsType::kIOS,
                           "Apple Inc.", "iPhone14,5");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("John's iPhone", candidates.fallback_full_name);
  EXPECT_EQ("John's iPhone", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_EmptyClientName) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "", DeviceInfo::OsType::kWindows, "Dell", "XPS 13");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("Dell Computer XPS 13", candidates.fallback_full_name);
  EXPECT_EQ("Dell Computer", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_ChromeOSDevices) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Chromebook", DeviceInfo::OsType::kChromeOsAsh, "Google",
      "Chromebook");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("Google Chromebook", candidates.fallback_full_name);
  EXPECT_EQ("Google Chromebook", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_AndroidPhones) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Pixel 2", DeviceInfo::OsType::kAndroid, "Google", "Pixel 2");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("Google Phone Pixel 2", candidates.fallback_full_name);
  EXPECT_EQ("Google Phone", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_AndroidTablets) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "Pixel C", DeviceInfo::OsType::kAndroid, "Google", "Pixel C",
      DeviceInfo::DeviceType::kTablet, DeviceInfo::FormFactor::kTablet);
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("Google Tablet Pixel C", candidates.fallback_full_name);
  EXPECT_EQ("Google Tablet", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_Windows_SigninOnly) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "BX123", DeviceInfo::OsType::kWindows, "Dell", "BX123");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("Dell Computer BX123", candidates.fallback_full_name);
  EXPECT_EQ("Dell Computer", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_Windows_FullySynced) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "BOBS-WINDOWS-1", DeviceInfo::OsType::kWindows, "Dell", "BX123");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("BOBS-WINDOWS-1", candidates.fallback_full_name);
  EXPECT_EQ("BOBS-WINDOWS-1", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_Linux_SigninOnly) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "30BDS0RA0G", DeviceInfo::OsType::kLinux, "LENOVO", "30BDS0RA0G");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("LENOVO Computer 30BDS0RA0G", candidates.fallback_full_name);
  EXPECT_EQ("LENOVO Computer", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, GetDisplayNameCandidates_Linux_FullySynced) {
  std::unique_ptr<DeviceInfo> device =
      CreateFakeDeviceInfo("guid", "bob.chromium.org",
                           DeviceInfo::OsType::kLinux, "LENOVO", "30BDS0RA0G");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("bob.chromium.org", candidates.fallback_full_name);
  EXPECT_EQ("bob.chromium.org", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, CheckManufacturerNameCapitalization) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "model", DeviceInfo::OsType::kWindows, "foo bar", "model");
  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("Foo Bar Computer model", candidates.fallback_full_name);
  EXPECT_EQ("Foo Bar Computer", candidates.preferred_name_if_unique);

  device = CreateFakeDeviceInfo("guid", "model", DeviceInfo::OsType::kWindows,
                                "foo1bar", "model");
  candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("Foo1Bar Computer model", candidates.fallback_full_name);
  EXPECT_EQ("Foo1Bar Computer", candidates.preferred_name_if_unique);

  device = CreateFakeDeviceInfo("guid", "model", DeviceInfo::OsType::kWindows,
                                "foo_bar-FOO", "model");
  candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("Foo_Bar-FOO Computer model", candidates.fallback_full_name);
  EXPECT_EQ("Foo_Bar-FOO Computer", candidates.preferred_name_if_unique);

  device = CreateFakeDeviceInfo("guid", "model", DeviceInfo::OsType::kWindows,
                                "foo&bar foo", "model");
  candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("Foo&Bar Foo Computer model", candidates.fallback_full_name);
  EXPECT_EQ("Foo&Bar Foo Computer", candidates.preferred_name_if_unique);

  // Non-ASCII manufacturer names should be returned as-is.
  device = CreateFakeDeviceInfo("guid", "model", DeviceInfo::OsType::kWindows,
                                "电子产品", "model");
  candidates = GetDisplayNameCandidates(device.get());

  EXPECT_EQ("电子产品 Computer model", candidates.fallback_full_name);
  EXPECT_EQ("电子产品 Computer", candidates.preferred_name_if_unique);
}

TEST_F(DeviceNameUtilTest, DetermineDisplayNamesAndDeduplicate) {
  std::unique_ptr<DeviceInfo> local_device = CreateFakeDeviceInfo(
      "local_guid", "XPS 13", DeviceInfo::OsType::kWindows, "Dell", "XPS 13");
  ASSERT_EQ("Dell Computer XPS 13",
            GetDisplayNameCandidates(local_device.get()).fallback_full_name);

  std::unique_ptr<DeviceInfo> device1 = CreateFakeDeviceInfo(
      "guid1", "Pixel 6", DeviceInfo::OsType::kAndroid, "Google", "Pixel 6");
  DisplayNameCandidates candidates1 = GetDisplayNameCandidates(device1.get());
  ASSERT_EQ("Google Phone Pixel 6", candidates1.fallback_full_name);
  ASSERT_EQ("Google Phone", candidates1.preferred_name_if_unique);

  std::unique_ptr<DeviceInfo> device2 = CreateFakeDeviceInfo(
      "guid2", "Pixel 7", DeviceInfo::OsType::kAndroid, "Google", "Pixel 7");
  DisplayNameCandidates candidates2 = GetDisplayNameCandidates(device2.get());
  ASSERT_EQ("Google Phone Pixel 7", candidates2.fallback_full_name);
  ASSERT_EQ("Google Phone", candidates2.preferred_name_if_unique);

  std::unique_ptr<DeviceInfo> device3 = CreateFakeDeviceInfo(
      "guid3", "XPS 13", DeviceInfo::OsType::kWindows, "Dell", "XPS 13");
  ASSERT_EQ("Dell Computer XPS 13",
            GetDisplayNameCandidates(device3.get()).fallback_full_name);

  std::vector<const DeviceInfo*> devices = {device1.get(), device2.get(),
                                            device3.get()};

  auto results = DetermineDisplayNamesAndDeduplicate(
      devices, GetDisplayNameCandidates(local_device.get()).fallback_full_name);

  // device1 and device2 have the same primary name "Google Phone", so they
  // should use their fallback names. device3 has the same fallback name as the
  // local device, so it should be filtered out.
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ(device1.get(), results[0].device);
  EXPECT_EQ("Google Phone Pixel 6", results[0].display_name);
  EXPECT_EQ(device2.get(), results[1].device);
  EXPECT_EQ("Google Phone Pixel 7", results[1].display_name);
}

TEST_F(DeviceNameUtilTest,
       DetermineDisplayNamesAndDeduplicate_UniquePreferredNames) {
  std::unique_ptr<DeviceInfo> device1 = CreateFakeDeviceInfo(
      "guid1", "Pixel 6", DeviceInfo::OsType::kAndroid, "Google", "Pixel 6");
  ASSERT_EQ("Google Phone",
            GetDisplayNameCandidates(device1.get()).preferred_name_if_unique);

  std::unique_ptr<DeviceInfo> device2 = CreateFakeDeviceInfo(
      "guid2", "XPS 13", DeviceInfo::OsType::kWindows, "Dell", "XPS 13");
  ASSERT_EQ("Dell Computer",
            GetDisplayNameCandidates(device2.get()).preferred_name_if_unique);

  std::vector<const DeviceInfo*> devices = {device1.get(), device2.get()};

  auto results = DetermineDisplayNamesAndDeduplicate(devices, std::nullopt);

  // Both have unique preferred names.
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ(device1.get(), results[0].device);
  EXPECT_EQ("Google Phone", results[0].display_name);
  EXPECT_EQ(device2.get(), results[1].device);
  EXPECT_EQ("Dell Computer", results[1].display_name);
}

class DeviceNameUtilSimplifyNamingTest : public testing::Test {
 public:
  DeviceNameUtilSimplifyNamingTest() {
    scoped_feature_list_.InitAndEnableFeature(kSyncSimplifyDeviceNaming);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that GetDeviceDisplayName returns the most user friendly name
// (preferred name).
TEST_F(DeviceNameUtilSimplifyNamingTest, ShouldUseMostUserFriendlyName) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid1", "Pixel 6", DeviceInfo::OsType::kAndroid, "Google", "Pixel 6");

  EXPECT_EQ("Google Phone", GetDeviceDisplayName(device.get()));
}

// Tests that when client name is empty, GetDeviceDisplayName still uses the
// most user friendly name (preferred name, which falls back to manufacturer +
// device type).
TEST_F(DeviceNameUtilSimplifyNamingTest,
       ShouldFallbackToModelWhenClientNameEmpty) {
  std::unique_ptr<DeviceInfo> device = CreateFakeDeviceInfo(
      "guid", "", DeviceInfo::OsType::kWindows, "Dell", "XPS 13");

  EXPECT_EQ("Dell Computer", GetDeviceDisplayName(device.get()));
}

namespace {

constexpr char kSamsungManufacturer[] = "Samsung";
constexpr char kGalaxyS22UltraMarketingName[] = "Galaxy S22 Ultra";
constexpr char kGalaxyS22UltraModel[] = "SM-S908U";

constexpr char kGoogleManufacturer[] = "Google";
constexpr char kPixel9Name[] = "Pixel 9";

constexpr char kAppleManufacturer[] = "Apple Inc.";
constexpr char kIPhone13MarketingName[] = "iPhone 13";
constexpr char kIPhone13Model[] = "iPhone14,5";

}  // namespace

class DeviceNameUtilUseServerDeterminedDeviceNameTest : public testing::Test {
 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      kSyncUseServerDeterminedDeviceName};
};

// Tests that if a server-determined marketing name is available, it is used as
// the preferred name. It also verifies that we bypass different legacy
// platform-specific rules (such as Android's generic "Manufacturer Phone"
// fallback and iOS's model-prefix parsing).
TEST_F(DeviceNameUtilUseServerDeterminedDeviceNameTest,
       GetDisplayNameCandidates_WithServerDeterminedName) {
  // Case 1: Android Phone where model differs from marketing name.
  // Bypasses legacy "Samsung Phone SM-S908U" fallback.
  {
    TestDeviceInfoBuilder builder(DeviceInfo::OsType::kAndroid);
    builder.WithGuid("guid1")
        .WithClientName(kGalaxyS22UltraModel)
        .WithManufacturerName(kSamsungManufacturer)
        .WithModelName(kGalaxyS22UltraModel)
        .WithServerDeterminedModelName(kGalaxyS22UltraMarketingName);
    std::unique_ptr<DeviceInfo> device = builder.Build();

    DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());
    EXPECT_EQ(kGalaxyS22UltraMarketingName,
              candidates.preferred_name_if_unique);
    // Fallback is also the marketing name in the original design.
    EXPECT_EQ(kGalaxyS22UltraMarketingName, candidates.fallback_full_name);
  }

  // Case 2: Android Phone where model is the same as marketing name.
  {
    TestDeviceInfoBuilder builder(DeviceInfo::OsType::kAndroid);
    builder.WithGuid("guid2")
        .WithClientName(kPixel9Name)
        .WithManufacturerName(kGoogleManufacturer)
        .WithModelName(kPixel9Name)
        .WithServerDeterminedModelName(kPixel9Name);
    std::unique_ptr<DeviceInfo> device = builder.Build();

    DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());
    EXPECT_EQ(kPixel9Name, candidates.preferred_name_if_unique);
    EXPECT_EQ(kPixel9Name, candidates.fallback_full_name);
  }

  // Case 3: iOS Phone. Bypasses legacy Apple prefix parsing.
  {
    TestDeviceInfoBuilder builder(DeviceInfo::OsType::kIOS);
    builder.WithGuid("guid3")
        .WithClientName("iPhone")
        .WithManufacturerName(kAppleManufacturer)
        .WithModelName(kIPhone13Model)
        .WithServerDeterminedModelName(kIPhone13MarketingName);
    std::unique_ptr<DeviceInfo> device = builder.Build();

    DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());
    EXPECT_EQ(kIPhone13MarketingName, candidates.preferred_name_if_unique);
    EXPECT_EQ(kIPhone13MarketingName, candidates.fallback_full_name);
  }
}

// Tests that server-determined marketing names always take precedence over
// user-defined custom names (high quality client names).
TEST_F(DeviceNameUtilUseServerDeterminedDeviceNameTest,
       GetDisplayNameCandidates_ServerDeterminedNameOverridesCustomName) {
  TestDeviceInfoBuilder builder(DeviceInfo::OsType::kAndroid);
  builder.WithGuid("guid1")
      .WithClientName("My Work Phone")  // Custom high-quality name
      .WithManufacturerName(kSamsungManufacturer)
      .WithModelName(kGalaxyS22UltraModel)
      .WithServerDeterminedModelName(kGalaxyS22UltraMarketingName);
  std::unique_ptr<DeviceInfo> device = builder.Build();

  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());
  EXPECT_EQ(kGalaxyS22UltraMarketingName, candidates.preferred_name_if_unique);
  EXPECT_EQ(kGalaxyS22UltraMarketingName, candidates.fallback_full_name);
}

// Tests that if the feature is enabled but the server-determined name is
// missing (nullopt), the utility falls back to the legacy naming logic.
TEST_F(DeviceNameUtilUseServerDeterminedDeviceNameTest,
       GetDisplayNameCandidates_FeatureEnabled_NoServerDeterminedName) {
  TestDeviceInfoBuilder builder(DeviceInfo::OsType::kAndroid);
  builder.WithGuid("guid1")
      .WithClientName(kGalaxyS22UltraModel)
      .WithManufacturerName(kSamsungManufacturer)
      .WithModelName(kGalaxyS22UltraModel);
  std::unique_ptr<DeviceInfo> device = builder.Build();

  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());
  EXPECT_EQ("Samsung Phone", candidates.preferred_name_if_unique);
  EXPECT_EQ("Samsung Phone SM-S908U", candidates.fallback_full_name);
}

// Tests that if the feature is enabled but the server-determined name is
// empty, the utility safely falls back to the legacy naming logic.
TEST_F(DeviceNameUtilUseServerDeterminedDeviceNameTest,
       GetDisplayNameCandidates_FeatureEnabled_EmptyServerDeterminedName) {
  TestDeviceInfoBuilder builder(DeviceInfo::OsType::kAndroid);
  builder.WithGuid("guid1")
      .WithClientName(kGalaxyS22UltraModel)
      .WithManufacturerName(kSamsungManufacturer)
      .WithModelName(kGalaxyS22UltraModel)
      .WithServerDeterminedModelName("");
  std::unique_ptr<DeviceInfo> device = builder.Build();

  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());
  EXPECT_EQ("Samsung Phone", candidates.preferred_name_if_unique);
  EXPECT_EQ("Samsung Phone SM-S908U", candidates.fallback_full_name);
}

// Tests that if the feature is disabled, the utility falls back to the
// legacy naming logic even if a server-determined name is available.
TEST_F(DeviceNameUtilTest,
       GetDisplayNameCandidates_ServerDeterminedName_FeatureDisabled) {
  TestDeviceInfoBuilder builder(DeviceInfo::OsType::kAndroid);
  builder.WithGuid("guid1")
      .WithClientName(kGalaxyS22UltraModel)
      .WithManufacturerName(kSamsungManufacturer)
      .WithModelName(kGalaxyS22UltraModel)
      .WithServerDeterminedModelName(kGalaxyS22UltraMarketingName);
  std::unique_ptr<DeviceInfo> device = builder.Build();

  DisplayNameCandidates candidates = GetDisplayNameCandidates(device.get());
  EXPECT_EQ("Samsung Phone", candidates.preferred_name_if_unique);
  EXPECT_EQ("Samsung Phone SM-S908U", candidates.fallback_full_name);
}

}  // namespace syncer
