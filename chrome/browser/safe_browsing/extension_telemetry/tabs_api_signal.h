// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_API_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_API_SIGNAL_H_

#include <string>

#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

using TabsApiInfo = ExtensionTelemetryReportRequest::SignalInfo::TabsApiInfo;

// A signal that is created when an extension invokes chrome.tabs API methods.
class TabsApiSignal : public ExtensionSignal {
 public:
  TabsApiSignal(const extensions::ExtensionId& extension_id,
                TabsApiInfo::ApiMethod api_method,
                const std::string& current_url,
                const std::string& new_url);
  ~TabsApiSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  // Returns a unique id, which can be used to compare API arguments and as a
  // key for storage (e.g., in a map).
  std::string GetUniqueCallDetailsId() const;

  TabsApiInfo::ApiMethod api_method() const { return api_method_; }
  const std::string& current_url() const { return current_url_; }
  const std::string& new_url() const { return new_url_; }

 protected:
  TabsApiInfo::ApiMethod api_method_;
  std::string current_url_;
  std::string new_url_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_EXECUTE_SCRIPT_SIGNAL_H_
