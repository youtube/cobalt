// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_UTILS_H_

#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/network/device_state.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace ash {

class CellularESimProfile;

// Generates a list of CellularESimProfile objects for all Hermes esim profile
// objects available through its dbus clients. Note that this function returns
// an empty array if CellularESimProfileHandler::RefreshProfileList has not
// been called. CellularESimProfileHandler::GetESimProfiles() should be called
// to fetch cached profiles.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::vector<CellularESimProfile> GenerateProfilesFromHermes();

// Generates a list of CellularSIMSlotInfo objects with missing EIDs
// populated by EIDs known by Hermes.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
const DeviceState::CellularSIMSlotInfos GetSimSlotInfosWithUpdatedEid(
    const DeviceState* device);

// Returns true if SIM with given |iccid| is in a primary slot on given
// cellular |device|.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool IsSimPrimary(const std::string& iccid, const DeviceState* device);

// Generates a service path corresponding to a stub cellular network (i.e., one
// that is not backed by Shill).
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string GenerateStubCellularServicePath(const std::string& iccid);

COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool IsStubCellularServicePath(const std::string& service_path);

// Returns the path to the Euicc that is currently used for all eSIM operations
// in OS Settings and System UI.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
absl::optional<dbus::ObjectPath> GetCurrentEuiccPath();

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CELLULAR_UTILS_H_
