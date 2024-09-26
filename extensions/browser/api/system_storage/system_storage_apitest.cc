// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <atomic>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "extensions/browser/api/system_storage/storage_api_test_util.h"
#include "extensions/browser/api/system_storage/storage_info_provider.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace {

using extensions::StorageUnitInfoList;
using extensions::test::TestStorageUnitInfo;
using extensions::test::kRemovableStorageData;
using storage_monitor::StorageMonitor;
using storage_monitor::TestStorageMonitor;

const struct TestStorageUnitInfo kTestingData[] = {
    {"dcim:device:001", "0xbeaf", 4098, 1},
    {"path:device:002", "/home", 4098, 2},
    {"path:device:003", "/data", 10000, 3}};

}  // namespace

class TestStorageInfoProvider : public extensions::StorageInfoProvider {
 public:
  TestStorageInfoProvider(const struct TestStorageUnitInfo* testing_data,
                          size_t n);

  void set_expected_call_count(int count) { expected_call_count_ = count; }

  void WaitForCallbacks() {
    DCHECK(expected_call_count_);
    if (callback_count_ != expected_call_count_) {
      run_loop_.Run();
    }
  }

 private:
  ~TestStorageInfoProvider() override;

  // StorageInfoProvider implementations.
  double GetStorageFreeSpaceFromTransientIdAsync(
      const std::string& transient_id) override;

  std::vector<struct TestStorageUnitInfo> testing_data_;

  // Read on the IO thread, written from another thread.
  std::atomic<int> callback_count_{0};

  // Assumed to be read-only once the test starts.
  int expected_call_count_ = 0;

  base::RunLoop run_loop_;
};

TestStorageInfoProvider::TestStorageInfoProvider(
    const struct TestStorageUnitInfo* testing_data,
    size_t n)
    : testing_data_(testing_data, testing_data + n) {
}

TestStorageInfoProvider::~TestStorageInfoProvider() = default;

double TestStorageInfoProvider::GetStorageFreeSpaceFromTransientIdAsync(
    const std::string& transient_id) {
  double result = -1;
  std::string device_id =
      StorageMonitor::GetInstance()->GetDeviceIdForTransientId(transient_id);
  for (const auto& info : testing_data_) {
    if (info.device_id == device_id) {
      result = static_cast<double>(info.available_capacity);
      break;
    }
  }
  if (++callback_count_ == expected_call_count_)
    run_loop_.QuitWhenIdle();
  return result;
}

class SystemStorageApiTest : public extensions::ShellApiTest {
 public:
  SystemStorageApiTest() = default;
  ~SystemStorageApiTest() override = default;

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    TestStorageMonitor::CreateForBrowserTests();
  }

  void SetUpAllMockStorageDevices() {
    for (const auto& entry : kTestingData) {
      AttachRemovableStorage(entry);
    }
  }

  void AttachRemovableStorage(
      const struct TestStorageUnitInfo& removable_storage_info) {
    StorageMonitor::GetInstance()->receiver()->ProcessAttach(
        extensions::test::BuildStorageInfoFromTestStorageUnitInfo(
            removable_storage_info));
  }

  void DetachRemovableStorage(const std::string& id) {
    StorageMonitor::GetInstance()->receiver()->ProcessDetach(id);
  }
};

IN_PROC_BROWSER_TEST_F(SystemStorageApiTest, Storage) {
  SetUpAllMockStorageDevices();
  auto provider = base::MakeRefCounted<TestStorageInfoProvider>(
      kTestingData, std::size(kTestingData));
  extensions::StorageInfoProvider::InitializeForTesting(provider);
  std::vector<std::unique_ptr<ExtensionTestMessageListener>>
      device_ids_listeners;
  for (const auto& entry : kTestingData) {
    device_ids_listeners.push_back(
        std::make_unique<ExtensionTestMessageListener>(
            StorageMonitor::GetInstance()->GetTransientIdForDeviceId(
                entry.device_id)));
  }
  // Set the number of expected callbacks into the StorageInfoProvider.
  provider->set_expected_call_count(device_ids_listeners.size());
  ASSERT_TRUE(RunAppTest("system/storage")) << message_;
  for (const auto& listener : device_ids_listeners)
    EXPECT_TRUE(listener->WaitUntilSatisfied());
  // Wait for the callbacks to complete so they don't run during
  // teardown.
  provider->WaitForCallbacks();
}

IN_PROC_BROWSER_TEST_F(SystemStorageApiTest, StorageAttachment) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener attach_listener("attach");
  ExtensionTestMessageListener detach_listener("detach");

  EXPECT_TRUE(LoadApp("system/storage_attachment"));
  // Simulate triggering onAttached event.
  ASSERT_TRUE(attach_listener.WaitUntilSatisfied());

  AttachRemovableStorage(kRemovableStorageData);

  std::string removable_storage_transient_id =
      StorageMonitor::GetInstance()->GetTransientIdForDeviceId(
          kRemovableStorageData.device_id);
  ExtensionTestMessageListener detach_device_id_listener(
      removable_storage_transient_id);

  // Simulate triggering onDetached event.
  ASSERT_TRUE(detach_listener.WaitUntilSatisfied());
  DetachRemovableStorage(kRemovableStorageData.device_id);

  ASSERT_TRUE(detach_device_id_listener.WaitUntilSatisfied());

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
