// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_CUPS_PRINTER_STATUS_REASON_ASH_H_
#define PRINTING_CUPS_PRINTER_STATUS_REASON_ASH_H_

#include "build/build_config.h"

static_assert(BUILDFLAG(IS_CHROMEOS), "Only ChromeOS is supported");

namespace printing {

// CupsPrinterStatusReason describes the state of a printer, and
// CupsPrinterStatusSeverity describes the level of seriousness of that state.
// They are used together in the CupsPrinterStatus::CupsPrinterStatusReason
// class defined in chromeos/printing/cups_printer_status.h. See
// chrome/browser/chromeos/printing/cups_printer_status_creator.cc for
// information about the UMA histogram mapping of
// printing::PrinterStatus::PrinterReason::Reason values which correspond to
// IPP printer-state-reasons (rfc2911#section-4.4.12) to the
// CupsPrinterStatusReason enum below.
// kNoError is a reserved value that is unused by the PrinterStatus object.
//
// These values are sent between C++ and JS/TS and must be kept in sync with
// PrinterStatusReason in
// chrome/browser/resources/ash/print_preview/data/printer_status_cros.ts.
enum class CupsPrinterStatusReason {
  kUnknownReason = 0,
  kDeviceError = 1,
  kDoorOpen = 2,
  kLowOnInk = 3,
  kLowOnPaper = 4,
  kNoError = 5,  // reserved
  kOutOfInk = 6,
  kOutOfPaper = 7,
  kOutputAreaAlmostFull = 8,
  kOutputFull = 9,
  kPaperJam = 10,
  kPaused = 11,
  kPrinterQueueFull = 12,
  kPrinterUnreachable = 13,
  kStopped = 14,
  kTrayMissing = 15,
  kExpiredCertificate = 16,
};

// These values are sent between C++ and JS/TS and must be kept in sync with
// PrinterStatusSeverity in
// chrome/browser/resources/ash/print_preview/data/printer_status_cros.ts.
enum class CupsPrinterStatusSeverity {
  kUnknownSeverity = 0,
  kReport = 1,
  kWarning = 2,
  kError = 3,
};

}  // namespace printing

#endif  // PRINTING_CUPS_PRINTER_STATUS_REASON_ASH_H_
