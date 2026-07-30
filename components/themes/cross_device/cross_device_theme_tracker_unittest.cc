// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/cross_device/cross_device_theme_tracker.h"

#include "base/test/task_environment.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace themes {

struct TestTheme {
  std::string color;
};

template <>
struct ThemeComparer<TestTheme> {
  static bool Equals(const TestTheme& lhs, const TestTheme& rhs) {
    return lhs.color == rhs.color;
  }
};

namespace {

class MockObserver : public CrossDeviceThemeTracker<TestTheme>::Observer {
 public:
  MOCK_METHOD(void, OnCrossDeviceThemeChanged, (), (override));
  MOCK_METHOD(void, OnServiceStatusChanged, (ServiceStatus), (override));
};

// Testable subclass to expose protected methods.
class TestCrossDeviceThemeTracker : public CrossDeviceThemeTracker<TestTheme> {
 public:
  explicit TestCrossDeviceThemeTracker(
      syncer::DeviceInfoTracker* device_info_tracker)
      : CrossDeviceThemeTracker(device_info_tracker) {}

  using CrossDeviceThemeTracker::RemoveThemeInfo;
  using CrossDeviceThemeTracker::SetStatus;
  using CrossDeviceThemeTracker::UpdateThemeInfo;
};

class CrossDeviceThemeTrackerTest : public testing::Test {
 protected:
  std::string AddDevice(const std::string& cache_guid,
                        const std::string& client_name,
                        syncer::DeviceInfo::OsType os_type,
                        syncer::DeviceInfo::FormFactor form_factor) {
    auto device_info = syncer::TestDeviceInfoBuilder()
                           .WithGuid(cache_guid)
                           .WithClientName(client_name)
                           .WithOsType(os_type)
                           .WithFormFactor(form_factor)
                           .Build();
    fake_device_info_tracker_.Add(std::move(device_info));

    syncer::DataType type = OsTypeToDataType(os_type);
    return syncer::ClientTagHash::FromUnhashed(type, cache_guid).value();
  }

  base::test::TaskEnvironment task_environment_;
  syncer::FakeDeviceInfoTracker fake_device_info_tracker_;
  TestCrossDeviceThemeTracker tracker_{&fake_device_info_tracker_};
};

TEST_F(CrossDeviceThemeTrackerTest, InitialState) {
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kInitializing);
  EXPECT_TRUE(tracker_.GetOtherDevicesThemes().empty());
}

TEST_F(CrossDeviceThemeTrackerTest, UpdateAndRemoveTheme) {
  MockObserver observer;
  tracker_.AddObserver(&observer);

  DeviceThemeInfo<TestTheme> theme_info;
  theme_info.device_name = "Phone";
  theme_info.os_type = syncer::DeviceInfo::OsType::kAndroid;
  theme_info.form_factor = syncer::DeviceInfo::FormFactor::kPhone;
  theme_info.theme.color = "blue";

  // Expect observer notification on update.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("guid_1", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify theme is in the list.
  auto themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].device_name, "Phone");
  EXPECT_EQ(themes[0].os_type, syncer::DeviceInfo::OsType::kAndroid);
  EXPECT_EQ(themes[0].form_factor, syncer::DeviceInfo::FormFactor::kPhone);
  EXPECT_EQ(themes[0].theme.color, "blue");

  // Update same guid.
  theme_info.theme.color = "red";
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("guid_1", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].theme.color, "red");

  // Update with same info, expect NO notification.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(0);
  tracker_.UpdateThemeInfo("guid_1", theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Add another guid.
  DeviceThemeInfo<TestTheme> theme_info2;
  theme_info2.device_name = "Tablet";
  theme_info2.os_type = syncer::DeviceInfo::OsType::kAndroid;
  theme_info2.form_factor = syncer::DeviceInfo::FormFactor::kTablet;
  theme_info2.theme.color = "green";

  // Expect observer notification on update.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo("guid_2", theme_info2);
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  EXPECT_EQ(themes.size(), 2u);

  // Remove one.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.RemoveThemeInfo("guid_1");
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].device_name, "Tablet");

  // Remove non-existent.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(0);
  tracker_.RemoveThemeInfo("guid_non_existent");
  testing::Mock::VerifyAndClearExpectations(&observer);

  tracker_.RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerTest, StatusChanges) {
  MockObserver observer;
  tracker_.AddObserver(&observer);

  EXPECT_CALL(observer, OnServiceStatusChanged(ServiceStatus::kActive))
      .Times(1);
  tracker_.SetStatus(ServiceStatus::kActive);
  EXPECT_EQ(tracker_.GetServiceStatus(), ServiceStatus::kActive);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Set same status, no notification.
  EXPECT_CALL(observer, OnServiceStatusChanged(testing::_)).Times(0);
  tracker_.SetStatus(ServiceStatus::kActive);
  testing::Mock::VerifyAndClearExpectations(&observer);

  tracker_.RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerTest, DeviceInfoChange) {
  MockObserver observer;
  tracker_.AddObserver(&observer);

  std::string cache_guid = "device_guid_1";
  std::string hash =
      AddDevice(cache_guid, "Phone", syncer::DeviceInfo::OsType::kAndroid,
                syncer::DeviceInfo::FormFactor::kPhone);

  DeviceThemeInfo<TestTheme> theme_info;
  theme_info.os_type = syncer::DeviceInfo::OsType::kAndroid;
  theme_info.theme.color = "blue";

  // Update theme. Since device info is already added, it should resolve
  // immediately.
  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  tracker_.UpdateThemeInfo(hash, theme_info);
  testing::Mock::VerifyAndClearExpectations(&observer);

  auto themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].device_name, "Phone");
  EXPECT_EQ(themes[0].form_factor, syncer::DeviceInfo::FormFactor::kPhone);

  // Now simulate a change in device info (e.g. name change).
  const syncer::DeviceInfo* old_device =
      fake_device_info_tracker_.GetDeviceInfo(cache_guid);
  ASSERT_TRUE(old_device);
  fake_device_info_tracker_.Remove(old_device);

  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  auto updated_device_info =
      syncer::TestDeviceInfoBuilder()
          .WithGuid(cache_guid)
          .WithClientName("New Phone Name")
          .WithOsType(syncer::DeviceInfo::OsType::kAndroid)
          .WithFormFactor(syncer::DeviceInfo::FormFactor::kPhone)
          .Build();
  fake_device_info_tracker_.Add(std::move(updated_device_info));
  testing::Mock::VerifyAndClearExpectations(&observer);

  themes = tracker_.GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].device_name, "New Phone Name");

  tracker_.RemoveObserver(&observer);
}

}  // namespace

}  // namespace themes
