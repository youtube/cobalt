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
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/ffmpeg/ffmpeg_demuxer.h"
#include "starboard/shared/ffmpeg/ffmpeg_demuxer_impl_interface.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

std::unique_ptr<FFmpegDemuxer> FFmpegDemuxer::Create(
    CobaltExtensionDemuxerDataSource* data_source) {
  FFMPEGDispatch* ffmpeg = FFMPEGDispatch::GetInstance();
  SB_DCHECK(ffmpeg && ffmpeg->is_valid());
  SB_DCHECK(FFMPEG == ffmpeg->specialization_version());

  return FFmpegDemuxerImpl<FFMPEG>::Create(data_source);
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
