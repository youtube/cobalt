// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TRUST_AND_SIGNATURE_VERIFIER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TRUST_AND_SIGNATURE_VERIFIER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/isolated_web_apps/types/source.h"

class Profile;

namespace web_app {

// Verifies the trust and signatures of an Isolated Web App.
//
// This function validates that:
// 1. The IWA is trusted to perform the given `operation` (via
// IsolatedWebAppTrustChecker).
// 2. The Signed Web Bundle signatures are valid (via
// ValidateSignedWebBundleSignatures).
//
// On success, returns the integrity block if the IWA is backed by a signed web
// bundle, or `std::nullopt` if it is a dev-mode proxy.
void CheckTrustAndSignatures(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IwaSourceWithMode& location,
    const IwaOperation& operation,
    Profile* profile,
    base::OnceCallback<
        void(base::expected<
             std::optional<web_package::SignedWebBundleIntegrityBlock>,
             std::string>)> callback);

// Verifies the trust and signatures of an Isolated Web App.
//
// Use this overload if you do not need the returned integrity block.
void CheckTrustAndSignatures(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IwaSourceWithMode& location,
    const IwaOperation& operation,
    Profile* profile,
    base::OnceCallback<void(base::expected<void, std::string>)> callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TRUST_AND_SIGNATURE_VERIFIER_H_
