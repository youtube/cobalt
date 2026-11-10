// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_MEDIA_H_
#define STARBOARD_COMMON_MEDIA_H_

#include <optional>
#include <ostream>
#include <string_view>

#include "starboard/media.h"

namespace starboard {

const char* GetMediaAudioCodecName(SbMediaAudioCodec codec);
const char* GetMediaVideoCodecName(SbMediaVideoCodec codec);
const char* GetMediaAudioConnectorName(SbMediaAudioConnector connector);
const char* GetMediaPrimaryIdName(SbMediaPrimaryId primary_id);
const char* GetMediaTransferIdName(SbMediaTransferId transfer_id);
const char* GetMediaMatrixIdName(SbMediaMatrixId matrix_id);
const char* GetMediaRangeIdName(SbMediaRangeId range_id);

const char* GetMediaAudioSampleTypeName(SbMediaAudioSampleType sample_type);

// Video codec information parsed from a codec string by ParseVideoCodec().
// Fields that cannot be determined from the codec string keep their default
// values.
struct ParsedVideoCodec {
  SbMediaVideoCodec codec = kSbMediaVideoCodecNone;
  int profile = -1;
  int level = -1;
  int bit_depth = 8;
  SbMediaPrimaryId primary_id = kSbMediaPrimaryIdUnspecified;
  SbMediaTransferId transfer_id = kSbMediaTransferIdUnspecified;
  SbMediaMatrixId matrix_id = kSbMediaMatrixIdUnspecified;
};

// Parses the video codec string (e.g. "vp09.02.10.10"). Returns std::nullopt
// when |codec_string| isn't a well-formed codec string. On success, the
// returned |codec| is never kSbMediaVideoCodecNone.
// |codec_string| doesn't need to be null-terminated.
std::optional<ParsedVideoCodec> ParseVideoCodec(std::string_view codec_string);

// For logging use only. Prints all fields of |codec_info|, e.g.
// "codec=vp9, profile=0, level=41, bit_depth=8, primary_id=bt709, ...".
std::ostream& operator<<(std::ostream& os, const ParsedVideoCodec& codec_info);

}  // namespace starboard

// For logging use only.
std::ostream& operator<<(std::ostream& os,
                         const SbMediaMasteringMetadata& metadata);
std::ostream& operator<<(std::ostream& os,
                         const SbMediaColorMetadata& metadata);
std::ostream& operator<<(std::ostream& os,
                         const SbMediaVideoSampleInfo& sample_info);
std::ostream& operator<<(std::ostream& os,
                         const SbMediaAudioSampleInfo& sample_info);
std::ostream& operator<<(std::ostream& os,
                         const SbMediaVideoStreamInfo& stream_info);
std::ostream& operator<<(std::ostream& os,
                         const SbMediaAudioStreamInfo& stream_info);

bool operator==(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2);

bool operator==(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2);

bool operator==(const SbMediaVideoStreamInfo& stream_info_1,
                const SbMediaVideoStreamInfo& stream_info_2);

bool operator!=(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2);

bool operator!=(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2);

bool operator!=(const SbMediaVideoStreamInfo& stream_info_1,
                const SbMediaVideoStreamInfo& stream_info_2);

#endif  // STARBOARD_COMMON_MEDIA_H_
