// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_LINUX_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_LINUX_H_

#include "services/device/generic_sensor/platform_sensor.h"

namespace device {

class SensorReader;
struct SensorInfoLinux;

class PlatformSensorLinux : public PlatformSensor {
 public:
  PlatformSensorLinux(mojom::SensorType type,
                      SensorReadingSharedBuffer* reading_buffer,
                      PlatformSensorProvider* provider,
                      const SensorInfoLinux* sensor_device);

  PlatformSensorLinux(const PlatformSensorLinux&) = delete;
  PlatformSensorLinux& operator=(const PlatformSensorLinux&) = delete;

  mojom::ReportingMode GetReportingMode() override;

  // Called by a sensor reader. Takes new readings.
  void UpdatePlatformSensorReading(SensorReading reading);

  // Called by a sensor reader if an error occurs.
  void NotifyPlatformSensorError();

 protected:
  ~PlatformSensorLinux() override;
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;
  PlatformSensorConfiguration GetDefaultConfiguration() override;

 private:
  const PlatformSensorConfiguration default_configuration_;
  const mojom::ReportingMode reporting_mode_;

  // A sensor reader that reads values from sensor files
  // and stores them to a SensorReading structure.
  std::unique_ptr<SensorReader> sensor_reader_;

  base::WeakPtrFactory<PlatformSensorLinux> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_LINUX_H_
