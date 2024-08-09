// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_VIDEO_STREAM_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_VIDEO_STREAM_H_

#include <mfapi.h>
#include <mfidl.h>

#include "third_party/chromium/media/renderers/win/media_foundation_stream_wrapper.h"

#include "third_party/chromium/media/media_buildflags.h"

namespace media_m96 {

// The common video stream.
class MediaFoundationVideoStream : public MediaFoundationStreamWrapper {
 public:
  static HRESULT Create(int stream_id,
                        IMFMediaSource* parent_source,
                        DemuxerStream* demuxer_stream,
                        std::unique_ptr<MediaLog> media_log,
                        MediaFoundationStreamWrapper** stream_out);

  bool IsEncrypted() const override;

 protected:
  HRESULT GetMediaType(IMFMediaType** media_type_out) override;
};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// The H264 specific video stream.
class MediaFoundationH264VideoStream : public MediaFoundationVideoStream {
 protected:
  HRESULT GetMediaType(IMFMediaType** media_type_out) override;
  bool AreFormatChangesEnabled() override;
};
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

#if BUILDFLAG(ENABLE_PLATFORM_HEVC) || BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
// The HEVC specific video stream.
class MediaFoundationHEVCVideoStream : public MediaFoundationVideoStream {
 protected:
  bool AreFormatChangesEnabled() override;
};
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) ||
        // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_VIDEO_STREAM_H_
