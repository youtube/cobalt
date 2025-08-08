// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USB_USBGUARD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USB_USBGUARD_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/usb/usbguard_observer.h"

namespace dbus {
class Bus;
}

namespace ash {

class COMPONENT_EXPORT(USB_CLIENT) UsbguardClient {
 public:
  UsbguardClient(const UsbguardClient&) = delete;
  UsbguardClient& operator=(const UsbguardClient&) = delete;

  virtual ~UsbguardClient();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static UsbguardClient* Get();

  // Adds the given observer.
  virtual void AddObserver(UsbguardObserver* observer) = 0;
  // Removes the given observer if this object has the observer.
  virtual void RemoveObserver(UsbguardObserver* observer) = 0;
  // Returns true if this object has the given observer.
  virtual bool HasObserver(const UsbguardObserver* observer) const = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  UsbguardClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USB_USBGUARD_CLIENT_H_
