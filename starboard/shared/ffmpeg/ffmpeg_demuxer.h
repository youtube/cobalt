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

#include "cobalt/extension/demuxer.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

class FFMPEGDispatch;

// Returns a demuxer implementation based on FFmpeg.
const CobaltExtensionDemuxerApi* GetFFmpegDemuxerApi();

#if !defined(COBALT_BUILD_TYPE_GOLD)
// For testing purposes, the FFMPEGDispatch -- through which all FFmpeg calls go
// -- can be overridden. This should never be called in production code.
void TestOnlySetFFmpegDispatch(FFMPEGDispatch* dispatch);
#endif

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_DEMUXER_H_
