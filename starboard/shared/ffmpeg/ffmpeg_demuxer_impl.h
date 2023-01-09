// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_DEMUXER_IMPL_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_DEMUXER_IMPL_H_

#include <deque>
#include <memory>
#include <vector>

#include "starboard/extension/demuxer.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/ffmpeg/ffmpeg_demuxer.h"
#include "starboard/shared/ffmpeg/ffmpeg_demuxer_impl_interface.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

class FFMPEGDispatch;

// Forward class declaration of the explicit specialization with value FFMPEG.
template <>
class FFmpegDemuxerImpl<FFMPEG>;

// Declare the explicit specialization of the class with value FFMPEG.
template <>
class FFmpegDemuxerImpl<FFMPEG> : public FFmpegDemuxer {
 public:
  // Creates an FFmpegDemuxer instance for a specific version of FFmpeg. Returns
  // null if the version of FFmpeg that is present is not supported.
  static std::unique_ptr<FFmpegDemuxer> Create(
      CobaltExtensionDemuxerDataSource* data_source);

  ~FFmpegDemuxerImpl() override;

  // FFmpegDemuxer implementation:
  CobaltExtensionDemuxerStatus Initialize() override;
  bool HasAudioStream() const override;
  bool HasVideoStream() const override;
  const CobaltExtensionDemuxerAudioDecoderConfig& GetAudioConfig()
      const override;
  const CobaltExtensionDemuxerVideoDecoderConfig& GetVideoConfig()
      const override;
  SbTime GetDuration() const override;
  SbTime GetStartTime() const override;
  SbTime GetTimelineOffset() const override;
  void Read(CobaltExtensionDemuxerStreamType type,
            CobaltExtensionDemuxerReadCB read_cb,
            void* read_cb_user_data) override;
  CobaltExtensionDemuxerStatus Seek(int64_t seek_time_us) override;

 private:
#if LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8
  struct ScopedPtrAVFreeContext {
    void operator()(void* ptr) const;
  };
#endif  // LIBAVUTIL_VERSION_INT >= LIBAVUTIL_VERSION_52_8

  struct ScopedPtrAVFreeAVIOContext {
    void operator()(void* ptr) const;
  };

  struct ScopedPtrAVFreePacket {
    void operator()(void* ptr) const;
  };

  using ScopedAVPacket = std::unique_ptr<AVPacket, ScopedPtrAVFreePacket>;

  explicit FFmpegDemuxerImpl(CobaltExtensionDemuxerDataSource* data_source);

  // Creates an empty ScopedAVPacket. The returned ScopedAVPacket will not be
  // null.
  // Since different versions of FFmpeg require different functions to create an
  // AVPacket, this function abstracts away those differences.
  ScopedAVPacket CreateScopedAVPacket();

  // Returns the next packet of type |type|, or nullptr if EoS has been reached
  // or an error was encountered.
  ScopedAVPacket GetNextPacket(CobaltExtensionDemuxerStreamType type);

  // Returns a buffered packet of type |type|, or nullptr if no buffered packet
  // is available.
  ScopedAVPacket GetBufferedPacket(CobaltExtensionDemuxerStreamType type);

  // Pushes |packet| into the queue specified by |type|.
  void BufferPacket(ScopedAVPacket packet,
                    CobaltExtensionDemuxerStreamType type);

  bool ParseAudioConfig(AVStream* audio_stream,
                        CobaltExtensionDemuxerAudioDecoderConfig* config);

  bool ParseVideoConfig(AVStream* video_stream,
                        CobaltExtensionDemuxerVideoDecoderConfig* config);

  CobaltExtensionDemuxerDataSource* data_source_ = nullptr;
  AVStream* video_stream_ = nullptr;
  AVStream* audio_stream_ = nullptr;
  std::deque<ScopedAVPacket> video_packets_;
  std::deque<ScopedAVPacket> audio_packets_;

  std::vector<uint8_t> extra_audio_data_;
  std::vector<uint8_t> extra_video_data_;

  // These will only be properly populated if the corresponding AVStream is not
  // null.
  CobaltExtensionDemuxerAudioDecoderConfig audio_config_ = {};
  CobaltExtensionDemuxerVideoDecoderConfig video_config_ = {};

  bool initialized_ = false;
  int64_t start_time_ = 0L;
  int64_t duration_us_ = 0L;
  int64_t timeline_offset_us_ = 0L;

  // FFmpeg-related structs.
  std::unique_ptr<AVIOContext, ScopedPtrAVFreeAVIOContext> avio_context_;
  AVFormatContext* format_context_ = nullptr;
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_DEMUXER_IMPL_H_
