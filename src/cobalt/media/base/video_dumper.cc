// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/media/base/video_dumper.h"

#if COBALT_MEDIA_ENABLE_VIDEO_DUMPER

#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/starboard_utils.h"

namespace cobalt {
namespace media {

namespace {

template <typename T>
void Write(SbFile file, const T& value) {
  int bytes_to_write = static_cast<int>(sizeof(value));
  int bytes_written =
      SbFileWrite(file, reinterpret_cast<const char*>(&value), bytes_to_write);
  DCHECK_EQ(bytes_to_write, bytes_written);
}

void Write(SbFile file, const void* buffer, size_t size) {
  Write(file, reinterpret_cast<const char*>(buffer), size);
}

}  // namespace

VideoDumper::VideoDumper(const char* file_name) {
  bool created = false;
  file_ =
      SbFileOpen(file_name, kSbFileCreateAlways | kSbFileWrite, &created, NULL);
  DCHECK(created);
  DCHECK(SbFileIsValid(file_));

  // Using a local variable to avoid addressing a constant which may cause
  // link error.
  uint32 bom = VideoDmpReader::kBOM;
  Write<uint32>(file_, bom);
}

VideoDumper::~VideoDumper() { SbFileClose(file_); }

void VideoDumper::DumpEmeInitData(
    const std::vector<EmeInitData>& eme_init_datas) {
  for (auto& eme_init_data : eme_init_datas) {
    if (eme_init_data.empty()) {
      continue;
    }
    Write<uint32>(file_, VideoDmpReader::kRecordTypeEmeInitData);
    // EME init data size
    Write<uint32>(file_, static_cast<uint32>(eme_init_data.size()));
    Write(file_, eme_init_data.data(), eme_init_data.size());
  }
}

void VideoDumper::DumpConfigs(const AudioDecoderConfig& audio_config,
                              const VideoDecoderConfig& video_config) {
  // Temporarily write empty audio/video configs
  Write<uint32>(file_, VideoDmpReader::kRecordTypeAudioConfig);
  Write<uint16_t>(file_,
                  MediaAudioCodecToSbMediaAudioCodec(audio_config.codec()));
  SbMediaAudioHeader audio_header =
      MediaAudioConfigToSbMediaAudioHeader(audio_config);
  Write<uint16_t>(file_, audio_header.format_tag);
  Write<uint16_t>(file_, audio_header.number_of_channels);
  Write<uint32_t>(file_, audio_header.samples_per_second);
  Write<uint32_t>(file_, audio_header.average_bytes_per_second);
  Write<uint16_t>(file_, audio_header.block_alignment);
  Write<uint16_t>(file_, audio_header.bits_per_sample);
  Write<uint16_t>(file_, audio_header.audio_specific_config_size);
  Write(file_, audio_header.audio_specific_config,
        audio_header.audio_specific_config_size);

  Write<uint32>(file_, VideoDmpReader::kRecordTypeVideoConfig);
  Write<uint16_t>(file_,
                  MediaVideoCodecToSbMediaVideoCodec(video_config.codec()));
}

void VideoDumper::DumpAccessUnit(const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK_EQ(buffer->allocations().number_of_buffers(), 1);

  uint32 dump_type;
  if (buffer->type() == DemuxerStream::AUDIO) {
    dump_type = VideoDmpReader::kRecordTypeAudioAccessUnit;
  } else if (buffer->type() == DemuxerStream::VIDEO) {
    dump_type = VideoDmpReader::kRecordTypeVideoAccessUnit;
  } else {
    NOTREACHED() << buffer->type();
  }
  Write(file_, dump_type);

  const DecryptConfig* decrypt_config = buffer->decrypt_config();
  if (decrypt_config && decrypt_config->key_id().size() == 16 &&
      decrypt_config->iv().size() == 16) {
    Write(file_, buffer->timestamp().InMicroseconds());
    Write<uint32>(file_, decrypt_config->key_id().size());  // key_id size
    Write(file_, &decrypt_config->key_id()[0], decrypt_config->key_id().size());
    Write<uint32>(file_, decrypt_config->iv().size());  // iv size
    Write(file_, &decrypt_config->iv()[0], decrypt_config->iv().size());
    // subsample count
    Write<uint32>(file_, decrypt_config->subsamples().size());
    for (size_t i = 0; i < decrypt_config->subsamples().size(); ++i) {
      Write<uint32>(file_, decrypt_config->subsamples()[i].clear_bytes);
      Write<uint32>(file_, decrypt_config->subsamples()[i].cypher_bytes);
    }
    Write<uint32>(file_, static_cast<uint32>(buffer->data_size()));
    Write(file_, buffer->allocations().buffers()[0], buffer->data_size());
  } else {
    Write(file_, buffer->timestamp().InMicroseconds());
    Write<uint32>(file_, 0);  // key_id size
    Write<uint32>(file_, 0);  // iv size
    Write<uint32>(file_, 0);  // subsample count
    Write<uint32>(file_, static_cast<uint32>(buffer->data_size()));
    Write(file_, buffer->allocations().buffers()[0], buffer->data_size());
  }
}

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_ENABLE_VIDEO_DUMPER
