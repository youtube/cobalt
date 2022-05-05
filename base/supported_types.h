// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SUPPORTED_TYPES_H_
#define MEDIA_BASE_SUPPORTED_TYPES_H_

#include "media/base/media_types.h"

namespace media {

// These functions will attempt to delegate to MediaClient (when present) to
// describe what types of media are supported. When no MediaClient is provided,
// they will fall back to calling the Default functions below.
MEDIA_EXPORT bool IsSupportedAudioType(const AudioType& type);
MEDIA_EXPORT bool IsSupportedVideoType(const VideoType& type);

// These functions describe what media/ alone supports. They do not call out to
// MediaClient and do not describe media/ embedder customization. Callers should
// generally prefer the non-Default APIs above.
MEDIA_EXPORT bool IsDefaultSupportedAudioType(const AudioType& type);
MEDIA_EXPORT bool IsDefaultSupportedVideoType(const VideoType& type);

}  // namespace media

#endif  // MEDIA_BASE_SUPPORTED_TYPES_H_
