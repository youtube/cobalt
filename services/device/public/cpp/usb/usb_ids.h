// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_USB_USB_IDS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_USB_USB_IDS_H_

#include <stdint.h>

namespace device {

// The vendor and product names associated with a USB vendor/product ID pair.
// A field is nullptr when the corresponding name is not present in the
// database. The strings are owned by the static lookup table and remain valid
// for the lifetime of the process.
struct UsbIdNames {
  const char* vendor_name = nullptr;
  const char* product_name = nullptr;
};

// UsbIds provides a static mapping from a vendor ID to a name, as well as a
// mapping from a vendor/product ID pair to a product name.
//
// The lookup table is shipped as a Chrome resource (IDR_USB_IDS_DATA) and is
// loaded lazily on first use via ui::ResourceBundle. The first call therefore
// must happen on a thread that is permitted to do blocking resource loads
// (i.e. after the browser's resource bundle has been initialized).
class UsbIds {
 public:
  UsbIds() = delete;
  UsbIds(const UsbIds&) = delete;
  UsbIds& operator=(const UsbIds&) = delete;
  ~UsbIds() = delete;

  // Gets the name of the vendor who owns |vendor_id|. Returns nullptr if the
  // specified |vendor_id| does not exist.
  static const char* GetVendorName(uint16_t vendor_id);

  // Looks up the vendor name for |vendor_id| and the product name for the
  // |vendor_id|/|product_id| pair in a single lookup. A field of the returned
  // struct is nullptr when that name does not exist; in particular both fields
  // are nullptr when |vendor_id| is unknown, and product_name is nullptr when
  // the vendor is known but the product is not.
  static UsbIdNames GetVendorAndProductName(uint16_t vendor_id,
                                            uint16_t product_id);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_USB_USB_IDS_H_
