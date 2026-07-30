// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_INTEGRITY_BLOCK_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_INTEGRITY_BLOCK_DATA_H_

#include "base/values.h"
#include "chrome/browser/web_applications/proto/web_app_isolation_data.pb.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"

namespace web_app {

// Represents the integrity block portion of `isolation_data()` for a Web App.
class IntegrityBlockData {
 public:
  explicit IntegrityBlockData(
      std::vector<web_package::SignedWebBundleSignatureInfo> signatures);
  ~IntegrityBlockData();

  IntegrityBlockData(const IntegrityBlockData&);
  IntegrityBlockData& operator=(const IntegrityBlockData&);

  bool operator==(const IntegrityBlockData& other) const;

  static IntegrityBlockData FromIntegrityBlock(
      const web_package::SignedWebBundleIntegrityBlock& integrity_block);

  static base::expected<IntegrityBlockData, std::string> FromProto(
      const proto::IsolationData::IntegrityBlockData& proto);

  proto::IsolationData::IntegrityBlockData ToProto() const;

  const std::vector<web_package::SignedWebBundleSignatureInfo>& signatures()
      const {
    return signatures_;
  }

  bool HasPublicKey(base::span<const uint8_t> public_key) const;

  base::Value AsDebugValue() const;

 private:
  std::vector<web_package::SignedWebBundleSignatureInfo> signatures_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_INTEGRITY_BLOCK_DATA_H_
