// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PRIVACY_SANDBOX_TRANSFORMER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PRIVACY_SANDBOX_TRANSFORMER_H_

#include <optional>
#include <string>

#include "chrome/browser/extensions/pref_transformer_interface.h"

namespace base {
class Value;
}  // namespace base

namespace extensions {

// Transforms the stored extension pref for enabling the Privacy Sandbox API
// to the browser enablement state.
class PrivacySandboxTransformer : public PrefTransformerInterface {
 public:
  PrivacySandboxTransformer();
  ~PrivacySandboxTransformer() override;

  // PrefTransformerInterface
  std::optional<base::Value> ExtensionToBrowserPref(
      const base::Value& extension_pref,
      std::string& error,
      bool& bad_message) override;
  std::optional<base::Value> BrowserToExtensionPref(
      const base::Value& browser_pref,
      bool is_incognito_profile) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PRIVACY_SANDBOX_TRANSFORMER_H_
