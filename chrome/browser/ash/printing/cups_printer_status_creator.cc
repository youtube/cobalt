// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_printer_status_creator.h"

#include "chrome/browser/ash/printing/printer_info.h"
#include "chromeos/printing/cups_printer_status.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

using ::chromeos::CupsPrinterStatus;
using CupsReason = CupsPrinterStatus::CupsPrinterStatusReason::Reason;
using CupsSeverity = CupsPrinterStatus::CupsPrinterStatusReason::Severity;
using ReasonFromPrinter = printing::PrinterStatus::PrinterReason::Reason;
using SeverityFromPrinter = printing::PrinterStatus::PrinterReason::Severity;

CupsPrinterStatus PrinterStatusToCupsPrinterStatus(
    const std::string& printer_id,
    const printing::PrinterStatus& printer_status,
    const chromeos::PrinterAuthenticationInfo& auth_info) {
  CupsPrinterStatus cups_printer_status(printer_id);

  for (const auto& reason : printer_status.reasons) {
    // TODO(crbug.com/1027400): Remove log once bug is confirmed fix.
    PRINTER_LOG(DEBUG) << "Printer status received for printer " << printer_id
                       << " reason: " << static_cast<int>(reason.reason)
                       << " severity: " << static_cast<int>(reason.severity);
    cups_printer_status.AddStatusReason(
        PrinterReasonToCupsReason(reason.reason),
        PrinterSeverityToCupsSeverity(reason.severity));
  }
  cups_printer_status.SetAuthenticationInfo(auth_info);
  return cups_printer_status;
}

CupsReason PrinterReasonToCupsReason(const ReasonFromPrinter& reason) {
  switch (reason) {
    case ReasonFromPrinter::kFuserOverTemp:
    case ReasonFromPrinter::kFuserUnderTemp:
    case ReasonFromPrinter::kInterpreterResourceUnavailable:
    case ReasonFromPrinter::kOpcLifeOver:
    case ReasonFromPrinter::kOpcNearEol:
      return CupsReason::kDeviceError;
    case ReasonFromPrinter::kCoverOpen:
    case ReasonFromPrinter::kDoorOpen:
    case ReasonFromPrinter::kInterlockOpen:
      return CupsReason::kDoorOpen;
    case ReasonFromPrinter::kDeveloperLow:
    case ReasonFromPrinter::kMarkerSupplyLow:
    case ReasonFromPrinter::kMarkerWasteAlmostFull:
    case ReasonFromPrinter::kTonerLow:
      return CupsReason::kLowOnInk;
    case ReasonFromPrinter::kMediaLow:
      return CupsReason::kLowOnPaper;
    case ReasonFromPrinter::kNone:
      return CupsReason::kNoError;
    case ReasonFromPrinter::kDeveloperEmpty:
    case ReasonFromPrinter::kMarkerSupplyEmpty:
    case ReasonFromPrinter::kMarkerWasteFull:
    case ReasonFromPrinter::kTonerEmpty:
      return CupsReason::kOutOfInk;
    case ReasonFromPrinter::kMediaEmpty:
    case ReasonFromPrinter::kMediaNeeded:
      return CupsReason::kOutOfPaper;
    case ReasonFromPrinter::kOutputAreaAlmostFull:
      return CupsReason::kOutputAreaAlmostFull;
    case ReasonFromPrinter::kOutputAreaFull:
      return CupsReason::kOutputFull;
    case ReasonFromPrinter::kMediaJam:
      return CupsReason::kPaperJam;
    case ReasonFromPrinter::kMovingToPaused:
    case ReasonFromPrinter::kPaused:
      return CupsReason::kPaused;
    case ReasonFromPrinter::kSpoolAreaFull:
      return CupsReason::kPrinterQueueFull;
    case ReasonFromPrinter::kConnectingToDevice:
    case ReasonFromPrinter::kShutdown:
    case ReasonFromPrinter::kTimedOut:
      return CupsReason::kPrinterUnreachable;
    case ReasonFromPrinter::kStoppedPartly:
    case ReasonFromPrinter::kStopping:
      return CupsReason::kStopped;
    case ReasonFromPrinter::kInputTrayMissing:
    case ReasonFromPrinter::kOutputTrayMissing:
      return CupsReason::kTrayMissing;
    case ReasonFromPrinter::kUnknownReason:
      return CupsReason::kUnknownReason;
  }
}

CupsSeverity PrinterSeverityToCupsSeverity(
    const SeverityFromPrinter& severity) {
  switch (severity) {
    case SeverityFromPrinter::kUnknownSeverity:
      return CupsSeverity::kUnknownSeverity;
    case SeverityFromPrinter::kReport:
      return CupsSeverity::kReport;
    case SeverityFromPrinter::kWarning:
      return CupsSeverity::kWarning;
    case SeverityFromPrinter::kError:
      return CupsSeverity::kError;
  }
}

}  // namespace ash
