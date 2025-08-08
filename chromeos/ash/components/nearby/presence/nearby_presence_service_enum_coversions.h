// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_ENUM_COVERSIONS_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_ENUM_COVERSIONS_H_

#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"

namespace ash::nearby::presence {

::nearby::internal::DeviceType ConvertMojomDeviceType(
    mojom::PresenceDeviceType mojom_type);

NearbyPresenceService::PresenceIdentityType ConvertToMojomIdentityType(
    NearbyPresenceService::IdentityType identity_type_);

NearbyPresenceService::StatusCode ConvertToPresenceStatus(
    mojo_base::mojom::AbslStatusCode status_code);

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_ENUM_COVERSIONS_H_
