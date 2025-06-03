// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"

namespace device {

SensorReadingSharedBuffer::SensorReadingSharedBuffer() = default;
SensorReadingSharedBuffer::~SensorReadingSharedBuffer() = default;

// static
uint64_t SensorReadingSharedBuffer::GetOffset(mojom::SensorType type) {
  return static_cast<uint64_t>(type) * sizeof(SensorReadingSharedBuffer);
}

}  // namespace device
