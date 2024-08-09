// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CDM_SUPPORTED_AUDIO_CODECS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CDM_SUPPORTED_AUDIO_CODECS_H_

#include <vector>

#include "third_party/chromium/media/base/audio_codecs.h"
#include "third_party/chromium/media/base/media_export.h"

namespace media_m96 {

// Returns a set of audio codecs that are supported for decoding by the
// browser after a CDM has decrypted the stream. This will be used by
// CDMs that only support decryption of audio content.
// Note that this should only be used on desktop CDMs. On other platforms
// (e.g. Android) we should query the system for (encrypted) audio codec
// support.
MEDIA_EXPORT const std::vector<AudioCodec> GetCdmSupportedAudioCodecs();

}  //  namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CDM_SUPPORTED_AUDIO_CODECS_H_
