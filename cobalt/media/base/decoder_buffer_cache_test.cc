// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/base/decoder_buffer_cache.h"

#include <algorithm>

#include "base/time/time.h"
#include "cobalt/media/decoder_buffer_allocator.h"
#include "media/base/decoder_buffer.h"
#include "starboard/media.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {
namespace {

using ::media::DecoderBuffer;
using ::media::DemuxerStream;

constexpr size_t kMaxSamplesPerRead = 8;
constexpr size_t kBufferArraySize = 8;
constexpr size_t kBufferSize = 8;
constexpr uint8_t kBufferDataArray[kBufferArraySize][kBufferSize] = {
    {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01},
    {0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02},
    {0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03},
    {0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04},
    {0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05},
    {0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x06},
    {0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x07},
    {0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08},
};

const size_t kVideoKeyFrameFrequency = 4;
const size_t kDurationPerFrame = 16;

bool operator==(const SbMediaAudioStreamInfo& left,
                const SbMediaAudioStreamInfo& right) {
  return !(left != right);
}

bool operator==(const SbMediaVideoStreamInfo& left,
                const SbMediaVideoStreamInfo& right) {
  return !(left != right);
}

SbMediaAudioStreamInfo GetAudioStreamInfo() {
  SbMediaAudioStreamInfo stream_info;
  stream_info.codec = kSbMediaAudioCodecAac;
  stream_info.mime = "";
  stream_info.number_of_channels = 2;
  stream_info.samples_per_second = 44100;
  stream_info.audio_specific_config_size = 0;
  return stream_info;
}

SbMediaVideoStreamInfo GetVideoStreamInfo() {
  SbMediaVideoStreamInfo stream_info;
  stream_info.codec = kSbMediaVideoCodecH264;
  stream_info.mime = "";
  stream_info.max_video_capabilities = "";
  stream_info.frame_width = 1920;
  stream_info.frame_height = 1080;
  return stream_info;
}

DecoderBufferCache::DecoderBuffers GetAudioSourceBuffers() {
  DecoderBufferCache::DecoderBuffers source_buffers;
  for (size_t i = 0; i < kBufferArraySize; i++) {
    auto decoder_buffer =
        media::DecoderBuffer::CopyFrom(kBufferDataArray[i], kBufferSize);
    decoder_buffer->set_is_key_frame(true);
    decoder_buffer->set_timestamp(
        base::TimeDelta::FromMilliseconds(kDurationPerFrame * i));
    source_buffers.push_back(decoder_buffer);
  }
  return source_buffers;
}

DecoderBufferCache::DecoderBuffers GetVideoSourceBuffers() {
  DecoderBufferCache::DecoderBuffers source_buffers;
  for (size_t i = 0; i < kBufferArraySize; i++) {
    auto decoder_buffer =
        media::DecoderBuffer::CopyFrom(kBufferDataArray[i], kBufferSize);
    if (i % kVideoKeyFrameFrequency == 0) {
      decoder_buffer->set_is_key_frame(true);
      decoder_buffer->set_timestamp(
          base::TimeDelta::FromMilliseconds(kDurationPerFrame * i));
    } else {
      // Intended to reverse the timestamp orders.
      decoder_buffer->set_timestamp(base::TimeDelta::FromMilliseconds(
          kDurationPerFrame *
          (i + kVideoKeyFrameFrequency - 2 * (i % kVideoKeyFrameFrequency))));
    }
    source_buffers.push_back(decoder_buffer);
  }
  return source_buffers;
}

bool CompareOutputBuffers(const DecoderBufferCache::DecoderBuffers& source,
                          size_t start_index,
                          const DecoderBufferCache::DecoderBuffers& output) {
  if (start_index + output.size() > source.size()) {
    return false;
  }
  for (size_t i = 0; i < output.size(); i++) {
    if (source[start_index + i] != output[i]) {
      return false;
    }
  }
  return true;
}

template <typename StreamInfo>
void ReadAndCompareOutputs(DecoderBufferCache* cache, size_t buffers_to_read,
                           const DecoderBufferCache::DecoderBuffers& source,
                           size_t start_index,
                           const StreamInfo& source_stream_info) {
  StreamInfo output_stream_info;
  DecoderBufferCache::DecoderBuffers output_buffers;
  while (buffers_to_read > 0) {
    output_buffers.clear();
    cache->ReadBuffers(&output_buffers,
                       std::min(kMaxSamplesPerRead, buffers_to_read),
                       &output_stream_info);
    ASSERT_TRUE(source_stream_info == output_stream_info);
    ASSERT_GT(output_buffers.size(), 0);
    ASSERT_TRUE(CompareOutputBuffers(source, start_index, output_buffers));
    start_index += output_buffers.size();
    buffers_to_read -= output_buffers.size();
  }
}

size_t FindKeyFrameIndexBeforeTime(
    const DecoderBufferCache::DecoderBuffers& source,
    const base::TimeDelta& time) {
  SB_DCHECK(!source.empty());
  SB_DCHECK(source[0]->is_key_frame());

  size_t previous_key_frame_index = 0;
  for (size_t i = 1; i < source.size(); i++) {
    if (source[i]->is_key_frame()) {
      if (source[i]->timestamp() <= time) {
        previous_key_frame_index = i;
      } else {
        return previous_key_frame_index;
      }
    }
  }
  return previous_key_frame_index;
}

}  // namespace

TEST(DecoderBufferCacheTest, SunnyDay) {
  DecoderBufferAllocator allocator;
  DecoderBufferCache cache;

  SbMediaAudioStreamInfo source_audio_stream_info = GetAudioStreamInfo();
  SbMediaVideoStreamInfo source_video_stream_info = GetVideoStreamInfo();
  DecoderBufferCache::DecoderBuffers source_audio_buffers =
      GetAudioSourceBuffers();
  DecoderBufferCache::DecoderBuffers source_video_buffers =
      GetVideoSourceBuffers();
  cache.AddBuffers(source_audio_buffers, source_audio_stream_info);
  cache.AddBuffers(source_video_buffers, source_video_stream_info);

  ASSERT_NO_FATAL_FAILURE(
      ReadAndCompareOutputs(&cache, source_audio_buffers.size(),
                            source_audio_buffers, 0, source_audio_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));

  ASSERT_NO_FATAL_FAILURE(
      ReadAndCompareOutputs(&cache, source_video_buffers.size(),
                            source_video_buffers, 0, source_video_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));

  cache.ClearAll();
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));
}

TEST(DecoderBufferCacheTest, EmptyCache) {
  DecoderBufferCache cache;

  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));

  DecoderBufferCache::DecoderBuffers buffers;
  SbMediaAudioStreamInfo audio_stream_info;
  SbMediaVideoStreamInfo video_stream_info;

  cache.ReadBuffers(&buffers, kMaxSamplesPerRead, &audio_stream_info);
  ASSERT_TRUE(buffers.empty());
  cache.ReadBuffers(&buffers, kMaxSamplesPerRead, &video_stream_info);
  ASSERT_TRUE(buffers.empty());

  cache.ClearSegmentsBeforeMediaTime(base::TimeDelta());

  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));

  cache.ReadBuffers(&buffers, kMaxSamplesPerRead, &audio_stream_info);
  ASSERT_TRUE(buffers.empty());

  cache.ReadBuffers(&buffers, kMaxSamplesPerRead, &video_stream_info);
  ASSERT_TRUE(buffers.empty());
}

TEST(DecoderBufferCacheTest, ClearSegmentsWithZero) {
  DecoderBufferAllocator allocator;
  DecoderBufferCache cache;

  SbMediaAudioStreamInfo source_audio_stream_info = GetAudioStreamInfo();
  SbMediaVideoStreamInfo source_video_stream_info = GetVideoStreamInfo();
  DecoderBufferCache::DecoderBuffers source_audio_buffers =
      GetAudioSourceBuffers();
  DecoderBufferCache::DecoderBuffers source_video_buffers =
      GetVideoSourceBuffers();
  cache.AddBuffers(source_audio_buffers, source_audio_stream_info);
  cache.AddBuffers(source_video_buffers, source_video_stream_info);

  // Cache should not clear anything.
  cache.ClearSegmentsBeforeMediaTime(base::TimeDelta::FromMilliseconds(0));

  ASSERT_NO_FATAL_FAILURE(
      ReadAndCompareOutputs(&cache, source_audio_buffers.size(),
                            source_audio_buffers, 0, source_audio_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));

  ASSERT_NO_FATAL_FAILURE(
      ReadAndCompareOutputs(&cache, source_video_buffers.size(),
                            source_video_buffers, 0, source_video_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));
}

TEST(DecoderBufferCacheTest, ClearSegmentsAndResume) {
  DecoderBufferAllocator allocator;
  DecoderBufferCache cache;

  SbMediaAudioStreamInfo source_audio_stream_info = GetAudioStreamInfo();
  SbMediaVideoStreamInfo source_video_stream_info = GetVideoStreamInfo();
  DecoderBufferCache::DecoderBuffers source_audio_buffers =
      GetAudioSourceBuffers();
  DecoderBufferCache::DecoderBuffers source_video_buffers =
      GetVideoSourceBuffers();
  cache.AddBuffers(source_audio_buffers, source_audio_stream_info);
  cache.AddBuffers(source_video_buffers, source_video_stream_info);

  base::TimeDelta clear_time =
      base::TimeDelta::FromMilliseconds(kDurationPerFrame);
  cache.ClearSegmentsBeforeMediaTime(clear_time);

  size_t start_index =
      FindKeyFrameIndexBeforeTime(source_audio_buffers, clear_time);
  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_audio_buffers.size() - start_index, source_audio_buffers,
      start_index, source_audio_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));

  start_index = FindKeyFrameIndexBeforeTime(source_video_buffers, clear_time);
  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_video_buffers.size() - start_index, source_video_buffers,
      start_index, source_video_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));

  cache.StartResuming();

  start_index = FindKeyFrameIndexBeforeTime(source_audio_buffers, clear_time);
  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_audio_buffers.size() - start_index, source_audio_buffers,
      start_index, source_audio_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));

  start_index = FindKeyFrameIndexBeforeTime(source_video_buffers, clear_time);
  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_video_buffers.size() - start_index, source_video_buffers,
      start_index, source_video_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));

  clear_time = base::TimeDelta::FromMilliseconds(kDurationPerFrame *
                                                 (kVideoKeyFrameFrequency + 1));
  cache.ClearSegmentsBeforeMediaTime(clear_time);
  cache.StartResuming();

  start_index = FindKeyFrameIndexBeforeTime(source_audio_buffers, clear_time);
  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_audio_buffers.size() - start_index, source_audio_buffers,
      start_index, source_audio_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));

  start_index = FindKeyFrameIndexBeforeTime(source_video_buffers, clear_time);
  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_video_buffers.size() - start_index, source_video_buffers,
      start_index, source_video_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));
}

TEST(DecoderBufferCacheTest, MultipleStreamInfos) {
  DecoderBufferAllocator allocator;
  DecoderBufferCache cache;

  SbMediaAudioStreamInfo source_audio_stream_info_1 = GetAudioStreamInfo();
  SbMediaVideoStreamInfo source_video_stream_info_1 = GetVideoStreamInfo();

  SbMediaAudioStreamInfo source_audio_stream_info_2 = GetAudioStreamInfo();
  source_audio_stream_info_2.samples_per_second *= 2;
  SbMediaVideoStreamInfo source_video_stream_info_2 = GetVideoStreamInfo();
  source_video_stream_info_2.frame_width *= 2;
  source_video_stream_info_2.frame_height *= 2;

  DecoderBufferCache::DecoderBuffers source_audio_buffers =
      GetAudioSourceBuffers();
  DecoderBufferCache::DecoderBuffers source_video_buffers =
      GetVideoSourceBuffers();

  DecoderBufferCache::DecoderBuffers source_audio_buffers_1(
      source_audio_buffers.begin(),
      source_audio_buffers.begin() + kVideoKeyFrameFrequency);
  DecoderBufferCache::DecoderBuffers source_video_buffers_1(
      source_video_buffers.begin(),
      source_video_buffers.begin() + kVideoKeyFrameFrequency);

  DecoderBufferCache::DecoderBuffers source_audio_buffers_2(
      source_audio_buffers.begin() + kVideoKeyFrameFrequency,
      source_audio_buffers.end());
  DecoderBufferCache::DecoderBuffers source_video_buffers_2(
      source_video_buffers.begin() + kVideoKeyFrameFrequency,
      source_video_buffers.end());

  cache.AddBuffers(source_audio_buffers_1, source_audio_stream_info_1);
  cache.AddBuffers(source_video_buffers_1, source_video_stream_info_1);
  cache.AddBuffers(source_audio_buffers_2, source_audio_stream_info_2);
  cache.AddBuffers(source_video_buffers_2, source_video_stream_info_2);

  DecoderBufferCache::DecoderBuffers output_buffers;
  SbMediaAudioStreamInfo output_audio_stream_info;
  SbMediaVideoStreamInfo output_video_stream_info;

  // The output buffers in one read call should have same configuration.
  cache.ReadBuffers(&output_buffers, kMaxSamplesPerRead,
                    &output_audio_stream_info);
  ASSERT_TRUE(source_audio_stream_info_1 == output_audio_stream_info);
  ASSERT_EQ(output_buffers.size(), source_audio_buffers_1.size());

  output_buffers.clear();
  cache.ReadBuffers(&output_buffers, kMaxSamplesPerRead,
                    &output_video_stream_info);
  ASSERT_TRUE(source_video_stream_info_1 == output_video_stream_info);
  ASSERT_EQ(output_buffers.size(), source_video_buffers_1.size());

  cache.StartResuming();
  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_audio_buffers_1.size(), source_audio_buffers_1, 0,
      source_audio_stream_info_1));

  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_video_buffers_1.size(), source_video_buffers_1, 0,
      source_video_stream_info_1));

  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_audio_buffers_2.size(), source_audio_buffers_2, 0,
      source_audio_stream_info_2));

  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_video_buffers_2.size(), source_video_buffers_2, 0,
      source_video_stream_info_2));

  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));

  base::TimeDelta clear_time = base::TimeDelta::FromMilliseconds(
      kDurationPerFrame * (kVideoKeyFrameFrequency));
  cache.ClearSegmentsBeforeMediaTime(clear_time);
  cache.StartResuming();

  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_audio_buffers_2.size(), source_audio_buffers_2, 0,
      source_audio_stream_info_2));

  ASSERT_NO_FATAL_FAILURE(ReadAndCompareOutputs(
      &cache, source_video_buffers_2.size(), source_video_buffers_2, 0,
      source_video_stream_info_2));

  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));
}

TEST(DecoderBufferCacheTest, AudioOnly) {
  DecoderBufferAllocator allocator;
  DecoderBufferCache cache;

  SbMediaAudioStreamInfo source_audio_stream_info = GetAudioStreamInfo();
  DecoderBufferCache::DecoderBuffers source_audio_buffers =
      GetAudioSourceBuffers();
  cache.AddBuffers(source_audio_buffers, source_audio_stream_info);

  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));

  ASSERT_NO_FATAL_FAILURE(
      ReadAndCompareOutputs(&cache, source_audio_buffers.size(),
                            source_audio_buffers, 0, source_audio_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));
}

TEST(DecoderBufferCacheTest, VideoOnly) {
  DecoderBufferAllocator allocator;
  DecoderBufferCache cache;

  SbMediaVideoStreamInfo source_video_stream_info = GetVideoStreamInfo();
  DecoderBufferCache::DecoderBuffers source_video_buffers =
      GetVideoSourceBuffers();
  cache.AddBuffers(source_video_buffers, source_video_stream_info);

  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::AUDIO));

  ASSERT_NO_FATAL_FAILURE(
      ReadAndCompareOutputs(&cache, source_video_buffers.size(),
                            source_video_buffers, 0, source_video_stream_info));
  ASSERT_FALSE(cache.HasMoreBuffers(DemuxerStream::VIDEO));
}

TEST(DecoderBufferCacheTest, StreamInfosDeepCopied) {
  const char* kAudioMime = "audio/mp4; codecs=\"mp4a.40.2\"";
  const char* kAudioSpecificConfig = "0000";
  const char* kVideoMime = "video/mp4; codecs=\"avc1.42001E\"";
  const char* kMaxVideoCapabilities = "width=1920; height=1080; framerate=15";

  DecoderBufferAllocator allocator;
  DecoderBufferCache cache;

  SbMediaAudioStreamInfo source_audio_stream_info = GetAudioStreamInfo();
  source_audio_stream_info.mime = kAudioMime;
  source_audio_stream_info.audio_specific_config_size =
      strlen(kAudioSpecificConfig);
  source_audio_stream_info.audio_specific_config = kAudioSpecificConfig;

  SbMediaVideoStreamInfo source_video_stream_info = GetVideoStreamInfo();
  source_video_stream_info.mime = kVideoMime;
  source_video_stream_info.max_video_capabilities = kMaxVideoCapabilities;

  DecoderBufferCache::DecoderBuffers source_audio_buffers =
      GetAudioSourceBuffers();
  DecoderBufferCache::DecoderBuffers source_video_buffers =
      GetVideoSourceBuffers();
  cache.AddBuffers(source_audio_buffers, source_audio_stream_info);
  cache.AddBuffers(source_video_buffers, source_video_stream_info);

  DecoderBufferCache::DecoderBuffers output_buffers;
  SbMediaAudioStreamInfo output_audio_stream_info;
  SbMediaVideoStreamInfo output_video_stream_info;

  output_buffers.clear();
  cache.ReadBuffers(&output_buffers, kMaxSamplesPerRead,
                    &output_audio_stream_info);
  ASSERT_TRUE(source_audio_stream_info == output_audio_stream_info);
  ASSERT_NE(source_audio_stream_info.mime, output_audio_stream_info.mime);
  ASSERT_NE(source_audio_stream_info.audio_specific_config,
            output_audio_stream_info.audio_specific_config);

  output_buffers.clear();
  cache.ReadBuffers(&output_buffers, kMaxSamplesPerRead,
                    &output_video_stream_info);
  ASSERT_TRUE(source_video_stream_info == output_video_stream_info);
  ASSERT_NE(source_video_stream_info.mime, output_video_stream_info.mime);
  ASSERT_NE(source_video_stream_info.max_video_capabilities,
            output_video_stream_info.max_video_capabilities);
}


}  // namespace media
}  // namespace cobalt
