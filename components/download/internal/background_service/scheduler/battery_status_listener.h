// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_BATTERY_STATUS_LISTENER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_BATTERY_STATUS_LISTENER_H_

namespace download {

// Interface to monitor device battery status.
class BatteryStatusListener {
 public:
  class Observer {
   public:
    // Called when device charging state changed.
    virtual void OnPowerStateChange(bool on_battery_power) = 0;

   protected:
    virtual ~Observer() {}
  };
  virtual ~BatteryStatusListener() = default;

  // Get the device battery percentage.
  virtual int GetBatteryPercentage() = 0;

  // Is the device is using battery power instead of charging.
  virtual bool IsOnBatteryPower() = 0;

  // Start/Stop to listen to battery status changes.
  virtual void Start(Observer* observer) = 0;
  virtual void Stop() = 0;

 protected:
  BatteryStatusListener() = default;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_BATTERY_STATUS_LISTENER_H_
