// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kDevicePathUSB[] = "/dev/test-usb";
const char kDevicePathSD[] = "/dev/test-sd";
const char kMountPath[] = "/test-mount";
const char kDeviceId[] = "FFFF-FFFF";
const char kDeviceName[] = "Test Device Name";
const char kVendorName[] = "Test Vendor";
const char kProductName[] = "Test Product";
const uint64_t kDeviceSize = 1024 * 1024 * 1024;
const bool kOnRemovableDevice = true;
const char kDiskFileSystemType[] = "vfat";

const char kUnknownSDDiskModel[] = "SD Card";
const char kUnknownUSBDiskModel[] = "USB Drive";

class RemovableStorageProviderChromeOsUnitTest : public testing::Test {
 public:
  RemovableStorageProviderChromeOsUnitTest() {}
  void SetUp() override {
    disk_mount_manager_mock_ = new ash::disks::MockDiskMountManager();
    ash::disks::DiskMountManager::InitializeForTesting(
        disk_mount_manager_mock_);
    disk_mount_manager_mock_->SetupDefaultReplies();
  }

  void TearDown() override { ash::disks::DiskMountManager::Shutdown(); }

  void DevicesCallback(scoped_refptr<StorageDeviceList> devices) {
    devices_ = devices;
  }

  void CreateDisk(const std::string& device_path,
                  ash::DeviceType device_type,
                  bool is_parent,
                  bool has_media,
                  bool on_boot_device) {
    return CreateDisk(device_path,
                      kVendorName,
                      kProductName,
                      device_type,
                      is_parent,
                      has_media,
                      on_boot_device);
  }

  void CreateDisk(const std::string& device_path,
                  const std::string& vendor_name,
                  const std::string& product_name,
                  ash::DeviceType device_type,
                  bool is_parent,
                  bool has_media,
                  bool on_boot_device) {
    ash::disks::DiskMountManager::MountPoint mount_info{
        device_path, kMountPath, ash::MountType::kDevice};
    disk_mount_manager_mock_->CreateDiskEntryForMountDevice(
        mount_info, kDeviceId, kDeviceName, vendor_name, product_name,
        device_type, kDeviceSize, is_parent, has_media, on_boot_device,
        kOnRemovableDevice, kDiskFileSystemType);
  }

  // Checks if the DeviceList has a specific entry.
  api::image_writer_private::RemovableStorageDevice* FindDevice(
      StorageDeviceList* list,
      const std::string& file_path) {
    for (auto& device : list->data) {
      if (device.storage_unit_id == file_path)
        return &device;
    }
    return nullptr;
  }

  void ExpectDevice(StorageDeviceList* list,
                    const std::string& device_path,
                    const std::string& vendor,
                    const std::string& model,
                    uint64_t capacity) {
    auto* device = FindDevice(devices_.get(), device_path);

    ASSERT_TRUE(device);

    EXPECT_EQ(device_path, device->storage_unit_id);
    EXPECT_EQ(vendor, device->vendor);
    EXPECT_EQ(model, device->model);
    EXPECT_EQ(capacity, device->capacity);
  }

  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<ash::disks::MockDiskMountManager, DanglingUntriaged | ExperimentalAsh>
      disk_mount_manager_mock_;
  scoped_refptr<StorageDeviceList> devices_;
};

}  // namespace

// Tests that GetAllDevices works as expected, only exposing USB and SD cards
// that are parents, have media and are not boot devices.  Other flags are
// uninteresting or should not occur for these device types.
TEST_F(RemovableStorageProviderChromeOsUnitTest, GetAllDevices) {
  CreateDisk(kDevicePathUSB, ash::DeviceType::kUSB, true, true, false);
  CreateDisk(kDevicePathSD, ash::DeviceType::kSD, true, true, false);
  CreateDisk("/dev/NotParent", ash::DeviceType::kUSB, false, true, false);
  CreateDisk("/dev/NoMedia", ash::DeviceType::kUSB, true, false, false);
  CreateDisk("/dev/OnBootDevice", ash::DeviceType::kUSB, true, true, true);

  RemovableStorageProvider::GetAllDevices(
      base::BindOnce(&RemovableStorageProviderChromeOsUnitTest::DevicesCallback,
                     base::Unretained(this)));

  task_environment_.RunUntilIdle();

  ASSERT_EQ(2U, devices_->data.size());

  ExpectDevice(
      devices_.get(), kDevicePathUSB, kVendorName, kProductName, kDeviceSize);
  ExpectDevice(
      devices_.get(), kDevicePathSD, kVendorName, kProductName, kDeviceSize);
}

// Tests that a USB drive with an empty vendor and product gets a generic name.
TEST_F(RemovableStorageProviderChromeOsUnitTest, EmptyProductAndModel) {
  CreateDisk(kDevicePathUSB, "", "", ash::DeviceType::kUSB, true, true, false);
  CreateDisk(kDevicePathSD, "", "", ash::DeviceType::kSD, true, true, false);

  RemovableStorageProvider::GetAllDevices(
      base::BindOnce(&RemovableStorageProviderChromeOsUnitTest::DevicesCallback,
                     base::Unretained(this)));

  task_environment_.RunUntilIdle();

  ASSERT_EQ(2U, devices_->data.size());

  ExpectDevice(
      devices_.get(), kDevicePathUSB, "", kUnknownUSBDiskModel, kDeviceSize);
  ExpectDevice(
      devices_.get(), kDevicePathSD, "", kUnknownSDDiskModel, kDeviceSize);
}

}  // namespace extensions
