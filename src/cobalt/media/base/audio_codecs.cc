// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/audio_codecs.h"

#include "base/logging.h"
#include "base/string_util.h"

namespace media {

// These names come from src/third_party/ffmpeg/libavcodec/codec_desc.c
std::string GetCodecName(AudioCodec codec) {
  switch (codec) {
    case kUnknownAudioCodec:
      return "unknown";
    case kCodecAAC:
      return "aac";
    case kCodecMP3:
      return "mp3";
    case kCodecPCM:
    case kCodecPCM_S16BE:
    case kCodecPCM_S24BE:
      return "pcm";
    case kCodecVorbis:
      return "vorbis";
    case kCodecFLAC:
      return "flac";
    case kCodecAMR_NB:
      return "amr_nb";
    case kCodecAMR_WB:
      return "amr_wb";
    case kCodecPCM_MULAW:
      return "pcm_mulaw";
    case kCodecGSM_MS:
      return "gsm_ms";
    case kCodecOpus:
      return "opus";
    case kCodecPCM_ALAW:
      return "pcm_alaw";
    case kCodecEAC3:
      return "eac3";
    case kCodecALAC:
      return "alac";
    case kCodecAC3:
      return "ac3";
  }
  NOTREACHED();
  return "";
}

AudioCodec StringToAudioCodec(const std::string& codec_id) {
  if (codec_id == "aac") return kCodecAAC;
  if (codec_id == "ac-3" || codec_id == "mp4a.A5") return kCodecAC3;
  if (codec_id == "ec-3" || codec_id == "mp4a.A6") return kCodecEAC3;
  if (codec_id == "mp3") return kCodecMP3;
  if (codec_id == "alac") return kCodecALAC;
  if (codec_id == "flac") return kCodecFLAC;
  if (codec_id == "opus") return kCodecOpus;
  if (codec_id == "vorbis") return kCodecVorbis;
  if (StartsWithASCII(codec_id, "mp4a.40.", true)) return kCodecAAC;
  return kUnknownAudioCodec;
}

}  // namespace media
