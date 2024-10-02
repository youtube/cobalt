// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/usb_printer_util.h"

#include <ctype.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/md5.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/usb_printer_id.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

using device::mojom::UsbDeviceInfo;

// Base class used for printer USB interfaces
// (https://www.usb.org/developers/defined_class).
constexpr uint8_t kPrinterInterfaceClass = 7;

// Subclass used for printers
// (http://www.usb.org/developers/docs/devclass_docs/usbprint11a021811.pdf).
constexpr uint8_t kPrinterInterfaceSubclass = 1;

// Protocol for ippusb printing.
// (http://www.usb.org/developers/docs/devclass_docs/IPP.zip).
constexpr uint8_t kPrinterIppusbProtocol = 4;

// Configuration for a GET_DEVICE_ID Printer Class-Specific Request.
const int kGetDeviceIdRequest = 0;
const int kDefaultInterface = 0;
const int kDefaultConfiguration = 0;

// Callback for device.mojom.UsbDevice.ControlTransferIn.
// Expects |data| to hold a newly queried Device ID.
void OnControlTransfer(mojo::Remote<device::mojom::UsbDevice> device,
                       GetDeviceIdCallback cb,
                       device::mojom::UsbTransferStatus status,
                       base::span<const uint8_t> data) {
  if (status != device::mojom::UsbTransferStatus::COMPLETED || data.empty()) {
    return std::move(cb).Run({});
  }

  // Cleanup device.
  device->ReleaseInterface(kDefaultInterface, base::DoNothing());
  device->Close(base::DoNothing());

  return std::move(cb).Run(chromeos::UsbPrinterId(data));
}

// Callback for device.mojom.UsbDevice.ClaimInterface.
// If interface was claimed successfully, attempts to query printer for a
// Device ID.
void OnClaimInterface(mojo::Remote<device::mojom::UsbDevice> device,
                      GetDeviceIdCallback cb,
                      device::mojom::UsbClaimInterfaceResult result) {
  if (result != device::mojom::UsbClaimInterfaceResult::kSuccess) {
    return std::move(cb).Run({});
  }

  auto params = device::mojom::UsbControlTransferParams::New();
  params->type = device::mojom::UsbControlTransferType::CLASS;
  params->recipient = device::mojom::UsbControlTransferRecipient::INTERFACE;
  params->request = kGetDeviceIdRequest;
  params->value = kDefaultConfiguration;  // default config index
  params->index = kDefaultInterface;      // default interface index

  // Query for IEEE1284 string.
  auto* device_raw = device.get();
  device_raw->ControlTransferIn(
      std::move(params), 255 /* max size */, 2000 /* 2 second timeout */,
      base::BindOnce(OnControlTransfer, std::move(device), std::move(cb)));
}

// Callback for device.mojom.UsbDevice.Open.
// If device was opened successfully, attempts to claim printer's default
// interface.
void OnDeviceOpen(mojo::Remote<device::mojom::UsbDevice> device,
                  GetDeviceIdCallback cb,
                  device::mojom::UsbOpenDeviceResultPtr result) {
  if (result->is_error() || !device) {
    return std::move(cb).Run({});
  }

  // Claim interface.
  auto* device_raw = device.get();
  device_raw->ClaimInterface(
      kDefaultInterface,
      base::BindOnce(OnClaimInterface, std::move(device), std::move(cb)));
}

// Incorporate the bytes of |val| into the incremental hash carried in |ctx| in
// big-endian order.  |val| must be a simple integer type
template <typename T>
void MD5UpdateBigEndian(base::MD5Context* ctx, T val) {
  static_assert(std::is_integral<T>::value, "Value must be an integer");
  char buf[sizeof(T)];
  base::WriteBigEndian(buf, val);
  base::MD5Update(ctx, base::StringPiece(buf, sizeof(T)));
}

// Update the hash with the contents of |str|.
//
// UTF-16 strings are a bit fraught for consistency in memory representation;
// endianness is an issue, but more importantly, there are *optional* prefix
// codepoints to identify the endianness of the string.
//
// This is a long way to say "UTF-16 is hard to hash, let's just convert
// to UTF-8 and hash that", which avoids all of these issues.
void MD5UpdateString16(base::MD5Context* ctx, const std::u16string& str) {
  std::string tmp = base::UTF16ToUTF8(str);
  base::MD5Update(ctx, base::StringPiece(tmp.data(), tmp.size()));
}

// Get the usb printer id for |device|.  This is used both as the identifier for
// the printer in the user's PrintersManager and as the name of the printer in
// CUPS, so it has to satisfy the naming restrictions of both.  CUPS in
// particular is intolerant of much more than [a-z0-9_-], so we use that
// character set.  This needs to be stable for a given device, but as unique as
// possible for that device.  So we basically toss every bit of stable
// information from the device into an MD5 hash, and then hexify the hash value
// as a suffix to "usb-" as the final printer id.
std::string CreateUsbPrinterId(const UsbDeviceInfo& device_info) {
  // Paranoid checks; in the unlikely event someone messes with the USB device
  // definition, our (supposedly stable) hashes will change.
  static_assert(sizeof(device_info.class_code) == 1, "Class size changed");
  static_assert(sizeof(device_info.subclass_code) == 1,
                "Subclass size changed");
  static_assert(sizeof(device_info.protocol_code) == 1,
                "Protocol size changed");
  static_assert(sizeof(device_info.vendor_id) == 2, "Vendor id size changed");
  static_assert(sizeof(device_info.product_id) == 2, "Product id size changed");
  static_assert(sizeof(device::GetDeviceVersion(device_info)) == 2,
                "Version size changed");

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  MD5UpdateBigEndian(&ctx, device_info.class_code);
  MD5UpdateBigEndian(&ctx, device_info.subclass_code);
  MD5UpdateBigEndian(&ctx, device_info.protocol_code);
  MD5UpdateBigEndian(&ctx, device_info.vendor_id);
  MD5UpdateBigEndian(&ctx, device_info.product_id);
  MD5UpdateBigEndian(&ctx, device::GetDeviceVersion(device_info));
  MD5UpdateString16(&ctx, GetManufacturerName(device_info));
  MD5UpdateString16(&ctx, GetProductName(device_info));
  MD5UpdateString16(&ctx, GetSerialNumber(device_info));
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return base::StringPrintf("usb-%s", base::MD5DigestToBase16(digest).c_str());
}

// Creates a mojom filter which can be used to identify a basic USB printer.
mojo::StructPtr<device::mojom::UsbDeviceFilter> CreatePrinterFilter() {
  auto printer_filter = device::mojom::UsbDeviceFilter::New();
  printer_filter->has_class_code = true;
  printer_filter->class_code = kPrinterInterfaceClass;
  printer_filter->has_subclass_code = true;
  printer_filter->subclass_code = kPrinterInterfaceSubclass;

  return printer_filter;
}

bool UsbDeviceSupportsIppusb(const UsbDeviceInfo& device_info) {
  auto printer_filter = CreatePrinterFilter();
  printer_filter->has_protocol_code = true;
  printer_filter->protocol_code = kPrinterIppusbProtocol;

  return device::UsbDeviceFilterMatches(*printer_filter, device_info);
}

// Convert the interesting details of a device to a string, for
// logging/debugging.
std::string UsbPrinterDeviceDetailsAsString(const UsbDeviceInfo& device_info) {
  return base::StringPrintf(
      " guid:                %s\n"
      " usb version:         %d\n"
      " device class:        %d\n"
      " device subclass:     %d\n"
      " device protocol:     %d\n"
      " vendor id:           %04x\n"
      " product id:          %04x\n"
      " device version:      %d\n"
      " manufacturer string: %s\n"
      " product string:      %s\n"
      " serial number:       %s",
      device_info.guid.c_str(), device::GetUsbVersion(device_info),
      device_info.class_code, device_info.subclass_code,
      device_info.protocol_code, device_info.vendor_id, device_info.product_id,
      device::GetDeviceVersion(device_info),
      base::UTF16ToUTF8(GetManufacturerName(device_info)).c_str(),
      base::UTF16ToUTF8(GetProductName(device_info)).c_str(),
      base::UTF16ToUTF8(GetSerialNumber(device_info)).c_str());
}

// Gets the URI CUPS would use to refer to this USB device.  Assumes device
// is a printer.
chromeos::Uri UsbPrinterUri(const UsbDeviceInfo& device_info) {
  // Note that serial may, for some devices, be empty or bogus (all zeros, non
  // unique, or otherwise not really a serial number), but having a non-unique
  // or empty serial field in the URI still lets us print, it just means we
  // don't have a way to uniquely identify a printer if there are multiple ones
  // plugged in with the same VID/PID, so we may print to the *wrong* printer.
  // There doesn't seem to be a robust solution to this problem; if printers
  // don't supply a serial number, we don't have any reliable way to do that
  // differentiation.
  std::string serial = base::UTF16ToUTF8(GetSerialNumber(device_info));
  chromeos::Uri uri;
  uri.SetScheme("usb");
  uri.SetHost(base::StringPrintf("%04x", device_info.vendor_id));
  uri.SetPath({base::StringPrintf("%04x", device_info.product_id)});
  uri.SetQuery({{"serial", serial}});
  return uri;
}

}  // namespace

std::u16string GetManufacturerName(const UsbDeviceInfo& device_info) {
  return device_info.manufacturer_name.value_or(std::u16string());
}

std::u16string GetProductName(const UsbDeviceInfo& device_info) {
  return device_info.product_name.value_or(std::u16string());
}

std::u16string GetSerialNumber(const UsbDeviceInfo& device_info) {
  return device_info.serial_number.value_or(std::u16string());
}

bool UsbDeviceIsPrinter(const UsbDeviceInfo& device_info) {
  auto printer_filter = CreatePrinterFilter();
  return device::UsbDeviceFilterMatches(*printer_filter, device_info);
}

// Attempt to gather all the information we need to work with this printer by
// querying the USB device.  This should only be called using devices we believe
// are printers, not arbitrary USB devices, as we may get weird partial results
// from arbitrary devices. The results are saved in the second parameter.
bool UsbDeviceToPrinter(const UsbDeviceInfo& device_info,
                        PrinterDetector::DetectedPrinter* entry) {
  DCHECK(entry);

  // Preflight all required fields and log errors if we find something wrong.
  if (device_info.vendor_id == 0 || device_info.product_id == 0) {
    LOG(ERROR) << "Failed to convert USB device to printer.  Fields were:\n"
               << UsbPrinterDeviceDetailsAsString(device_info);
    return false;
  }

  entry->ppd_search_data.usb_manufacturer =
      base::UTF16ToUTF8(GetManufacturerName(device_info));
  entry->ppd_search_data.usb_model =
      base::UTF16ToUTF8(GetProductName(device_info));

  const std::string& make = entry->ppd_search_data.usb_manufacturer;
  const std::string& model = entry->ppd_search_data.usb_model;

  // Synthesize make-and-model string for printer identification.
  entry->printer.set_make_and_model(base::JoinString({make, model}, " "));

  // Construct the display name by however much of the manufacturer/model
  // information that we have available.
  if (make.empty() && model.empty()) {
    entry->printer.set_display_name(
        l10n_util::GetStringUTF8(IDS_USB_PRINTER_UNKNOWN_DISPLAY_NAME));
  } else if (!make.empty() && !model.empty()) {
    entry->printer.set_display_name(l10n_util::GetStringFUTF8(
        IDS_USB_PRINTER_DISPLAY_NAME, base::UTF8ToUTF16(make),
        base::UTF8ToUTF16(model)));
  } else {
    // Exactly one string is present.
    DCHECK_NE(make.empty(), model.empty());
    entry->printer.set_display_name(
        l10n_util::GetStringFUTF8(IDS_USB_PRINTER_DISPLAY_NAME_MAKE_OR_MODEL,
                                  base::UTF8ToUTF16(make + model)));
  }

  entry->printer.set_description(entry->printer.display_name());
  entry->printer.SetUri(UsbPrinterUri(device_info));
  entry->printer.set_id(CreateUsbPrinterId(device_info));
  entry->printer.set_supports_ippusb(UsbDeviceSupportsIppusb(device_info));
  return true;
}

void GetDeviceId(mojo::Remote<device::mojom::UsbDevice> device,
                 GetDeviceIdCallback cb) {
  // Open device.
  auto* device_raw = device.get();
  device_raw->Open(
      base::BindOnce(OnDeviceOpen, std::move(device), std::move(cb)));
}

}  // namespace ash
