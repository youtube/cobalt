// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_DISTRIBUTION_IWA_KEY_DISTRIBUTION_HISTOGRAMS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_DISTRIBUTION_IWA_KEY_DISTRIBUTION_HISTOGRAMS_H_

#include <string_view>

namespace web_app {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class KeyRotationInfoSource {
  // No key distribution component loaded.
  kNone = 0,

  // Data comes from a preloaded version of the component.
  kPreloaded = 1,

  // Data comes from a downloaded up-to-date version of the component.
  kDownloaded = 2,

  kMaxValue = kDownloaded,
};

inline constexpr std::string_view kIwaKeyRotationInfoSource =
    "WebApp.Isolated.KeyDistributionComponent.KeyRotationInfoSource";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IwaComponentUpdateError {
  kStaleVersion = 0,
  kFileNotFound = 1,
  kProtoParsingFailure = 2,
  kMalformedBase64Key = 3,
  kMaxValue = kMalformedBase64Key,
};

inline constexpr std::string_view kIwaKeyDistributionComponentUpdateError =
    "WebApp.Isolated.KeyDistributionComponent.UpdateError";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IwaComponentUpdateSource {
  // Data comes from a preloaded version of the component.
  kPreloaded = 0,

  // Data comes from a downloaded up-to-date version of the component.
  kDownloaded = 1,

  kMaxValue = kDownloaded,
};

inline constexpr std::string_view kIwaKeyDistributionComponentUpdateSource =
    "WebApp.Isolated.KeyDistributionComponent.UpdateSource";

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_KEY_DISTRIBUTION_IWA_KEY_DISTRIBUTION_HISTOGRAMS_H_
