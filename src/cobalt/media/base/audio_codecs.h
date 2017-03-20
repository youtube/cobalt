// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_AUDIO_CODECS_H_
#define COBALT_MEDIA_BASE_AUDIO_CODECS_H_

#include <string>

#include "cobalt/media/base/media_export.h"

namespace media {

enum AudioCodec {
  // These values are histogrammed over time; do not change their ordinal
  // values.  When deleting a codec replace it with a dummy value; when adding a
  // codec, do so at the bottom before kAudioCodecMax, and update the value of
  // kAudioCodecMax to equal the new codec.
  kUnknownAudioCodec = 0,
  kCodecAAC = 1,
  kCodecMP3 = 2,
  kCodecPCM = 3,
  kCodecVorbis = 4,
  kCodecFLAC = 5,
  kCodecAMR_NB = 6,
  kCodecAMR_WB = 7,
  kCodecPCM_MULAW = 8,
  kCodecGSM_MS = 9,
  kCodecPCM_S16BE = 10,
  kCodecPCM_S24BE = 11,
  kCodecOpus = 12,
  kCodecEAC3 = 13,
  kCodecPCM_ALAW = 14,
  kCodecALAC = 15,
  kCodecAC3 = 16,
  // DO NOT ADD RANDOM AUDIO CODECS!
  //
  // The only acceptable time to add a new codec is if there is production code
  // that uses said codec in the same CL.

  // Must always be equal to the largest entry ever logged.
  kAudioCodecMax = kCodecAC3,
};

std::string MEDIA_EXPORT GetCodecName(AudioCodec codec);

MEDIA_EXPORT AudioCodec StringToAudioCodec(const std::string& codec_id);

}  // namespace media

#endif  // COBALT_MEDIA_BASE_AUDIO_CODECS_H_
