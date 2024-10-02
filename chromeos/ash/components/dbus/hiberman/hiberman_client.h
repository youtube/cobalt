// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HIBERMAN_HIBERMAN_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HIBERMAN_HIBERMAN_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"

namespace dbus {
class Bus;
}

namespace ash {

// HibermanClient is used to communicate with the org.chromium.Hibernate
// service exposed by hiberman. All method should be called from the origin
// thread (UI thread) which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(HIBERMAN_CLIENT) HibermanClient {
 public:
  using ResumeFromHibernateCallback = chromeos::VoidDBusMethodCallback;

  // Not copyable or movable.
  HibermanClient(const HibermanClient&) = delete;
  HibermanClient& operator=(const HibermanClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static HibermanClient* Get();

  virtual bool IsAlive() const = 0;

  // Actual DBus Methods:
  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) = 0;

  // Resume from hibernate, if possible. Upon a successful resume from
  // hibernation, this function does not return, as execution continues in the
  // resumed image.
  virtual void ResumeFromHibernate(const std::string& account_id,
                                   ResumeFromHibernateCallback callback) = 0;

  // Resume from hibernate with an auth session, if possible. Upon a successful
  // resume from hibernation, this function does not return, as execution
  // continues in the resumed image.
  virtual void ResumeFromHibernateAS(const std::string& auth_session_id,
                                     ResumeFromHibernateCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  HibermanClient();
  virtual ~HibermanClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HIBERMAN_HIBERMAN_CLIENT_H_
