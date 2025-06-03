// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_

#include <string>

#include "content/common/content_export.h"

namespace base {
class Time;
class ValueView;
}  // namespace base

namespace content {

// Calculates the last trigger time that could have produced `report_time`.
CONTENT_EXPORT base::Time LastTriggerTimeForReportTime(base::Time report_time);

CONTENT_EXPORT std::string SerializeAttributionJson(base::ValueView body,
                                                    bool pretty_print = false);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_UTILS_H_
