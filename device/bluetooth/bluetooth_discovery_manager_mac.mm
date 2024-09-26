// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_discovery_manager_mac.h"

#include "base/memory/raw_ptr.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothDeviceInquiry.h>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"

namespace device {

class BluetoothDiscoveryManagerMacClassic;

}  // namespace device

// IOBluetoothDeviceInquiryDelegate implementation.
@interface BluetoothDeviceInquiryDelegate
    : NSObject<IOBluetoothDeviceInquiryDelegate> {
 @private
  raw_ptr<device::BluetoothDiscoveryManagerMacClassic> _manager;  // weak
}

- (instancetype)initWithManager:
    (device::BluetoothDiscoveryManagerMacClassic*)manager;

@end

namespace device {

// Implementation of BluetoothDiscoveryManagerMac for Bluetooth classic device
// discovery, using the IOBluetooth framework.
class BluetoothDiscoveryManagerMacClassic
    : public BluetoothDiscoveryManagerMac {
 public:
  explicit BluetoothDiscoveryManagerMacClassic(Observer* observer)
      : BluetoothDiscoveryManagerMac(observer),
        should_do_discovery_(false),
        inquiry_running_(false),
        inquiry_delegate_(
            [[BluetoothDeviceInquiryDelegate alloc] initWithManager:this]),
        inquiry_([[IOBluetoothDeviceInquiry alloc]
            initWithDelegate:inquiry_delegate_]) {}

  BluetoothDiscoveryManagerMacClassic(
      const BluetoothDiscoveryManagerMacClassic&) = delete;
  BluetoothDiscoveryManagerMacClassic& operator=(
      const BluetoothDiscoveryManagerMacClassic&) = delete;

  ~BluetoothDiscoveryManagerMacClassic() override {}

  // BluetoothDiscoveryManagerMac override.
  bool IsDiscovering() const override { return should_do_discovery_; }

  // BluetoothDiscoveryManagerMac override.
  bool StartDiscovery() override {
    DVLOG(1) << "Bluetooth Classic: StartDiscovery";
    DCHECK(!should_do_discovery_);

    DVLOG(1) << "Discovery requested";
    should_do_discovery_ = true;

    // Clean the cache so that new discovery sessions find previously
    // discovered devices as well.
    [inquiry_ clearFoundDevices];

    if (inquiry_running_) {
      DVLOG(1) << "Device inquiry already running";
      return true;
    }

    DVLOG(1) << "Requesting to start device inquiry";
    if ([inquiry_ start] != kIOReturnSuccess) {
      DVLOG(1) << "Failed to start device inquiry";

      // Set |should_do_discovery_| to false here. Since we're reporting an
      // error, we're indicating that the adapter call StartDiscovery again
      // if needed.
      should_do_discovery_ = false;
      return false;
    }

    DVLOG(1) << "Device inquiry start was successful";
    return true;
  }

  // BluetoothDiscoveryManagerMac override.
  bool StopDiscovery() override {
    DVLOG(1) << "Bluetooth Classic: StopDiscovery";
    DCHECK(should_do_discovery_);

    should_do_discovery_ = false;

    if (!inquiry_running_) {
      DVLOG(1) << "No device inquiry running; discovery stopped";
      return true;
    }

    DVLOG(1) << "Requesting to stop device inquiry";
    IOReturn status = [inquiry_ stop];
    if (status == kIOReturnSuccess) {
      DVLOG(1) << "Device inquiry stop was successful";
      return true;
    }

    if (status == kIOReturnNotPermitted) {
      DVLOG(1) << "Device inquiry was already stopped";
      return true;
    }

    LOG(WARNING) << "Failed to stop device inquiry";
    return false;
  }

  // Called by BluetoothDeviceInquiryDelegate.
  void DeviceInquiryStarted(IOBluetoothDeviceInquiry* inquiry) {
    DCHECK(!inquiry_running_);

    DVLOG(1) << "Device inquiry started!";

    // If discovery was requested to stop in the mean time, stop the inquiry.
    if (!should_do_discovery_) {
      DVLOG(1) << "Discovery stop was requested earlier. Stopping inquiry";
      [inquiry stop];
      return;
    }

    inquiry_running_ = true;
  }

  void DeviceFound(IOBluetoothDeviceInquiry* inquiry,
                   IOBluetoothDevice* device) {
    DCHECK(observer_);
    observer_->ClassicDeviceFound(device);
  }

  void DeviceInquiryComplete(IOBluetoothDeviceInquiry* inquiry,
                             IOReturn error,
                             bool aborted) {
    DCHECK_EQ(inquiry_, inquiry);
    DCHECK(observer_);
    DVLOG(1) << "Device inquiry complete";
    inquiry_running_ = false;

    // If discovery is no longer desired, notify observers that discovery
    // has stopped and return.
    if (!should_do_discovery_) {
      observer_->ClassicDiscoveryStopped(false /* unexpected */);
      return;
    }

    // If discovery has stopped due to an unexpected reason, notify the
    // observers and return.
    if (error != kIOReturnSuccess) {
      DVLOG(1) << "Inquiry has stopped with an error: " << error;
      should_do_discovery_ = false;
      observer_->ClassicDiscoveryStopped(true /* unexpected */);
      return;
    }

    DVLOG(1) << "Restarting device inquiry";

    if ([inquiry_ start] == kIOReturnSuccess) {
      DVLOG(1) << "Device inquiry restart was successful";
      return;
    }

    DVLOG(1) << "Failed to restart discovery";
    should_do_discovery_ = false;
    DCHECK(observer_);
    observer_->ClassicDiscoveryStopped(true /* unexpected */);
  }

 private:
  // The requested discovery state.
  bool should_do_discovery_;

  // The current inquiry state.
  bool inquiry_running_;

  // Objective-C objects for running and tracking device inquiry.
  base::scoped_nsobject<BluetoothDeviceInquiryDelegate> inquiry_delegate_;
  base::scoped_nsobject<IOBluetoothDeviceInquiry> inquiry_;
};

BluetoothDiscoveryManagerMac::BluetoothDiscoveryManagerMac(
    Observer* observer) : observer_(observer) {
  DCHECK(observer);
}

BluetoothDiscoveryManagerMac::~BluetoothDiscoveryManagerMac() {
}

// static
BluetoothDiscoveryManagerMac* BluetoothDiscoveryManagerMac::CreateClassic(
    Observer* observer) {
  return new BluetoothDiscoveryManagerMacClassic(observer);
}

}  // namespace device

@implementation BluetoothDeviceInquiryDelegate

- (instancetype)initWithManager:
    (device::BluetoothDiscoveryManagerMacClassic*)manager {
  if ((self = [super init]))
    _manager = manager;

  return self;
}

- (void)deviceInquiryStarted:(IOBluetoothDeviceInquiry*)sender {
  _manager->DeviceInquiryStarted(sender);
}

- (void)deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry*)sender
                          device:(IOBluetoothDevice*)device {
  _manager->DeviceFound(sender, device);
}

- (void)deviceInquiryComplete:(IOBluetoothDeviceInquiry*)sender
                        error:(IOReturn)error
                      aborted:(BOOL)aborted {
  _manager->DeviceInquiryComplete(sender, error, aborted);
}

@end
