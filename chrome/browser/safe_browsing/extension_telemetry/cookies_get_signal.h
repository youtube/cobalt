// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_SIGNAL_H_

#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"

namespace safe_browsing {

// A signal that is created when an extension invokes cookies.get API.
class CookiesGetSignal : public ExtensionSignal {
 public:
  CookiesGetSignal(const extensions::ExtensionId& extension_id,
                   const std::string& name,
                   const std::string& store_id,
                   const std::string& url);
  ~CookiesGetSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  // Creates a unique id, which can be used to compare argument sets and as a
  // key for storage (e.g., in a map).
  std::string getUniqueArgSetId() const;

  const std::string& name() const { return name_; }
  const std::string& store_id() const { return store_id_; }
  const std::string& url() const { return url_; }

 protected:
  // Filters the cookies by name.
  std::string name_;
  // The cookie store to retrieve cookies from. If omitted, the current
  // execution context's cookie store will be used.
  std::string store_id_;
  // Restricts the retrieved cookies to those that would match the given URL.
  std::string url_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_SIGNAL_H_
