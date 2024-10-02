// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_MANAGER_IMPL_H_
#define SERVICES_DEVICE_HID_HID_MANAGER_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/hid/hid_device_info.h"
#include "services/device/hid/hid_service.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

// HidManagerImpl is owned by Device Service. It is reponsible for handling mojo
// communications from clients. It delegates to HidService the real work of
// talking with different platforms.
class HidManagerImpl : public mojom::HidManager, public HidService::Observer {
 public:
  HidManagerImpl();
  HidManagerImpl(HidManagerImpl&) = delete;
  HidManagerImpl& operator=(HidManagerImpl&) = delete;
  ~HidManagerImpl() override;

  // SetHidServiceForTesting only effects the next call to HidManagerImpl's
  // constructor in which the HidManagerImpl will take over the ownership of
  // passed |hid_service|.
  static void SetHidServiceForTesting(std::unique_ptr<HidService> hid_service);

  // IsHidServiceTesting() will return true when the next call to the
  // constructor will use the HidService instance set by
  // SetHidServiceForTesting().
  static bool IsHidServiceTesting();

  void AddReceiver(mojo::PendingReceiver<mojom::HidManager> receiver) override;

  // mojom::HidManager implementation:
  void GetDevicesAndSetClient(
      mojo::PendingAssociatedRemote<mojom::HidManagerClient> client,
      GetDevicesCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void Connect(
      const std::string& device_guid,
      mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
      mojo::PendingRemote<mojom::HidConnectionWatcher> watcher,
      bool allow_protected_reports,
      bool allow_fido_reports,
      ConnectCallback callback) override;

 private:
  void CreateDeviceList(
      GetDevicesCallback callback,
      mojo::PendingAssociatedRemote<mojom::HidManagerClient> client,
      std::vector<mojom::HidDeviceInfoPtr> devices);

  void CreateConnection(
      ConnectCallback callback,
      mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
      mojo::PendingRemote<mojom::HidConnectionWatcher> watcher,
      scoped_refptr<HidConnection> connection);

  // HidService::Observer:
  void OnDeviceAdded(mojom::HidDeviceInfoPtr device_info) override;
  void OnDeviceRemoved(mojom::HidDeviceInfoPtr device_info) override;
  void OnDeviceChanged(mojom::HidDeviceInfoPtr device_info) override;

  std::unique_ptr<HidService> hid_service_;
  mojo::ReceiverSet<mojom::HidManager> receivers_;
  mojo::AssociatedRemoteSet<mojom::HidManagerClient> clients_;
  base::ScopedObservation<HidService, HidService::Observer>
      hid_service_observation_{this};

  base::WeakPtrFactory<HidManagerImpl> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_MANAGER_IMPL_H_
