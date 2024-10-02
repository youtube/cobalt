// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_win.h"

#include "base/logging.h"
#include "content/browser/media/service_factory.h"
#include "media/cdm/win/media_foundation_cdm.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "url/gurl.h"

namespace content {

namespace {

template <typename T>
base::flat_set<T> VectorToSet(const std::vector<T>& v) {
  return base::flat_set<T>(v.begin(), v.end());
}

void OnKeySystemCapability(
    CdmCapabilityCB cdm_capability_cb,
    bool is_supported,
    media::mojom::KeySystemCapabilityPtr key_system_capability) {
  // Key system must support at least 1 video codec, 1 encryption scheme,
  // and 1 encryption scheme to be considered. Support for audio codecs is
  // optional.
  if (!is_supported || !key_system_capability ||
      !key_system_capability->hw_secure_capability ||
      key_system_capability->hw_secure_capability->video_codecs.empty() ||
      key_system_capability->hw_secure_capability->encryption_schemes.empty() ||
      key_system_capability->hw_secure_capability->session_types.empty()) {
    std::move(cdm_capability_cb).Run(absl::nullopt);
    return;
  }

  std::move(cdm_capability_cb).Run(key_system_capability->hw_secure_capability);
}

}  // namespace

void GetMediaFoundationServiceHardwareSecureCdmCapability(
    const std::string& key_system,
    const base::FilePath& cdm_path,
    CdmCapabilityCB cdm_capability_cb) {
  if (!media::MediaFoundationCdm::IsAvailable()) {
    DVLOG(1) << "MediaFoundationCdm not available!";
    std::move(cdm_capability_cb).Run(absl::nullopt);
    return;
  }

  // CDM capability is global, use a generic BrowserContext and Site to query.
  auto& mf_service = GetMediaFoundationService(nullptr, GURL(), cdm_path);
  mf_service.IsKeySystemSupported(
      key_system,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&OnKeySystemCapability, std::move(cdm_capability_cb)),
          false, nullptr));
}

}  // namespace content
