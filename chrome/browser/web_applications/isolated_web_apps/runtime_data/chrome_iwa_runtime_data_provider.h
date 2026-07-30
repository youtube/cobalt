// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_RUNTIME_DATA_CHROME_IWA_RUNTIME_DATA_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_RUNTIME_DATA_CHROME_IWA_RUNTIME_DATA_PROVIDER_H_

#include "base/types/pass_key.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"

class BrowserProcessImpl;
class TestingBrowserProcess;

namespace web_app {

// Augments the IwaRuntimeDataProvider with chrome-specific data and policies.
// Consumers in //chrome should use this interface to access IWA runtime data.
class ChromeIwaRuntimeDataProvider : public IwaRuntimeDataProvider {
 public:
  static ChromeIwaRuntimeDataProvider& GetInstance();

  static void SetInstance(
      base::PassKey<BrowserProcessImpl, TestingBrowserProcess>,
      ChromeIwaRuntimeDataProvider* instance);

  ~ChromeIwaRuntimeDataProvider() override = default;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_RUNTIME_DATA_CHROME_IWA_RUNTIME_DATA_PROVIDER_H_
