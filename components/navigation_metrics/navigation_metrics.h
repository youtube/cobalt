// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_METRICS_NAVIGATION_METRICS_H_
#define COMPONENTS_NAVIGATION_METRICS_NAVIGATION_METRICS_H_

#include <string>

#include "components/url_formatter/spoof_checks/idna_metrics.h"

class GURL;

namespace profile_metrics {
enum class BrowserProfileType;
}

namespace navigation_metrics {

// Names of the metrics logged by RecordPrimaryMainFrameNavigation() function.
extern const char kMainFrameScheme[];
extern const char kMainFrameSchemeDifferentPage[];
extern const char kMainFrameSchemeOTR[];
extern const char kMainFrameSchemeDifferentPageOTR[];
extern const char kMainFrameHasRTLDomain[];
extern const char kMainFrameHasRTLDomainDifferentPage[];
extern const char kMainFrameProfileType[];

// A Scheme is an C++ enum type loggable in UMA for a histogram of UMA enum type
// NavigationScheme.
//
// These values are written to logs. New enum values can be added, but existing
// value must never be renumbered or deleted and reused. Any new scheme must
// be added at the end, before COUNT.
enum class Scheme {
  UNKNOWN = 0,
  HTTP = 1,
  HTTPS = 2,
  FILE = 3,
  FTP = 4,
  DATA = 5,
  JAVASCRIPT = 6,
  ABOUT = 7,
  CHROME = 8,
  BLOB = 9,
  FILESYSTEM = 10,
  CHROME_NATIVE = 11,
  CHROME_SEARCH = 12,
  CHROME_DISTILLER = 13,
  CHROME_DEVTOOLS = 14,
  CHROME_EXTENSION = 15,
  VIEW_SOURCE = 16,
  EXTERNALFILE = 17,
  ISOLATED_APP = 18,
  COUNT,
};

Scheme GetScheme(const GURL& url);

void RecordPrimaryMainFrameNavigation(
    const GURL& url,
    bool is_same_document,
    bool is_off_the_record,
    profile_metrics::BrowserProfileType profile_type);

void RecordOmniboxURLNavigation(const GURL& url);

// Records metrics about deviation characters in `hostname`. `hostname` can
// be punycode or unicode and can have subdomains.
IDNA2008DeviationCharacter RecordIDNA2008Metrics(
    const std::u16string& hostname);

}  // namespace navigation_metrics

#endif  // COMPONENTS_NAVIGATION_METRICS_NAVIGATION_METRICS_H_
