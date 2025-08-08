// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_TESTING_INTERNALS_SENSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_TESTING_INTERNALS_SENSOR_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_virtual_sensor_type.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CreateVirtualSensorOptions;
class Internals;
class VirtualSensorReading;
class ScriptPromise;
class ScriptState;

class InternalsSensor {
  STATIC_ONLY(InternalsSensor);

 public:
  static ScriptPromise createVirtualSensor(ScriptState*,
                                           Internals&,
                                           V8VirtualSensorType,
                                           CreateVirtualSensorOptions*);
  static ScriptPromise updateVirtualSensor(ScriptState*,
                                           Internals&,
                                           V8VirtualSensorType,
                                           VirtualSensorReading*);
  static ScriptPromise removeVirtualSensor(ScriptState*,
                                           Internals&,
                                           V8VirtualSensorType);
  static ScriptPromise getVirtualSensorInformation(ScriptState*,
                                                   Internals&,
                                                   V8VirtualSensorType);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_TESTING_INTERNALS_SENSOR_H_
