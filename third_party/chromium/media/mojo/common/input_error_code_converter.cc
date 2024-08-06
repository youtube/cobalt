// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/mojo/common/input_error_code_converter.h"

namespace media_m96 {
AudioCapturerSource::ErrorCode ConvertToCaptureCallbackCode(
    mojom::InputStreamErrorCode code) {
  switch (code) {
    case mojom::InputStreamErrorCode::kSystemPermissions:
      return AudioCapturerSource::ErrorCode::kSystemPermissions;
    case mojom::InputStreamErrorCode::kDeviceInUse:
      return AudioCapturerSource::ErrorCode::kDeviceInUse;
    case mojom::InputStreamErrorCode::kUnknown:
      break;
  }
  return AudioCapturerSource::ErrorCode::kUnknown;
}
}  // namespace media_m96
