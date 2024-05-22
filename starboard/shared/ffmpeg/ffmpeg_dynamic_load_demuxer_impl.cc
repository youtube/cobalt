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

#include <memory>

#include "starboard/common/log.h"
#include "starboard/shared/ffmpeg/ffmpeg_demuxer.h"
#include "starboard/shared/ffmpeg/ffmpeg_demuxer_impl_interface.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

// static
std::unique_ptr<FFmpegDemuxer> FFmpegDemuxer::Create(
    CobaltExtensionDemuxerDataSource* data_source) {
  FFMPEGDispatch* ffmpeg = FFMPEGDispatch::GetInstance();
  if (!ffmpeg || !ffmpeg->is_valid()) {
    return nullptr;
  }
  switch (ffmpeg->specialization_version()) {
    case 540:
      return FFmpegDemuxerImpl<540>::Create(data_source);
    case 550:
    case 560:
      return FFmpegDemuxerImpl<560>::Create(data_source);
    case 571:
    case 581:
      return FFmpegDemuxerImpl<581>::Create(data_source);
    case 591:
      return FFmpegDemuxerImpl<591>::Create(data_source);
    case 601:
      return FFmpegDemuxerImpl<601>::Create(data_source);
    default:
      SB_LOG(WARNING) << "Unsupported FFMPEG specialization "
                      << ffmpeg->specialization_version();
      return nullptr;
  }
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
