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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_DEMUXER_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_DEMUXER_H_

#include <memory>

#include "starboard/extension/demuxer.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

class FFMPEGDispatch;

// Returns a demuxer implementation based on FFmpeg.
const CobaltExtensionDemuxerApi* GetFFmpegDemuxerApi();

// The API of this class mirrors that of CobaltExtensionDemuxer; those calls get
// forwarded to an instance of this class. See demuxer.h for details about the
// API.
class FFmpegDemuxer {
 public:
  // Creates an FFmpegDemuxer for the currently-loaded FFmpeg library.
  static std::unique_ptr<FFmpegDemuxer> Create(
      CobaltExtensionDemuxerDataSource* data_source);

  FFmpegDemuxer();
  virtual ~FFmpegDemuxer();

  virtual CobaltExtensionDemuxerStatus Initialize() = 0;
  virtual bool HasAudioStream() const = 0;
  virtual bool HasVideoStream() const = 0;
  virtual const CobaltExtensionDemuxerAudioDecoderConfig& GetAudioConfig()
      const = 0;
  virtual const CobaltExtensionDemuxerVideoDecoderConfig& GetVideoConfig()
      const = 0;
  virtual SbTime GetDuration() const = 0;
  virtual SbTime GetStartTime() const = 0;
  virtual SbTime GetTimelineOffset() const = 0;
  virtual void Read(CobaltExtensionDemuxerStreamType type,
                    CobaltExtensionDemuxerReadCB read_cb,
                    void* read_cb_user_data) = 0;
  virtual CobaltExtensionDemuxerStatus Seek(int64_t seek_time_us) = 0;

  // All child classes should call this function to access FFmpeg.
  static const FFMPEGDispatch* GetDispatch();

#if !defined(COBALT_BUILD_TYPE_GOLD)
  // For testing purposes, the FFMPEGDispatch -- through which all FFmpeg calls
  // go -- can be overridden. This should never be called in production code.
  static void TestOnlySetFFmpegDispatch(FFMPEGDispatch* ffmpeg);
#endif
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_DEMUXER_H_
