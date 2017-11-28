// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor/system_monitor.h"

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/test/mock_devices_changed_observer.h"
#include "base/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class PowerTest : public SystemMonitor::PowerObserver {
 public:
  PowerTest()
      : power_state_changes_(0),
        suspends_(0),
        resumes_(0) {
  }

  // PowerObserver callbacks.
  virtual void OnPowerStateChange(bool on_battery_power) override {
    power_state_changes_++;
  }

  virtual void OnSuspend() override {
    suspends_++;
  }

  virtual void OnResume() override {
    resumes_++;
  }

  // Test status counts.
  int power_state_changes() { return power_state_changes_; }
  int suspends() { return suspends_; }
  int resumes() { return resumes_; }

 private:
  int power_state_changes_;  // Count of OnPowerStateChange notifications.
  int suspends_;  // Count of OnSuspend notifications.
  int resumes_;  // Count of OnResume notifications.
};

class SystemMonitorTest : public testing::Test {
 protected:
  SystemMonitorTest() {
#if defined(OS_MACOSX)
    // This needs to happen before SystemMonitor's ctor.
    SystemMonitor::AllocateSystemIOPorts();
#endif
    system_monitor_.reset(new SystemMonitor);
  }
  virtual ~SystemMonitorTest() {}

  MessageLoop message_loop_;
  scoped_ptr<SystemMonitor> system_monitor_;

  DISALLOW_COPY_AND_ASSIGN(SystemMonitorTest);
};

TEST_F(SystemMonitorTest, PowerNotifications) {
  const int kObservers = 5;

  PowerTest test[kObservers];
  for (int index = 0; index < kObservers; ++index)
    system_monitor_->AddPowerObserver(&test[index]);

  // Send a bunch of power changes.  Since the battery power hasn't
  // actually changed, we shouldn't get notifications.
  for (int index = 0; index < 5; index++) {
    system_monitor_->ProcessPowerMessage(SystemMonitor::POWER_STATE_EVENT);
    EXPECT_EQ(test[0].power_state_changes(), 0);
  }

  // Sending resume when not suspended should have no effect.
  system_monitor_->ProcessPowerMessage(SystemMonitor::RESUME_EVENT);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(test[0].resumes(), 0);

  // Pretend we suspended.
  system_monitor_->ProcessPowerMessage(SystemMonitor::SUSPEND_EVENT);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(test[0].suspends(), 1);

  // Send a second suspend notification.  This should be suppressed.
  system_monitor_->ProcessPowerMessage(SystemMonitor::SUSPEND_EVENT);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(test[0].suspends(), 1);

  // Pretend we were awakened.
  system_monitor_->ProcessPowerMessage(SystemMonitor::RESUME_EVENT);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(test[0].resumes(), 1);

  // Send a duplicate resume notification.  This should be suppressed.
  system_monitor_->ProcessPowerMessage(SystemMonitor::RESUME_EVENT);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(test[0].resumes(), 1);
}

TEST_F(SystemMonitorTest, DeviceChangeNotifications) {
  const int kObservers = 5;
  const string16 kDeviceName = ASCIIToUTF16("media device");
  const std::string kDeviceId1 = "1";
  const std::string kDeviceId2 = "2";

  testing::Sequence mock_sequencer[kObservers];
  MockDevicesChangedObserver observers[kObservers];
  for (int index = 0; index < kObservers; ++index) {
    system_monitor_->AddDevicesChangedObserver(&observers[index]);

    EXPECT_CALL(observers[index],
                OnDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN))
        .Times(3)
        .InSequence(mock_sequencer[index]);
    EXPECT_CALL(observers[index], OnRemovableStorageAttached(kDeviceId1,
                                                             kDeviceName,
                                                             testing::_))
        .InSequence(mock_sequencer[index]);
    EXPECT_CALL(observers[index], OnRemovableStorageDetached(kDeviceId1))
        .InSequence(mock_sequencer[index]);
    EXPECT_CALL(observers[index], OnRemovableStorageDetached(kDeviceId2))
        .Times(0).InSequence(mock_sequencer[index]);
  }

  system_monitor_->ProcessDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN);
  message_loop_.RunUntilIdle();

  system_monitor_->ProcessDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN);
  system_monitor_->ProcessDevicesChanged(SystemMonitor::DEVTYPE_UNKNOWN);
  message_loop_.RunUntilIdle();

  system_monitor_->ProcessRemovableStorageAttached(kDeviceId1,
                                                   kDeviceName,
                                                   FILE_PATH_LITERAL("path"));
  message_loop_.RunUntilIdle();

  system_monitor_->ProcessRemovableStorageDetached(kDeviceId1);
  system_monitor_->ProcessRemovableStorageDetached(kDeviceId2);
  message_loop_.RunUntilIdle();
}

TEST_F(SystemMonitorTest, GetAttachedRemovableStorageEmpty) {
  std::vector<SystemMonitor::RemovableStorageInfo> devices =
      system_monitor_->GetAttachedRemovableStorage();
  EXPECT_EQ(0U, devices.size());
}

TEST_F(SystemMonitorTest, GetAttachedRemovableStorageAttachDetach) {
  const std::string kDeviceId1 = "42";
  const string16 kDeviceName1 = ASCIIToUTF16("test");
  const FilePath kDevicePath1(FILE_PATH_LITERAL("/testfoo"));
  system_monitor_->ProcessRemovableStorageAttached(kDeviceId1,
                                                   kDeviceName1,
                                                   kDevicePath1.value());
  message_loop_.RunUntilIdle();
  std::vector<SystemMonitor::RemovableStorageInfo> devices =
      system_monitor_->GetAttachedRemovableStorage();
  ASSERT_EQ(1U, devices.size());
  EXPECT_EQ(kDeviceId1, devices[0].device_id);
  EXPECT_EQ(kDeviceName1, devices[0].name);
  EXPECT_EQ(kDevicePath1.value(), devices[0].location);

  const std::string kDeviceId2 = "44";
  const string16 kDeviceName2 = ASCIIToUTF16("test2");
  const FilePath kDevicePath2(FILE_PATH_LITERAL("/testbar"));
  system_monitor_->ProcessRemovableStorageAttached(kDeviceId2,
                                                   kDeviceName2,
                                                   kDevicePath2.value());
  message_loop_.RunUntilIdle();
  devices = system_monitor_->GetAttachedRemovableStorage();
  ASSERT_EQ(2U, devices.size());
  EXPECT_EQ(kDeviceId1, devices[0].device_id);
  EXPECT_EQ(kDeviceName1, devices[0].name);
  EXPECT_EQ(kDevicePath1.value(), devices[0].location);
  EXPECT_EQ(kDeviceId2, devices[1].device_id);
  EXPECT_EQ(kDeviceName2, devices[1].name);
  EXPECT_EQ(kDevicePath2.value(), devices[1].location);

  system_monitor_->ProcessRemovableStorageDetached(kDeviceId1);
  message_loop_.RunUntilIdle();
  devices = system_monitor_->GetAttachedRemovableStorage();
  ASSERT_EQ(1U, devices.size());
  EXPECT_EQ(kDeviceId2, devices[0].device_id);
  EXPECT_EQ(kDeviceName2, devices[0].name);
  EXPECT_EQ(kDevicePath2.value(), devices[0].location);

  system_monitor_->ProcessRemovableStorageDetached(kDeviceId2);
  message_loop_.RunUntilIdle();
  devices = system_monitor_->GetAttachedRemovableStorage();
  EXPECT_EQ(0U, devices.size());
}

}  // namespace

}  // namespace base
