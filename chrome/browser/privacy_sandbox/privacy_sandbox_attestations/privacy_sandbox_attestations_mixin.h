// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_MIXIN_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_MIXIN_H_

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

#include <memory>

#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"

namespace privacy_sandbox {

// Browser tests that use privacy sandbox attestations feature should inherit
// publicly from `MixinBasedInProcessBrowserTest` and initialize a
// `PrivacySandboxAttestationsMixin` member to create a scoped attestations
// instance.
class PrivacySandboxAttestationsMixin : public InProcessBrowserTestMixin {
 public:
  explicit PrivacySandboxAttestationsMixin(InProcessBrowserTestMixinHost* host);
  ~PrivacySandboxAttestationsMixin() override;

  void SetUpOnMainThread() override;

 private:
  std::unique_ptr<ScopedPrivacySandboxAttestations> scoped_attestations_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_MIXIN_H_
