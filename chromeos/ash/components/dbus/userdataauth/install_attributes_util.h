// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_INSTALL_ATTRIBUTES_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_INSTALL_ATTRIBUTES_UTIL_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/userdataauth/install_attributes_client.h"

namespace ash::install_attributes_util {

// Wrappers for calls to InstallAttributesClient. Must be called from the UI
// thread. NOTE: Some of these make blocking DBus calls which will spin until
// the DBus call completes. They should be avoided if possible.

// Blocking calls to InstallAttributesClient methods.
COMPONENT_EXPORT(USERDATAAUTH_CLIENT)
bool InstallAttributesGet(const std::string& name, std::string* value);
COMPONENT_EXPORT(USERDATAAUTH_CLIENT)
bool InstallAttributesSet(const std::string& name, const std::string& value);
COMPONENT_EXPORT(USERDATAAUTH_CLIENT) bool InstallAttributesFinalize();
COMPONENT_EXPORT(USERDATAAUTH_CLIENT)
user_data_auth::InstallAttributesState InstallAttributesGetStatus();
COMPONENT_EXPORT(USERDATAAUTH_CLIENT) bool InstallAttributesIsInvalid();
COMPONENT_EXPORT(USERDATAAUTH_CLIENT) bool InstallAttributesIsFirstInstall();

}  // namespace ash::install_attributes_util

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_INSTALL_ATTRIBUTES_UTIL_H_
