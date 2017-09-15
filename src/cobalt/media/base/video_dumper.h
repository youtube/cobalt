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

// #define COBALT_MEDIA_ENABLE_VIDEO_DUMPER 1

#include <string>
#include <vector>

#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/decoder_buffer.h"
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

  explicit VideoDumper(const char* file_name);
  ~VideoDumper();

  void DumpEmeInitData(const std::vector<EmeInitData>& eme_init_datas);
  void DumpConfigs(const AudioDecoderConfig& audio_config,
                   const VideoDecoderConfig& video_config);
  void DumpAccessUnit(const scoped_refptr<DecoderBuffer>& buffer);

 private:
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
