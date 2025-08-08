// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_SERVICE_H_
#define SERVICES_DEVICE_HID_HID_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "services/device/hid/hid_device_info.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class HidConnection;

// The HidService keeps track of human interface devices connected to the
// system. Call HidService::GetInstance to get the singleton instance.
class HidService {
 public:
  // Clients of HidService should add themselves as observer in their
  // GetDevicesCallback. Earlier might cause OnDeviceAdded() and
  // OnDeviceRemoved() to be called before the GetDevicesCallback, while later
  // might cause missing OnDeviceAdded() and OnDeviceRemoved() notifications.
  class Observer {
   public:
    virtual void OnDeviceAdded(mojom::HidDeviceInfoPtr info);
    // Notifies all observers that a device is being removed, called before
    // removing the device from HidService. Observers should not depend on the
    // order in which they are notified of the OnDeviceRemove event.
    virtual void OnDeviceRemoved(mojom::HidDeviceInfoPtr info);
    // Notifies all observers that the device info for a device was updated.
    virtual void OnDeviceChanged(mojom::HidDeviceInfoPtr info);
  };

  using GetDevicesCallback =
      base::OnceCallback<void(std::vector<mojom::HidDeviceInfoPtr>)>;
  using ConnectCallback =
      base::OnceCallback<void(scoped_refptr<HidConnection> connection)>;

  // These task traits are to be used for posting blocking tasks to the thread
  // pool.
  static constexpr base::TaskTraits kBlockingTaskTraits = {
      base::MayBlock(), base::TaskPriority::USER_VISIBLE,
      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

  // This function should be called on a thread with a MessageLoopForUI.
  static std::unique_ptr<HidService> Create();

  HidService(const HidService&) = delete;
  HidService& operator=(const HidService&) = delete;
  virtual ~HidService();

  // Enumerates available devices. The provided callback will always be posted
  // to the calling thread's task runner.
  void GetDevices(GetDevicesCallback callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Opens a connection to a device. The callback will be run with null on
  // failure.
  virtual void Connect(const std::string& device_guid,
                       bool allow_protected_reports,
                       bool allow_fido_reports,
                       ConnectCallback callback) = 0;

 protected:
  friend class HidConnectionTest;

  typedef std::map<std::string, scoped_refptr<HidDeviceInfo>> DeviceMap;

  HidService();

  virtual base::WeakPtr<HidService> GetWeakPtr() = 0;
  void AddDevice(scoped_refptr<HidDeviceInfo> info);
  void RemoveDevice(const HidPlatformDeviceId& platform_device_id);
  void FirstEnumerationComplete();

  const DeviceMap& devices() const { return devices_; }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void RunPendingEnumerations();
  absl::optional<std::string> FindDeviceGuidInDeviceMap(
      const HidPlatformDeviceId& platform_device_id);
  scoped_refptr<HidDeviceInfo> FindSiblingDevice(
      const HidDeviceInfo& device_info) const;

  DeviceMap devices_;

  bool enumeration_ready_ = false;
  std::vector<GetDevicesCallback> pending_enumerations_;
  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_SERVICE_H_
