// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_CHROME_APPS_BROWSER_API_PROVIDER_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_CHROME_APPS_BROWSER_API_PROVIDER_H_

#include "extensions/browser/extensions_browser_api_provider.h"

namespace chrome_apps {

class ChromeAppsBrowserAPIProvider
    : public extensions::ExtensionsBrowserAPIProvider {
 public:
  ChromeAppsBrowserAPIProvider();
  ChromeAppsBrowserAPIProvider(const ChromeAppsBrowserAPIProvider&) = delete;
  ChromeAppsBrowserAPIProvider& operator=(const ChromeAppsBrowserAPIProvider&) =
      delete;
  ~ChromeAppsBrowserAPIProvider() override;

  // extensions::ExtensionsBrowserAPIProvider:
  void RegisterExtensionFunctions(ExtensionFunctionRegistry* registry) override;
};

}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_CHROME_APPS_BROWSER_API_PROVIDER_H_
