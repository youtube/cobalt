// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ACCELEROMETER_MAC_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ACCELEROMETER_MAC_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "services/device/generic_sensor/platform_sensor.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

namespace device {

// Implementation of PlatformSensor for macOS to query the accelerometer
// sensor.
// This is a single instance object per browser process which is created by
// PlatformSensorProviderMac. If there are no clients, this instance is not
// created.
class PlatformSensorAccelerometerMac : public PlatformSensor {
 public:
  // Construct a platform sensor of type ACCELEROMETER, given a buffer |mapping|
  // where readings will be written.
  PlatformSensorAccelerometerMac(SensorReadingSharedBuffer* reading_buffer,
                                 PlatformSensorProvider* provider);

  PlatformSensorAccelerometerMac(const PlatformSensorAccelerometerMac&) =
      delete;
  PlatformSensorAccelerometerMac& operator=(
      const PlatformSensorAccelerometerMac&) = delete;

  mojom::ReportingMode GetReportingMode() override;
  // Can only be called once, the first time or after a StopSensor call.
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;

 protected:
  ~PlatformSensorAccelerometerMac() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;
  PlatformSensorConfiguration GetDefaultConfiguration() override;

 private:
  class BlockingTaskRunnerHelper;

  void OnReadingAvailable(SensorReading reading);

  base::SequenceBound<BlockingTaskRunnerHelper> blocking_task_helper_;

  SensorReading reading_;

  bool is_reading_active_ = false;

  base::WeakPtrFactory<PlatformSensorAccelerometerMac> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_ACCELEROMETER_MAC_H_
