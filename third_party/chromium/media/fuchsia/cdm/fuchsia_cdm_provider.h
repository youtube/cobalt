// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_PROVIDER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_PROVIDER_H_

#include <fuchsia/media/drm/cpp/fidl.h>
#include <string>

#include "third_party/chromium/media/base/media_export.h"

namespace media_m96 {

// Interface to connect fuchsia::media_m96::drm::ContentDecryptionModule to the
// remote service.
class MEDIA_EXPORT FuchsiaCdmProvider {
 public:
  virtual ~FuchsiaCdmProvider() = default;
  virtual void CreateCdmInterface(
      const std::string& key_system,
      fidl::InterfaceRequest<fuchsia::media_m96::drm::ContentDecryptionModule>
          cdm_request) = 0;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_PROVIDER_H_
