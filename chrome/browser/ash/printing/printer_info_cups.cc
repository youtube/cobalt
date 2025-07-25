// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printer_info.h"

#include <array>
#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chromeos/printing/cups_printer_status.h"
#include "printing/backend/cups_jobs.h"
#include "printing/printer_status.h"

namespace {

const char kPdfMimeType[] = "application/pdf";
const char kPwgRasterMimeType[] = "image/pwg-raster";

// Wraps several printing data structures so that we can use
// PostTaskAndReplyWithResult().
struct QueryResult {
  printing::PrinterQueryResult result;
  printing::PrinterInfo printer_info;
  printing::PrinterStatus printer_status;
};

// Enums for Printing.CUPS.HighestIppVersion.  Do not delete entries.  Keep
// synced with enums.xml.  Represents IPP versions 1.0, 1.1, 2.0, 2.1, and 2.2.
// Error is used if the version was unparsable.  OutOfRange is used for values
// that are not currently mapped.
enum class IppVersion {
  kError,
  kUnknown,
  k10,
  k11,
  k20,
  k21,
  k22,
  kMaxValue = k22
};

using MajorMinor = std::pair<uint32_t, uint32_t>;
using VersionEntry = std::pair<MajorMinor, IppVersion>;

IppVersion ToIppVersion(const base::Version& version) {
  constexpr std::array<VersionEntry, 5> kVersions = {
      VersionEntry(MajorMinor((uint32_t)1, (uint32_t)0), IppVersion::k10),
      VersionEntry(MajorMinor((uint32_t)1, (uint32_t)1), IppVersion::k11),
      VersionEntry(MajorMinor((uint32_t)2, (uint32_t)0), IppVersion::k20),
      VersionEntry(MajorMinor((uint32_t)2, (uint32_t)1), IppVersion::k21),
      VersionEntry(MajorMinor((uint32_t)2, (uint32_t)2), IppVersion::k22)};

  if (!version.IsValid()) {
    return IppVersion::kError;
  }

  const auto& components = version.components();
  if (components.size() != 2) {
    return IppVersion::kError;
  }

  const auto target = MajorMinor(components[0], components[1]);
  const VersionEntry* iter =
      base::ranges::find(kVersions, target, &VersionEntry::first);

  if (iter == kVersions.end()) {
    return IppVersion::kUnknown;
  }

  return iter->second;
}

// Returns true if any of the |ipp_versions| are greater than or equal to 2.0.
bool AllowedIpp(const std::vector<base::Version>& ipp_versions) {
  return base::ranges::any_of(ipp_versions, [](const base::Version& version) {
    return version.IsValid() && version.components()[0] >= 2;
  });
}

// Returns true if |mime_type| is one of the supported types.
bool SupportedMime(const std::string& mime_type) {
  return mime_type == kPwgRasterMimeType || mime_type == kPdfMimeType;
}

// Returns true if |formats| contains one of the supported printer description
// languages for an autoconf printer identified by MIME type.
bool SupportsRequiredPDLS(const std::vector<std::string>& formats) {
  return base::ranges::any_of(formats, &SupportedMime);
}

// Returns true if |info| describes a printer for which we want to attempt
// automatic configuration.
bool IsAutoconf(const ::printing::PrinterInfo& info) {
  return info.ipp_everywhere || (AllowedIpp(info.ipp_versions) &&
                                 SupportsRequiredPDLS(info.document_formats));
}

// Dispatches an IPP request to |host| to retrieve printer information.  Returns
// a nullptr if the request fails.
QueryResult QueryPrinterImpl(const std::string& host,
                             const int port,
                             const std::string& path,
                             bool encrypted) {
  QueryResult result;
  result.result =
      ::printing::GetPrinterInfo(host, port, path, encrypted,
                                 &result.printer_info, &result.printer_status);
  if (result.result != ::printing::PrinterQueryResult::kSuccess) {
    LOG(ERROR) << "Could not retrieve printer info";
  }

  return result;
}

// Handles the request for |info|.  Parses make and model information before
// calling |callback|.
void OnPrinterQueried(ash::PrinterInfoCallback callback,
                      const QueryResult& query_result) {
  const ::printing::PrinterQueryResult& result = query_result.result;
  const ::printing::PrinterInfo& printer_info = query_result.printer_info;
  const ::printing::PrinterStatus& printer_status = query_result.printer_status;
  if (result != ::printing::PrinterQueryResult::kSuccess) {
    VLOG(1) << "Could not reach printer";
    std::move(callback).Run(result, ::printing::PrinterStatus(),
                            /*make_and_model=*/std::string(),
                            /*document_formats=*/{}, /*ipp_everywhere=*/false,
                            chromeos::PrinterAuthenticationInfo{});
    return;
  }

  DCHECK(!printer_info.ipp_versions.empty())
      << "Properly queried PrinterInfo always has at least one version";
  base::UmaHistogramEnumeration(
      "Printing.CUPS.HighestIppVersion",
      ToIppVersion(*std::max_element(printer_info.ipp_versions.begin(),
                                     printer_info.ipp_versions.end())));

  std::move(callback).Run(result, printer_status, printer_info.make_and_model,
                          printer_info.document_formats,
                          IsAutoconf(printer_info),
                          {.oauth_server = printer_info.oauth_server,
                           .oauth_scope = printer_info.oauth_scope});
}

}  // namespace

namespace ash {

void QueryIppPrinter(const std::string& host,
                     const int port,
                     const std::string& path,
                     bool encrypted,
                     PrinterInfoCallback callback) {
  DCHECK(!host.empty());

  // QueryPrinterImpl could block on a network call for a noticable amount of
  // time (100s of ms). Also the user is waiting on this result.  Thus, run at
  // USER_VISIBLE with MayBlock.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&QueryPrinterImpl, host, port, path, encrypted),
      base::BindOnce(&OnPrinterQueried, std::move(callback)));
}

}  // namespace ash
