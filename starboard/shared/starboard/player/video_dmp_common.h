// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_COMMON_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_COMMON_H_

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/memory.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace video_dmp {

// Video dmp file format
// File: <Byte Order Mark> <Record>*
//   Byte Order Mark: 0x76543210
//   Dmp writer version number: 0x00010000
//   Record: <4 bytes fourcc type> + <4 bytes size> + <|size| bytes binary data>
//
//     audio config:
//       fourcc type: 'acfg'
//       2 bytes audio codec type in SbMediaAudioCodec
//       all members of SbMediaAudioSampleInfo
//
//     video config:
//       fourcc type: 'vcfg'
//       2 bytes video codec type in SbMediaVideoCodec
//
//     audio/video access unit:
//       fourcc type: 'adat'/'vdat'
//       <8 bytes time stamp int64_t>
//       <1 byte of drm_sample_info_present in bool>
//         <4 bytes size of key_id> + |size| bytes of key id
//         <4 bytes size of iv> + |size| bytes of iv
//         <4 bytes count> (0 for non-encrypted AU/frame)
//           (subsample: 4 bytes clear size, 4 bytes encrypted size) * |count|
//       <4 bytes size>
//         |size| bytes encoded binary data
//       all members of SbMediaAudioSampleInfo/SbMediaVideoSampleInfo
//         for audio/video access units

typedef std::function<int(const void*, int size)> WriteCB;
typedef std::function<int(void*, int size)> ReadCB;

enum RecordType {
  kRecordTypeAudioConfig = 'acfg',
  kRecordTypeVideoConfig = 'vcfg',
  kRecordTypeAudioAccessUnit = 'adat',
  kRecordTypeVideoAccessUnit = 'vdat',
};

struct SbDrmSampleInfoWithSubSampleMapping : public SbDrmSampleInfo {
  SbDrmSampleInfoWithSubSampleMapping() {}
  SbDrmSampleInfoWithSubSampleMapping(
      const SbDrmSampleInfoWithSubSampleMapping& that)
      : SbDrmSampleInfo(that),
        stored_subsample_mapping(that.stored_subsample_mapping) {
    subsample_mapping = stored_subsample_mapping.data();
  }
  void operator=(const SbDrmSampleInfoWithSubSampleMapping& that) = delete;

  std::vector<SbDrmSubSampleMapping> stored_subsample_mapping;
};

const uint32_t kByteOrderMark = 0x76543210;
const uint32_t kSupportedWriterVersion = 0x00001000;

void Read(const ReadCB& read_cb, void* buffer, size_t size);

template <typename T>
void Read(const ReadCB& read_cb, bool reverse_byte_order, T* value) {
  SB_DCHECK(value);
  Read(read_cb, value, sizeof(*value));
  if (reverse_byte_order) {
    std::reverse(reinterpret_cast<uint8_t*>(value),
                 reinterpret_cast<uint8_t*>(value + 1));
  }
}

void Write(const WriteCB& write_cb, const void* buffer, size_t size);

template <typename T>
void Write(const WriteCB& write_cb, T value) {
  Write(write_cb, &value, sizeof(value));
}

void Read(const ReadCB& read_cb, bool reverse_byte_order, bool* value);
void Write(const WriteCB& write_cb, bool value);

void Read(const ReadCB& read_cb, bool reverse_byte_order, int* value);
void Write(const WriteCB& write_cb, int value);

void Read(const ReadCB& read_cb, bool reverse_byte_order, unsigned int* value);
void Write(const WriteCB& write_cb, unsigned int value);

void Write(const WriteCB& write_cb, RecordType record_type);

void Read(const ReadCB& read_cb,
          bool reverse_byte_order,
          media::AudioSampleInfo* audio_sample_info);
void Write(const WriteCB& write_cb,
           SbMediaAudioCodec audio_codec,
           const media::AudioStreamInfo& audio_stream_info);

void Read(const ReadCB& read_cb,
          bool reverse_byte_order,
          SbDrmSampleInfoWithSubSampleMapping* drm_sample_info);
void Write(const WriteCB& write_cb, const SbDrmSampleInfo& drm_sample_info);

void Read(const ReadCB& read_cb,
          bool reverse_byte_order,
          media::VideoSampleInfo* video_sample_info);
void Write(const WriteCB& write_cb,
           SbMediaVideoCodec video_codec,
           const media::VideoSampleInfo& video_sample_info);

}  // namespace video_dmp
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_COMMON_H_
