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

#ifndef COBALT_MEDIA_BASE_VIDEO_DUMPER_H_
#define COBALT_MEDIA_BASE_VIDEO_DUMPER_H_

#include <string>
#include <vector>

#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/video_decoder_config.h"
#include "cobalt/media/base/video_dmp_reader.h"
#include "starboard/file.h"

namespace cobalt {
namespace media {

#if COBALT_MEDIA_ENABLE_VIDEO_DUMPER

// This class saves video data according to the format specified inside
// video_dmp_reader.h.
class VideoDumper {
 public:
  typedef VideoDmpReader::EmeInitData EmeInitData;

  explicit VideoDumper(const char* file_name) {
    bool created = false;
    file_ = SbFileOpen(file_name, kSbFileCreateAlways | kSbFileWrite, &created,
                       NULL);
    DCHECK(created);
    DCHECK(SbFileIsValid(file_));

    // Using a local variable to avoid addressing a constant which may cause
    // link error.
    uint32 bom = VideoDmpReader::kBOM;
    Write<uint32>(bom);
  }

  ~VideoDumper() { SbFileClose(file_); }

  void DumpEmeInitData(const std::vector<EmeInitData>& eme_init_datas) {
    for (auto& eme_init_data : eme_init_datas) {
      if (eme_init_data.empty()) {
        continue;
      }
      Write<uint32>(VideoDmpReader::kRecordTypeEmeInitData);
      // EME init data size
      Write<uint32>(static_cast<uint32>(eme_init_data.size()));
      Write(eme_init_data.data(), eme_init_data.size());
    }
  }

  void DumpConfigs(const AudioDecoderConfig& audio_config,
                   const VideoDecoderConfig& video_config) {
    // Temporarily write empty audio/video configs
    Write<uint32>(VideoDmpReader::kRecordTypeAudioConfig);
    Write<uint32>(0);
    Write<uint32>(VideoDmpReader::kRecordTypeVideoConfig);
    Write<uint32>(0);
  }

  void DumpAccessUnit(const scoped_refptr<DecoderBuffer>& buffer) {
    uint32 dump_type;
    if (buffer->type() == DemuxerStream::AUDIO) {
      dump_type = VideoDmpReader::kRecordTypeAudioAccessUnit;
    } else if (buffer->type() == DemuxerStream::VIDEO) {
      dump_type = VideoDmpReader::kRecordTypeVideoAccessUnit;
    } else {
      NOTREACHED() << buffer->type();
    }
    Write(dump_type);

    const DecryptConfig* decrypt_config = buffer->decrypt_config();
    if (decrypt_config && decrypt_config->key_id().size() == 16 &&
        decrypt_config->iv().size() == 16) {
      uint32 record_size =
          sizeof(int64_t)                                       // timestamp
          + sizeof(uint32_t)                                    // key_id size
          + decrypt_config->key_id().size() + sizeof(uint32_t)  // iv size
          + decrypt_config->iv().size() + sizeof(uint32_t)  // subsample count
          +
          sizeof(uint32_t) * 2 *
              decrypt_config->subsamples().size()  // subsample count
          + sizeof(uint32_t)                       // size of encoded data
          + buffer->data_size();
      Write(static_cast<uint32>(record_size));
      Write(buffer->timestamp().InMicroseconds());
      Write<uint32>(decrypt_config->key_id().size());  // key_id size
      Write(&decrypt_config->key_id()[0], decrypt_config->key_id().size());
      Write<uint32>(decrypt_config->iv().size());  // iv size
      Write(&decrypt_config->iv()[0], decrypt_config->iv().size());
      // subsample count
      Write<uint32>(decrypt_config->subsamples().size());
      for (size_t i = 0; i < decrypt_config->subsamples().size(); ++i) {
        Write<uint32>(decrypt_config->subsamples()[i].clear_bytes);
        Write<uint32>(decrypt_config->subsamples()[i].cypher_bytes);
      }
      Write<uint32>(static_cast<uint32>(buffer->data_size()));
      Write(buffer->data(), buffer->data_size());
    } else {
      size_t record_size = sizeof(int64_t)     // timestamp
                           + sizeof(uint32_t)  // key_id size
                           + sizeof(uint32_t)  // iv size
                           + sizeof(uint32_t)  // subsample count
                           + sizeof(uint32_t)  // size of encoded data
                           + buffer->data_size();
      Write(static_cast<uint32>(record_size));
      Write(buffer->timestamp().InMicroseconds());
      Write<uint32>(0);  // key_id size
      Write<uint32>(0);  // iv size
      Write<uint32>(0);  // subsample count
      Write<uint32>(static_cast<uint32>(buffer->data_size()));
      Write(buffer->data(), buffer->data_size());
    }
  }

 private:
  template <typename T>
  void Write(const T& value) {
    int bytes_to_write = static_cast<int>(sizeof(value));
    int bytes_written = SbFileWrite(
        file_, reinterpret_cast<const char*>(&value), bytes_to_write);
    DCHECK_EQ(bytes_to_write, bytes_written);
  }

  void Write(const char* buffer, size_t size) {
    if (size == 0) {
      return;
    }
    int bytes_written = SbFileWrite(file_, buffer, static_cast<int>(size));
    DCHECK_EQ(static_cast<int>(size), bytes_written);
  }

  void Write(const uint8_t* buffer, size_t size) {
    Write(reinterpret_cast<const char*>(buffer), size);
  }

  SbFile file_;
};

#else  // COBALT_MEDIA_ENABLE_VIDEO_DUMPER

class VideoDumper {
 public:
  explicit VideoDumper(const char*) {}
  void StartDump(const std::vector<std::vector<uint8_t> >&) {}
  void DumpConfigs(const AudioDecoderConfig&, const VideoDecoderConfig&) {}
  void DumpAccessUnit(const scoped_refptr<DecoderBuffer>&) {}
};

#endif  // COBALT_MEDIA_ENABLE_VIDEO_DUMPER

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_VIDEO_DUMPER_H_
