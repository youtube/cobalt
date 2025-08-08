// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_HELPER_H_
#define PRINTING_BACKEND_CUPS_HELPER_H_

#include <cups/cups.h>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"

class GURL;

// These are helper functions for dealing with CUPS.
namespace printing {

struct PrinterSemanticCapsAndDefaults;

// Time willing to wait for individual CUPS calls to complete, such as
// establishing a new connection or enumerating list of printers.
constexpr int kCupsTimeoutMs = 3000;

// Exclude fax and scanner devices when enumerating printers.
// Also exclude discovered printers that have not been added locally.
// On macOS, AirPrint destinations show up even if they're not added to
// the system, and their capabilities cannot be read in that situation.
// (crbug.com/1027834)
constexpr cups_ptype_t kDestinationsFilterMask =
    CUPS_PRINTER_FAX | CUPS_PRINTER_SCANNER | CUPS_PRINTER_DISCOVERED;

// Helper wrapper around http_t structure, with connection and cleanup
// functionality.
class COMPONENT_EXPORT(PRINT_BACKEND) HttpConnectionCUPS {
 public:
  HttpConnectionCUPS(const GURL& print_server_url,
                     http_encryption_t encryption,
                     bool blocking);
  ~HttpConnectionCUPS();

  http_t* http();

 private:
  raw_ptr<http_t> http_;
};

// Helper function to parse and convert PPD capabilitites to
// semantic options.
COMPONENT_EXPORT(PRINT_BACKEND)
bool ParsePpdCapabilities(cups_dest_t* dest,
                          base::StringPiece locale,
                          base::StringPiece printer_capabilities,
                          PrinterSemanticCapsAndDefaults* printer_info);

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_HELPER_H_
