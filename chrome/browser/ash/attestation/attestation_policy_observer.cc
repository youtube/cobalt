// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/attestation_policy_observer.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/attestation/machine_certificate_uploader.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace attestation {

AttestationPolicyObserver::AttestationPolicyObserver(
    MachineCertificateUploader* certificate_uploader)
    : cros_settings_(CrosSettings::Get()),
      certificate_uploader_(certificate_uploader) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  attestation_subscription_ = cros_settings_->AddSettingsObserver(
      kDeviceAttestationEnabled,
      base::BindRepeating(&AttestationPolicyObserver::AttestationSettingChanged,
                          base::Unretained(this)));
  Start();
}

AttestationPolicyObserver::~AttestationPolicyObserver() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void AttestationPolicyObserver::AttestationSettingChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Start();
}

void AttestationPolicyObserver::Start() {
  // If attestation is not enabled, there is nothing to do.
  bool enabled = false;
  if (!cros_settings_->GetBoolean(kDeviceAttestationEnabled, &enabled) ||
      !enabled) {
    return;
  }
  certificate_uploader_->UploadCertificateIfNeeded(base::DoNothing());
}

}  // namespace attestation
}  // namespace ash
