// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_AUDIO_STREAM_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_AUDIO_STREAM_H_

#include <mfapi.h>
#include <mfidl.h>

#include "third_party/chromium/media/renderers/win/media_foundation_stream_wrapper.h"

#include "third_party/chromium/media/base/media_log.h"
#include "third_party/chromium/media/media_buildflags.h"

namespace media_m96 {

// The common audio stream.
class MediaFoundationAudioStream : public MediaFoundationStreamWrapper {
 public:
  static HRESULT Create(int stream_id,
                        IMFMediaSource* parent_source,
                        DemuxerStream* demuxer_stream,
                        std::unique_ptr<MediaLog> media_log,
                        MediaFoundationStreamWrapper** stream_out);
  bool IsEncrypted() const override;
  HRESULT GetMediaType(IMFMediaType** media_type_out) override;
};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// The AAC specific audio stream.
class MediaFoundationAACAudioStream : public MediaFoundationAudioStream {
 public:
  HRESULT GetMediaType(IMFMediaType** media_type_out) override;
  HRESULT TransformSample(Microsoft::WRL::ComPtr<IMFSample>& sample) override;

 private:
  bool enable_adts_header_removal_ = false;
};
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_AUDIO_STREAM_H_
