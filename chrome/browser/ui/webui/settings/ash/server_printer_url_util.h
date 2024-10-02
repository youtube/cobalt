// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SERVER_PRINTER_URL_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SERVER_PRINTER_URL_UTIL_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace ash::settings {

// Returns true if |gurl| has any the following scheme: HTTP, HTTPS, IPP, or
// IPPS. Returns false for an empty or any other scheme.
bool HasValidServerPrinterScheme(const GURL& gurl);

// Returns a GURL from the input |url|. Returns absl::nullopt if
// either |url| is invalid or constructing the GURL failed. This will also
// default the server printer URI to use HTTPS if it detects a missing scheme.
absl::optional<GURL> GenerateServerPrinterUrlWithValidScheme(
    const std::string& url);

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SERVER_PRINTER_URL_UTIL_H_
