// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/log_entry.h"

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/uri.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {

// Builds a single log entry for device-log.
std::string LogEntry(base::StringPiece message,
                     base::StringPiece method,
                     const GURL& auth_server,
                     absl::optional<StatusCode> status,
                     const chromeos::Uri& ipp_endpoint) {
  std::vector<base::StringPiece> strv;
  strv.reserve(10);
  strv.emplace_back("oauth ");
  strv.emplace_back(method);
  if (!auth_server.is_empty()) {
    strv.emplace_back(";server=");
    strv.emplace_back(auth_server.possibly_invalid_spec());
  }
  const std::string endpoint = ipp_endpoint.GetNormalized();
  if (!endpoint.empty()) {
    strv.emplace_back(";endpoint=");
    strv.emplace_back(endpoint);
  }
  if (status) {
    strv.emplace_back(";status=");
    strv.emplace_back(ToStringPiece(*status));
  }
  if (!message.empty()) {
    strv.emplace_back(": ");
    strv.emplace_back(message);
  }
  return base::StrCat(strv);
}

}  // namespace ash::printing::oauth2
