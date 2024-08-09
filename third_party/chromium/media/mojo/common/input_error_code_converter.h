// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_COMMON_INPUT_ERROR_CODE_CONVERTER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_COMMON_INPUT_ERROR_CODE_CONVERTER_H_

#include "third_party/chromium/media/base/audio_capturer_source.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"

namespace media_m96 {
AudioCapturerSource::ErrorCode ConvertToCaptureCallbackCode(
    mojom::InputStreamErrorCode code);
}

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_COMMON_INPUT_ERROR_CODE_CONVERTER_H_
