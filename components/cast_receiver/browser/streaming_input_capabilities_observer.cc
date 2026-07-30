// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_input_capabilities_observer.h"

#include "base/strings/string_number_conversions.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace cast_receiver {

namespace {

cast_receiver::InputDevice ConvertInputDeviceCommon(
    const ui::InputDevice& device,
    cast_receiver::InputType type) {
  cast_receiver::InputDevice proto;
  proto.set_device_id(base::NumberToString(device.id));
  proto.set_type(type);
  proto.set_display_name(device.name);

  proto.set_vendor_id(device.vendor_id);
  proto.set_product_id(device.product_id);

  return proto;
}

cast_receiver::InputDevice ConvertKeyboardDevice(
    const ui::KeyboardDevice& device) {
  cast_receiver::InputDevice proto =
      ConvertInputDeviceCommon(device, cast_receiver::INPUT_TYPE_KEYBOARD);
  auto* keyboard_metadata = proto.mutable_keyboard_metadata();
  keyboard_metadata->set_is_virtual(device.type == ui::INPUT_DEVICE_UNKNOWN);
  return proto;
}

cast_receiver::InputDevice ConvertMouseDevice(const ui::InputDevice& device) {
  return ConvertInputDeviceCommon(device, cast_receiver::INPUT_TYPE_MOUSE);
}

cast_receiver::InputDevice ConvertTouchscreenDevice(
    const ui::TouchscreenDevice& device) {
  cast_receiver::InputDevice proto =
      ConvertInputDeviceCommon(device, cast_receiver::INPUT_TYPE_TOUCH);
  auto* touch_metadata = proto.mutable_touch_metadata();
  touch_metadata->set_max_touch_points(device.touch_points);
  return proto;
}

}  // namespace

StreamingInputCapabilitiesObserver::StreamingInputCapabilitiesObserver(
    ui::DeviceDataManager* device_data_manager,
    InputCapabilitiesCallback callback)
    : device_data_manager_(device_data_manager),
      callback_(std::move(callback)) {
  CHECK(device_data_manager_);
  device_data_manager_->AddObserver(this);
  if (device_data_manager_->AreDeviceListsComplete()) {
    UpdateCapabilities();
  }
}

StreamingInputCapabilitiesObserver::~StreamingInputCapabilitiesObserver() {
  device_data_manager_->RemoveObserver(this);
}

void StreamingInputCapabilitiesObserver::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & (kKeyboard | kMouse | kTouchscreen)) {
    UpdateCapabilities();
  }
}

void StreamingInputCapabilitiesObserver::OnDeviceListsComplete() {
  UpdateCapabilities();
}

void StreamingInputCapabilitiesObserver::UpdateCapabilities() {
  cast_receiver::InputCapabilities capabilities = BuildCapabilities();
  std::string serialized;
  if (capabilities.SerializeToString(&serialized)) {
    if (!last_serialized_capabilities_ ||
        serialized != *last_serialized_capabilities_) {
      last_serialized_capabilities_ = std::move(serialized);
      callback_.Run(std::move(capabilities));
    }
  }
}

cast_receiver::InputCapabilities
StreamingInputCapabilitiesObserver::BuildCapabilities() const {
  cast_receiver::InputCapabilities capabilities;

  for (const ui::KeyboardDevice& device :
       device_data_manager_->GetKeyboardDevices()) {
    *capabilities.add_devices() = ConvertKeyboardDevice(device);
  }

  for (const ui::InputDevice& device :
       device_data_manager_->GetMouseDevices()) {
    *capabilities.add_devices() = ConvertMouseDevice(device);
  }

  for (const ui::TouchscreenDevice& device :
       device_data_manager_->GetTouchscreenDevices()) {
    *capabilities.add_devices() = ConvertTouchscreenDevice(device);
  }

  return capabilities;
}

}  // namespace cast_receiver
