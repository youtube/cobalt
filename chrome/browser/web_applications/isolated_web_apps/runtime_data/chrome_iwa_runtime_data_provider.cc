// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"

#include "base/types/pass_key.h"

namespace web_app {

// static
ChromeIwaRuntimeDataProvider& ChromeIwaRuntimeDataProvider::GetInstance() {
  return static_cast<ChromeIwaRuntimeDataProvider&>(
      IwaRuntimeDataProvider::GetInstance());
}

// static
void ChromeIwaRuntimeDataProvider::SetInstance(
    base::PassKey<BrowserProcessImpl, TestingBrowserProcess> pass_key,
    ChromeIwaRuntimeDataProvider* instance) {
  IwaRuntimeDataProvider::SetInstance(pass_key, instance);
}

}  // namespace web_app
