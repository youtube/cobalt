// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printer_setup_util.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/crash/core/common/crash_keys.h"
#include "content/public/browser/browser_thread.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#endif

namespace ash {
namespace printing {

namespace {

void LogPrinterSetup(const chromeos::Printer& printer,
                     PrinterSetupResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::UmaHistogramEnumeration(
      printer.IsZeroconf()
          ? "Printing.CUPS.ZeroconfPrinterSetupResult.PrintPreview"
          : "Printing.CUPS.PrinterSetupResult.PrintPreview",
      result, PrinterSetupResult::kMaxValue);

  switch (result) {
    case PrinterSetupResult::kSuccess: {
      VLOG(1) << "Printer setup successful for " << printer.id()
              << " fetching properties";
      if (printer.IsUsbProtocol()) {
        // Record UMA for USB printer setup source.
        PrinterConfigurer::RecordUsbPrinterSetupSource(
            UsbPrinterSetupSource::kPrintPreview);
      }
      return;
    }
    case PrinterSetupResult::kPrinterUnreachable:
    case PrinterSetupResult::kPrinterSentWrongResponse:
    case PrinterSetupResult::kPpdNotFound:
    case PrinterSetupResult::kPpdUnretrievable:
      // Prompt user to update configuration or check internet connection.
      // TODO(skau): Fill me in
      LOG(WARNING) << ResultCodeToMessage(result);
      break;
    case PrinterSetupResult::kFatalError:
    case PrinterSetupResult::kDbusError:
    case PrinterSetupResult::kNativePrintersNotAllowed:
    case PrinterSetupResult::kPpdTooLarge:
    case PrinterSetupResult::kInvalidPpd:
    case PrinterSetupResult::kIoError:
    case PrinterSetupResult::kMemoryAllocationError:
    case PrinterSetupResult::kBadUri:
    case PrinterSetupResult::kDbusNoReply:
    case PrinterSetupResult::kDbusTimeout:
    case PrinterSetupResult::kManualSetupRequired:
      LOG(ERROR) << ResultCodeToMessage(result);
      break;
    case PrinterSetupResult::kInvalidPrinterUpdate:
    case PrinterSetupResult::kEditSuccess:
    case PrinterSetupResult::kPrinterIsNotAutoconfigurable:
    case PrinterSetupResult::kComponentUnavailable:
    case PrinterSetupResult::kMaxValue:
      LOG(ERROR) << "Unexpected error in printer setup: "
                 << ResultCodeToMessage(result);
      break;
  }
}

// This runs on a ThreadPoolForegroundWorker and not the UI thread.
absl::optional<::printing::PrinterSemanticCapsAndDefaults>
FetchCapabilitiesOnBlockingTaskRunner(const std::string& printer_id,
                                      const std::string& locale) {
  auto print_backend = ::printing::PrintBackend::CreateInstance(locale);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  VLOG(1) << "Get printer capabilities start for " << printer_id;
  crash_keys::ScopedPrinterInfo crash_key(
      print_backend->GetPrinterDriverInfo(printer_id));

  auto caps = absl::make_optional<::printing::PrinterSemanticCapsAndDefaults>();
  if (print_backend->GetPrinterSemanticCapsAndDefaults(printer_id, &*caps) !=
      ::printing::mojom::ResultCode::kSuccess) {
    // Failed to get capabilities, but proceed to assemble the settings to
    // return what information we do have.
    LOG(WARNING) << "Failed to get capabilities for " << printer_id;
    return absl::nullopt;
  }
  return caps;
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)
void CapabilitiesFetchedFromService(
    const std::string& printer_id,
    bool elevated_privileges,
    GetPrinterCapabilitiesCallback cb,
    ::printing::mojom::PrinterSemanticCapsAndDefaultsResultPtr printer_caps) {
  if (printer_caps->is_result_code()) {
    LOG(WARNING) << "Failure fetching printer capabilities from service for "
                 << printer_id << " - error "
                 << printer_caps->get_result_code();

    // If we failed because of access denied then we could retry at an elevated
    // privilege (if not already elevated).
    if (printer_caps->get_result_code() ==
            ::printing::mojom::ResultCode::kAccessDenied &&
        !elevated_privileges) {
      // Register that this printer requires elevated privileges.
      ::printing::PrintBackendServiceManager& service_mgr =
          ::printing::PrintBackendServiceManager::GetInstance();
      service_mgr.SetPrinterDriverFoundToRequireElevatedPrivilege(printer_id);

      // Retry the operation which should now happen at a higher privilege
      // level.
      service_mgr.GetPrinterSemanticCapsAndDefaults(
          printer_id,
          base::BindOnce(&CapabilitiesFetchedFromService, printer_id,
                         /*elevated_privileges=*/true, std::move(cb)));
      return;
    }

    // Unable to fallback, call back without data.
    std::move(cb).Run(absl::nullopt);
    return;
  }

  VLOG(1) << "Successfully received printer capabilities from service for "
          << printer_id;
  std::move(cb).Run(printer_caps->get_printer_caps());
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

void FetchCapabilities(const std::string& printer_id,
                       GetPrinterCapabilitiesCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (base::FeatureList::IsEnabled(
          ::printing::features::kEnableOopPrintDrivers)) {
    VLOG(1) << "Fetching printer capabilities via service";
    ::printing::PrintBackendServiceManager& service_mgr =
        ::printing::PrintBackendServiceManager::GetInstance();
    service_mgr.GetPrinterSemanticCapsAndDefaults(
        printer_id,
        base::BindOnce(&CapabilitiesFetchedFromService, printer_id,
                       service_mgr.PrinterDriverFoundToRequireElevatedPrivilege(
                           printer_id),
                       std::move(cb)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  VLOG(1) << "Fetching printer capabilities in-process";
  // USER_VISIBLE because the result is displayed in the print preview dialog.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(FetchCapabilitiesOnBlockingTaskRunner, printer_id,
                     g_browser_process->GetApplicationLocale()),
      std::move(cb));
}

void OnPrinterInstalled(
    CupsPrintersManager* printers_manager,
    const chromeos::Printer& printer,
    base::OnceCallback<void(
        const absl::optional<::printing::PrinterSemanticCapsAndDefaults>&)> cb,
    PrinterSetupResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  LogPrinterSetup(printer, result);
  if (result != PrinterSetupResult::kSuccess) {
    std::move(cb).Run(absl::nullopt);
    return;
  }
  printers_manager->PrinterInstalled(printer, /*is_automatic=*/true);
  // Fetch settings off of the UI thread and invoke callback.
  FetchCapabilities(printer.id(), std::move(cb));
}

}  // namespace

void SetUpPrinter(CupsPrintersManager* printers_manager,
                  PrinterConfigurer* printer_configurer,
                  const chromeos::Printer& printer,
                  GetPrinterCapabilitiesCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Log printer configuration for selected printer.
  base::UmaHistogramEnumeration("Printing.CUPS.ProtocolUsed",
                                printer.GetProtocol(),
                                chromeos::Printer::kProtocolMax);

  if (printers_manager->IsPrinterInstalled(printer)) {
    // Skip setup if the printer does not need to be installed.
    // Fetch settings off of the UI thread and invoke callback.
    FetchCapabilities(printer.id(), std::move(cb));
    return;
  }

  printer_configurer->SetUpPrinter(
      printer, base::BindOnce(OnPrinterInstalled, printers_manager, printer,
                              std::move(cb)));
}

}  // namespace printing
}  // namespace ash
