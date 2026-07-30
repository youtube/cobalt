// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/private_verification_tokens_installer.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/private_verification_tokens_installer_policy.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "net/base/features.h"

namespace component_updater {

void RegisterPrivateVerificationTokensComponentIfEnabled(
    ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(
          net::features::kEnablePrivateVerificationTokens)) {
    return;
  }

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<PrivateVerificationTokensInstallerPolicy>(
          base::BindRepeating(
              [](std::unique_ptr<private_verification_tokens::
                                     PrivateVerificationTokensIssuerConfig>
                     issuer_config) {
                // TODO(crbug.com/513184977): Hook PVT service to the component
                // updater.
              })));

  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
