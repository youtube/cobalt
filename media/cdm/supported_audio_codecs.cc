// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/supported_audio_codecs.h"

#include "media/media_buildflags.h"

namespace media {

const std::vector<AudioCodec> GetCdmSupportedAudioCodecs() {
  return {
    AudioCodec::kOpus, AudioCodec::kVorbis, AudioCodec::kFLAC,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
        AudioCodec::kAAC,
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  };
}

}  // namespace media
