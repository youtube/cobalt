// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_MANAGER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_MANAGER_H_

#include "services/device/public/cpp/geolocation/geolocation_manager.h"

namespace device {

class FakeGeolocationManager : public GeolocationManager {
 public:
  FakeGeolocationManager();
  ~FakeGeolocationManager() override = default;

  void SetSystemPermission(LocationSystemPermissionStatus status);
#if BUILDFLAG(IS_APPLE)
  bool watching_position();
  void FakePositionUpdated(device::mojom::GeopositionResultPtr result);
#endif  // BUILDFLAG(IS_MAC)
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_GEOLOCATION_MANAGER_H_
