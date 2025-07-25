// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/common/clearkey_drm_delegate_android.h"

#include "media/cdm/clear_key_cdm_common.h"

using media::kClearKeyUuid;

namespace cdm {

ClearKeyDrmDelegateAndroid::ClearKeyDrmDelegateAndroid() = default;

ClearKeyDrmDelegateAndroid::~ClearKeyDrmDelegateAndroid() = default;

const std::vector<uint8_t> ClearKeyDrmDelegateAndroid::GetUUID() const {
  return std::vector<uint8_t>(kClearKeyUuid,
                              kClearKeyUuid + std::size(kClearKeyUuid));
}

}  // namespace cdm
