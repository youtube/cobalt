// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_DECODER_BUFFER_CACHE_H_
#define COBALT_MEDIA_BASE_DECODER_BUFFER_CACHE_H_

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "starboard/media.h"

namespace cobalt {
namespace media {

// This class can be used to hold media buffers in decoding order.  It also
// provides function that given a media time, the function will find a key frame
// before the media time and clear all buffers before the key frame.  This class
// can be used to "replay" the video from the current playback position and can
// be useful to implement suspend/resume.
class DecoderBufferCache {
 public:
  typedef ::media::DecoderBuffer DecoderBuffer;
  typedef ::media::DemuxerStream DemuxerStream;
  typedef std::vector<scoped_refptr<DecoderBuffer>> DecoderBuffers;

  DecoderBufferCache();

  // TODO(b/268098991): Replace them with AudioStreamInfo and VideoStreamInfo
  //                    wrapper classes.
  void AddBuffers(const DecoderBuffers& buffers,
                  const SbMediaAudioStreamInfo& stream_info);
  void AddBuffers(const DecoderBuffers& buffers,
                  const SbMediaVideoStreamInfo& stream_info);
  void ClearSegmentsBeforeMediaTime(const base::TimeDelta& media_time);
  void ClearAll();

  // Start resuming, reset indices to audio/video buffers to the very beginning.
  void StartResuming();
  bool HasMoreBuffers(DemuxerStream::Type type) const;
  // |buffers| and |stream_info| cannot be null.
  void ReadBuffers(DecoderBuffers* buffers, size_t max_buffers_per_read,
                   SbMediaAudioStreamInfo* stream_info);
  void ReadBuffers(DecoderBuffers* buffers, size_t max_buffers_per_read,
                   SbMediaVideoStreamInfo* stream_info);

 private:
  // The buffers with same stream info will be stored in same segment.
  class BufferGroup {
   public:
    explicit BufferGroup(DemuxerStream::Type type);
    void AddBuffers(const DecoderBuffers& buffers,
                    const SbMediaAudioStreamInfo& stream_info);
    void AddBuffers(const DecoderBuffers& buffers,
                    const SbMediaVideoStreamInfo& stream_info);
    void ClearSegmentsBeforeMediaTime(const base::TimeDelta& media_time);
    void ClearAll();

    bool HasMoreBuffers() const;
    void ReadBuffers(DecoderBuffers* buffers, size_t max_buffers_per_read,
                     SbMediaAudioStreamInfo* stream_info);
    void ReadBuffers(DecoderBuffers* buffers, size_t max_buffers_per_read,
                     SbMediaVideoStreamInfo* stream_info);
    void ResetIndex();

   private:
    struct Segment {
      explicit Segment(const SbMediaAudioStreamInfo& stream_info);
      explicit Segment(const SbMediaVideoStreamInfo& stream_info);

      std::deque<scoped_refptr<DecoderBuffer>> buffers;
      union {
        SbMediaAudioStreamInfo audio_stream_info;
        SbMediaVideoStreamInfo video_stream_info;
      };
      // Deep copy of the audio and video stream infos.
      std::string mime;
      std::string max_video_capabilities;
      std::vector<uint8_t> audio_specific_config;
    };

    BufferGroup(const BufferGroup&) = delete;
    BufferGroup& operator=(const BufferGroup&) = delete;

    template <typename StreamInfo>
    void AddStreamInfo(const StreamInfo& stream_info);
    void AddBuffers(const DecoderBuffers& buffers);
    void AdvanceGroupIndexIfNecessary();
    void ReadBuffers(DecoderBuffers* buffers, size_t max_buffers_per_read);

    const DemuxerStream::Type type_;
    std::deque<Segment> segments_;
    std::deque<base::TimeDelta> key_frame_timestamps_;
    size_t segment_index_ = 0;
    size_t buffer_index_ = 0;
  };

  THREAD_CHECKER(thread_checker_);

  BufferGroup audio_buffer_group_;
  BufferGroup video_buffer_group_;
};

#if !defined(COBALT_BUILD_TYPE_GOLD)
// Expose the functions for tests.
bool operator!=(const SbMediaAudioStreamInfo& left,
                const SbMediaAudioStreamInfo& right);
bool operator!=(const SbMediaVideoStreamInfo& left,
                const SbMediaVideoStreamInfo& right);
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_DECODER_BUFFER_CACHE_H_
