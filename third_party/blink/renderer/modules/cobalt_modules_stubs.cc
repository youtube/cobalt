// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth.h"  // nogncheck
#include "third_party/blink/renderer/modules/webusb/usb.h"  // nogncheck
#include "third_party/blink/renderer/modules/hid/hid.h"  // nogncheck
#include "third_party/blink/renderer/modules/serial/serial.h"  // nogncheck
#include "third_party/blink/renderer/modules/ml/navigator_ml.h"  // nogncheck
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"

// Generated V8 Binding Headers
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_advertising_event.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_characteristic_properties.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_device.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_le_scan.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_manufacturer_data_map.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_remote_gatt_characteristic.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_remote_gatt_descriptor.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_remote_gatt_server.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_remote_gatt_service.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_service_data_map.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_bluetooth_uuid.h"  // nogncheck

#include "third_party/blink/renderer/bindings/modules/v8/v8_usb.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_alternate_interface.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_configuration.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_connection_event.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_device.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_endpoint.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_in_transfer_result.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_interface.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_isochronous_in_transfer_packet.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_isochronous_in_transfer_result.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_isochronous_out_transfer_packet.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_isochronous_out_transfer_result.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_out_transfer_result.h"  // nogncheck

#include "third_party/blink/renderer/bindings/modules/v8/v8_hid.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_connection_event.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_device.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_input_report_event.h"  // nogncheck

#include "third_party/blink/renderer/bindings/modules/v8/v8_serial.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_serial_port.h"  // nogncheck

#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_message.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_reader.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_reading_event.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_record.h"  // nogncheck

#include "third_party/blink/renderer/bindings/modules/v8/v8_ml.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_graph.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_graph_builder.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor.h"  // nogncheck
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context.h"  // nogncheck

namespace blink {

static void DummyInstallInterfaceTemplateFunc(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Template> interface_template) {}

static const WrapperTypeInfo g_dummy_wrapper_type_info = {
    gin::kEmbedderBlink,
    DummyInstallInterfaceTemplateFunc,
    nullptr,
    "Dummy",
    nullptr,
    v8::CppHeapPointerTag::kDefaultTag,
    v8::CppHeapPointerTag::kDefaultTag,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kIdlInterface,
    false,
};

#define STUB_V8_WRAPPER(ClassName) \
  const WrapperTypeInfo ClassName::wrapper_type_info_ = g_dummy_wrapper_type_info;

// Web Bluetooth V8 Wrappers
STUB_V8_WRAPPER(V8Bluetooth)
STUB_V8_WRAPPER(V8BluetoothAdvertisingEvent)
STUB_V8_WRAPPER(V8BluetoothCharacteristicProperties)
STUB_V8_WRAPPER(V8BluetoothDevice)
STUB_V8_WRAPPER(V8BluetoothLEScan)
STUB_V8_WRAPPER(V8BluetoothManufacturerDataMap)
STUB_V8_WRAPPER(V8BluetoothRemoteGATTCharacteristic)
STUB_V8_WRAPPER(V8BluetoothRemoteGATTDescriptor)
STUB_V8_WRAPPER(V8BluetoothRemoteGATTServer)
STUB_V8_WRAPPER(V8BluetoothRemoteGATTService)
STUB_V8_WRAPPER(V8BluetoothServiceDataMap)
STUB_V8_WRAPPER(V8BluetoothUUID)

// WebUSB V8 Wrappers
STUB_V8_WRAPPER(V8USB)
STUB_V8_WRAPPER(V8USBAlternateInterface)
STUB_V8_WRAPPER(V8USBConfiguration)
STUB_V8_WRAPPER(V8USBConnectionEvent)
STUB_V8_WRAPPER(V8USBDevice)
STUB_V8_WRAPPER(V8USBEndpoint)
STUB_V8_WRAPPER(V8USBInTransferResult)
STUB_V8_WRAPPER(V8USBInterface)
STUB_V8_WRAPPER(V8USBIsochronousInTransferPacket)
STUB_V8_WRAPPER(V8USBIsochronousInTransferResult)
STUB_V8_WRAPPER(V8USBIsochronousOutTransferPacket)
STUB_V8_WRAPPER(V8USBIsochronousOutTransferResult)
STUB_V8_WRAPPER(V8USBOutTransferResult)

// WebHID V8 Wrappers
STUB_V8_WRAPPER(V8HID)
STUB_V8_WRAPPER(V8HIDConnectionEvent)
STUB_V8_WRAPPER(V8HIDDevice)
STUB_V8_WRAPPER(V8HIDInputReportEvent)

// Web Serial V8 Wrappers
STUB_V8_WRAPPER(V8Serial)
STUB_V8_WRAPPER(V8SerialPort)

// Web NFC V8 Wrappers
STUB_V8_WRAPPER(V8NDEFMessage)
STUB_V8_WRAPPER(V8NDEFReader)
STUB_V8_WRAPPER(V8NDEFReadingEvent)
STUB_V8_WRAPPER(V8NDEFRecord)

// WebNN / ML V8 Wrappers
STUB_V8_WRAPPER(V8ML)
STUB_V8_WRAPPER(V8MLGraph)
STUB_V8_WRAPPER(V8MLGraphBuilder)
STUB_V8_WRAPPER(V8MLOperand)
STUB_V8_WRAPPER(V8MLTensor)
STUB_V8_WRAPPER(V8MLContext)

// --- Web Bluetooth, WebUSB, WebHID, and Web Serial C++ Stubs ---

const WrapperTypeInfo& Bluetooth::wrapper_type_info_ = g_dummy_wrapper_type_info;
const WrapperTypeInfo& USB::wrapper_type_info_ = g_dummy_wrapper_type_info;
const WrapperTypeInfo& HID::wrapper_type_info_ = g_dummy_wrapper_type_info;
const WrapperTypeInfo& Serial::wrapper_type_info_ = g_dummy_wrapper_type_info;

Bluetooth* Bluetooth::bluetooth(Navigator&) {
  return nullptr;
}

USB* USB::usb(NavigatorBase&) {
  return nullptr;
}

HID* HID::hid(NavigatorBase&) {
  return nullptr;
}

Serial* Serial::serial(NavigatorBase&) {
  return nullptr;
}

// --- WebNN / ML C++ Stubs ---

const char NavigatorML::kSupplementName[] = "NavigatorML";

ML* NavigatorML::ml(NavigatorBase&) {
  return nullptr;
}

}  // namespace blink
