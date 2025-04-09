// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/system_trust_store.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"

#include <memory>

#include "net/cert/internal/trust_store_chrome.h"

#if BUILDFLAG(IS_STARBOARD)

namespace net {

namespace {

// This is a system trust store stub that contains only Chrome root store.
// Default Linux implementation delegates System trust store to NSS through a
// runtime-loaded dynamic library, Starboard cannot support this.
//
// Note: Used only for Linux/Starboard builds of Cobalt, as Android can leverage
// the default Android implementation.
class SystemTrustStoreChromeOnly : public SystemTrustStore {
 public:
  explicit SystemTrustStoreChromeOnly(std::unique_ptr<TrustStoreChrome> trust_store_chrome)
      : trust_store_chrome_(std::move(trust_store_chrome)) {}

  TrustStore* GetTrustStore() override { return trust_store_chrome_.get(); }
  bool UsesSystemTrustStore() const override { return false; }
  bool IsKnownRoot(const ParsedCertificate* trust_anchor) const override {
    return trust_store_chrome_->Contains(trust_anchor);
  }
  int64_t chrome_root_store_version() override {
    return trust_store_chrome_->version();
  }
 private:
  std::unique_ptr<TrustStoreChrome> trust_store_chrome_;
};
} // namespace

std::unique_ptr<SystemTrustStore> CreateSslSystemTrustStoreChromeRoot(
    std::unique_ptr<TrustStoreChrome> chrome_root) {
  return std::make_unique<SystemTrustStoreChromeOnly>(std::move(chrome_root));
}

} // namespace net

#endif // BUILDFLAG(IS_STARBOARD)
