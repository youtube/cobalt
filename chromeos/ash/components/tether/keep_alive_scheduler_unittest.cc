// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/keep_alive_scheduler.h"

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/mock_timer.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/ash/components/tether/fake_active_host.h"
#include "chromeos/ash/components/tether/fake_host_scan_cache.h"
#include "chromeos/ash/components/tether/proto_test_util.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

const char kTetherNetworkGuid[] = "tetherNetworkGuid";
const char kWifiNetworkGuid[] = "wifiNetworkGuid";

class OperationDeletedHandler {
 public:
  virtual void OnOperationDeleted() = 0;
};

class FakeKeepAliveOperation : public KeepAliveOperation {
 public:
  FakeKeepAliveOperation(
      multidevice::RemoteDeviceRef device_to_connect,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      OperationDeletedHandler* handler)
      : KeepAliveOperation(device_to_connect,
                           device_sync_client,
                           secure_channel_client),
        handler_(handler),
        remote_device_(device_to_connect) {}

  ~FakeKeepAliveOperation() override { handler_->OnOperationDeleted(); }

  void SendOperationFinishedEvent(std::unique_ptr<DeviceStatus> device_status) {
    device_status_ = std::move(device_status);
    OnOperationFinished();
  }

  multidevice::RemoteDeviceRef remote_device() { return remote_device_; }

 private:
  raw_ptr<OperationDeletedHandler, ExperimentalAsh> handler_;
  const multidevice::RemoteDeviceRef remote_device_;
};

class FakeKeepAliveOperationFactory final : public KeepAliveOperation::Factory,
                                            public OperationDeletedHandler {
 public:
  FakeKeepAliveOperationFactory()
      : num_created_(0), num_deleted_(0), last_created_(nullptr) {}
  ~FakeKeepAliveOperationFactory() override = default;

  uint32_t num_created() { return num_created_; }

  uint32_t num_deleted() { return num_deleted_; }

  FakeKeepAliveOperation* last_created() { return last_created_; }

  void OnOperationDeleted() override { num_deleted_++; }

 protected:
  std::unique_ptr<KeepAliveOperation> CreateInstance(
      multidevice::RemoteDeviceRef device_to_connect,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client) override {
    num_created_++;
    last_created_ = new FakeKeepAliveOperation(
        device_to_connect, device_sync_client, secure_channel_client, this);
    return base::WrapUnique(last_created_.get());
  }

 private:
  uint32_t num_created_;
  uint32_t num_deleted_;
  raw_ptr<FakeKeepAliveOperation, ExperimentalAsh> last_created_;
};

}  // namespace

class KeepAliveSchedulerTest : public testing::Test {
 public:
  KeepAliveSchedulerTest(const KeepAliveSchedulerTest&) = delete;
  KeepAliveSchedulerTest& operator=(const KeepAliveSchedulerTest&) = delete;

 protected:
  KeepAliveSchedulerTest()
      : test_devices_(multidevice::CreateRemoteDeviceRefListForTest(2)) {}

  void SetUp() override {
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    fake_active_host_ = std::make_unique<FakeActiveHost>();
    fake_host_scan_cache_ = std::make_unique<FakeHostScanCache>();
    device_id_tether_network_guid_map_ =
        std::make_unique<DeviceIdTetherNetworkGuidMap>();
    mock_timer_ = new base::MockRepeatingTimer();

    fake_operation_factory_ =
        base::WrapUnique(new FakeKeepAliveOperationFactory());
    KeepAliveOperation::Factory::SetFactoryForTesting(
        fake_operation_factory_.get());

    scheduler_ = base::WrapUnique(new KeepAliveScheduler(
        fake_device_sync_client_.get(), fake_secure_channel_client_.get(),
        fake_active_host_.get(), fake_host_scan_cache_.get(),
        device_id_tether_network_guid_map_.get(),
        base::WrapUnique(mock_timer_.get())));
  }

  void VerifyTimerRunning(bool is_running) {
    EXPECT_EQ(is_running, mock_timer_->IsRunning());

    if (is_running) {
      EXPECT_EQ(base::Minutes(KeepAliveScheduler::kKeepAliveIntervalMinutes),
                mock_timer_->GetCurrentDelay());
    }
  }

  void SendOperationFinishedEventFromLastCreatedOperation(
      const std::string& cell_provider,
      int battery_percentage,
      int connection_strength) {
    fake_operation_factory_->last_created()->SendOperationFinishedEvent(
        std::make_unique<DeviceStatus>(CreateTestDeviceStatus(
            cell_provider, battery_percentage, connection_strength)));
  }

  void VerifyCacheUpdated(multidevice::RemoteDeviceRef remote_device,
                          const std::string& carrier,
                          int battery_percentage,
                          int signal_strength) {
    const HostScanCacheEntry* entry = fake_host_scan_cache_->GetCacheEntry(
        device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
            remote_device.GetDeviceId()));
    ASSERT_TRUE(entry);
    EXPECT_EQ(carrier, entry->carrier);
    EXPECT_EQ(battery_percentage, entry->battery_percentage);
    EXPECT_EQ(signal_strength, entry->signal_strength);
  }

  const multidevice::RemoteDeviceRefList test_devices_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::SecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeHostScanCache> fake_host_scan_cache_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;
  raw_ptr<base::MockRepeatingTimer, ExperimentalAsh> mock_timer_;

  std::unique_ptr<FakeKeepAliveOperationFactory> fake_operation_factory_;

  std::unique_ptr<KeepAliveScheduler> scheduler_;
};

TEST_F(KeepAliveSchedulerTest, DISABLED_TestSendTickle_OneActiveHost) {
  EXPECT_FALSE(fake_operation_factory_->num_created());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Start connecting to a device. No operation should be started.
  fake_active_host_->SetActiveHostConnecting(test_devices_[0].GetDeviceId(),
                                             std::string(kTetherNetworkGuid));
  EXPECT_FALSE(fake_operation_factory_->num_created());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Connect to the device; the operation should be started.
  fake_active_host_->SetActiveHostConnected(test_devices_[0].GetDeviceId(),
                                            std::string(kTetherNetworkGuid),
                                            std::string(kWifiNetworkGuid));
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_devices_[0],
            fake_operation_factory_->last_created()->remote_device());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Ensure that once the operation is finished, it is deleted.
  SendOperationFinishedEventFromLastCreatedOperation(
      "cellProvider", 50 /* battery_percentage */, 2 /* connection_strength */);
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);
  VerifyCacheUpdated(test_devices_[0], "cellProvider",
                     50 /* battery_percentage */, 50 /* signal_strength */);

  // Fire the timer; this should result in tickle #2 being sent.
  mock_timer_->Fire();
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_devices_[0],
            fake_operation_factory_->last_created()->remote_device());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Finish tickle operation #2.
  SendOperationFinishedEventFromLastCreatedOperation(
      "cellProvider", 40 /* battery_percentage */, 3 /* connection_strength */);
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(2u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);
  VerifyCacheUpdated(test_devices_[0], "cellProvider",
                     40 /* battery_percentage */, 75 /* signal_strength */);

  // Fire the timer; this should result in tickle #3 being sent.
  mock_timer_->Fire();
  EXPECT_EQ(3u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_devices_[0],
            fake_operation_factory_->last_created()->remote_device());
  EXPECT_EQ(2u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Finish tickler operation #3. This time, simulate a failure to receive a
  // DeviceStatus back.
  fake_operation_factory_->last_created()->SendOperationFinishedEvent(nullptr);
  EXPECT_EQ(3u, fake_operation_factory_->num_created());
  EXPECT_EQ(3u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);
  // The same data returned by tickle #2 should be present.
  VerifyCacheUpdated(test_devices_[0], "cellProvider",
                     40 /* battery_percentage */, 75 /* signal_strength */);

  // Disconnect that device.
  fake_active_host_->SetActiveHostDisconnected();
  EXPECT_EQ(3u, fake_operation_factory_->num_created());
  EXPECT_EQ(3u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);
}

TEST_F(KeepAliveSchedulerTest, DISABLED_TestSendTickle_MultipleActiveHosts) {
  EXPECT_FALSE(fake_operation_factory_->num_created());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Start connecting to a device. No operation should be started.
  fake_active_host_->SetActiveHostConnecting(test_devices_[0].GetDeviceId(),
                                             std::string(kTetherNetworkGuid));
  EXPECT_FALSE(fake_operation_factory_->num_created());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Connect to the device; the operation should be started.
  fake_active_host_->SetActiveHostConnected(test_devices_[0].GetDeviceId(),
                                            std::string(kTetherNetworkGuid),
                                            std::string(kWifiNetworkGuid));
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_devices_[0],
            fake_operation_factory_->last_created()->remote_device());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Disconnect that device before the operation is finished. It should still be
  // deleted.
  fake_active_host_->SetActiveHostDisconnected();
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Start connecting to a different. No operation should be started.
  fake_active_host_->SetActiveHostConnecting(test_devices_[1].GetDeviceId(),
                                             std::string(kTetherNetworkGuid));
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Connect to the second device; the operation should be started.
  fake_active_host_->SetActiveHostConnected(test_devices_[1].GetDeviceId(),
                                            std::string(kTetherNetworkGuid),
                                            std::string(kWifiNetworkGuid));
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_devices_[1],
            fake_operation_factory_->last_created()->remote_device());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Ensure that once the second operation is finished, it is deleted.
  SendOperationFinishedEventFromLastCreatedOperation(
      "cellProvider", 80 /* battery_percentage */, 4 /* connection_strength */);
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(2u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);
  VerifyCacheUpdated(test_devices_[1], "cellProvider",
                     80 /* battery_percentage */, 100 /* signal_strength */);

  // Disconnect that device.
  fake_active_host_->SetActiveHostDisconnected();
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(2u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);
}

}  // namespace tether

}  // namespace ash
