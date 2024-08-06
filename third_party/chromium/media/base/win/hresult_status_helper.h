// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_WIN_HRESULT_STATUS_HELPER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_WIN_HRESULT_STATUS_HELPER_H_

#include <wrl/client.h>

#include "third_party/chromium/media/base/status.h"

namespace media_m96 {

// Generate a status from an HRESULT. If message is provided, it will add the
// HRESULT hex value as a data value to the status. Otherwise, the hex value
// will be included in the error message itself.
Status HresultToStatus(
    HRESULT hresult,
    const char* message = nullptr,
    StatusCode code = StatusCode::kWindowsWrappedHresult,
    const base::Location& location = base::Location::Current());

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_WIN_HRESULT_STATUS_HELPER_H_
