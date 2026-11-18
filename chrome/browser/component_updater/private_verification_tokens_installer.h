// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PRIVATE_VERIFICATION_TOKENS_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PRIVATE_VERIFICATION_TOKENS_INSTALLER_H_

namespace component_updater {

class ComponentUpdateService;

// Registers PVT component with the ComponentUpdateService `cus`, if the PVT
// feature is enabled.
void RegisterPrivateVerificationTokensComponentIfEnabled(
    ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PRIVATE_VERIFICATION_TOKENS_INSTALLER_H_
