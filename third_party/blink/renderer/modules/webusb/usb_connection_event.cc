// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb_connection_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_connection_event_init.h"
#include "third_party/blink/renderer/modules/webusb/usb_device.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"

namespace blink {

USBConnectionEvent* USBConnectionEvent::Create(
    const AtomicString& type,
    const USBConnectionEventInit* initializer,
    const DOMWrapperWorld* world) {
  return MakeGarbageCollected<USBConnectionEvent>(type, initializer, world);
}

USBConnectionEvent* USBConnectionEvent::Create(const AtomicString& type,
                                               USBDevice* device,
                                               const DOMWrapperWorld* world) {
  return MakeGarbageCollected<USBConnectionEvent>(type, device, world);
}

USBConnectionEvent::USBConnectionEvent(
    const AtomicString& type,
    const USBConnectionEventInit* initializer,
    const DOMWrapperWorld* world)
    : Event(type, initializer), device_(initializer->device()), world_(world) {}

USBConnectionEvent::USBConnectionEvent(const AtomicString& type,
                                       USBDevice* device,
                                       const DOMWrapperWorld* world)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      device_(device),
      world_(world) {}

bool USBConnectionEvent::CanBeDispatchedInWorld(
    const DOMWrapperWorld& world) const {
  // When the feature is disabled, `world_` is null and the same event (sharing
  // the same USBDevice C++ instance) is dispatched to the `USB` interface in
  // all worlds.
  // When enabled, separate events with isolated USBDevice instances are
  // created and restricted to the `USB` interface in their target worlds.
  return !world_ || &world == world_.Get();
}

void USBConnectionEvent::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(world_);
  Event::Trace(visitor);
}

}  // namespace blink
