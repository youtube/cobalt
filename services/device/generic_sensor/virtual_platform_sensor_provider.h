// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_VIRTUAL_PLATFORM_SENSOR_PROVIDER_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_VIRTUAL_PLATFORM_SENSOR_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "services/device/public/mojom/sensor_provider.mojom-forward.h"

namespace device {

// OS-agnostic PlatformSensorProvider implementation used in (web) tests. Used
// together with SensorProviderImpl in order to override one or more sensor
// types. Calls to GetSensor() will, upon success, always create
// VirtualPlatformSensor instances.
class VirtualPlatformSensorProvider : public PlatformSensorProvider {
 public:
  VirtualPlatformSensorProvider();
  ~VirtualPlatformSensorProvider() override;

  // Starts causing GetSensor() calls with |type| to return
  // VirtualPlatformSensor instances with the properties specified in
  // |metadata|.
  //
  // Returns false if the given |type| is already being overridden (in which
  // case RemoveSensorOverride() must be called first).
  bool AddSensorOverride(mojom::SensorType type,
                         mojom::VirtualSensorMetadataPtr metadata);

  // Stops overriding sensors of type |type|, that is, GetSensor() calls with
  // |type| will no longer a VirtualPlatformSensor instance. Existing instances
  // will behave as if a hardware sensor had been removed (i.e. possibly notify
  // clients of an error).
  //
  // Does nothing if |type| is not being overridden.
  void RemoveSensorOverride(mojom::SensorType type);

  bool IsOverridingSensor(mojom::SensorType type) const;

  // PlatformSensorProvider overrides.
  void CreateSensorInternal(mojom::SensorType type,
                            SensorReadingSharedBuffer* reading_buffer,
                            CreateSensorCallback callback) override;

 private:
  base::flat_map<mojom::SensorType, mojom::VirtualSensorMetadataPtr>
      type_metadata_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_VIRTUAL_PLATFORM_SENSOR_PROVIDER_H_
