// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_CONTENT_APP_SOURCE_URL_RECORDER_H_
#define COMPONENTS_UKM_CONTENT_APP_SOURCE_URL_RECORDER_H_

#include "services/metrics/public/cpp/ukm_source_id.h"

#include "base/feature_list.h"

#include <string>

class GURL;

namespace ukm {

const base::Feature kUkmAppLogging{"UkmAppLogging",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

class AppSourceUrlRecorder {
 private:
  friend class AppSourceUrlRecorderTest;

  // Get a UKM SourceId for a Chrome app.
  static SourceId GetSourceIdForChromeApp(const std::string& id);

  // Get a UKM SourceId for an Arc app.
  static SourceId GetSourceIdForArc(const std::string& package_name);

  // Get a UKM SourceId for a PWA.
  static SourceId GetSourceIdForPWA(const GURL& url);

  // For internal use only.
  static SourceId GetSourceIdForUrl(const GURL& url);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_CONTENT_APP_SOURCE_URL_RECORDER_H_
