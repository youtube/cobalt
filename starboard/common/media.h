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

#include <ostream>

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
const char* GetMediaAudioStorageTypeName(
    SbMediaAudioFrameStorageType storage_type);

// This function parses the video codec string and returns a codec.  All fields
// will be filled with information parsed from the codec string when possible,
// otherwise they will have the following default values:
//            profile: -1
//              level: -1
//          bit_depth: 8
//         primary_id: kSbMediaPrimaryIdUnspecified
//        transfer_id: kSbMediaTransferIdUnspecified
//          matrix_id: kSbMediaMatrixIdUnspecified
// It returns true when |codec| contains a well-formed codec string, otherwise
// it returns false.
bool ParseVideoCodec(const char* codec_string,
                     SbMediaVideoCodec* codec,
                     int* profile,
                     int* level,
                     int* bit_depth,
                     SbMediaPrimaryId* primary_id,
                     SbMediaTransferId* transfer_id,
                     SbMediaMatrixId* matrix_id);

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

#if SB_API_VERSION >= 15
std::ostream& operator<<(std::ostream& os,
                         const SbMediaVideoStreamInfo& stream_info);
std::ostream& operator<<(std::ostream& os,
                         const SbMediaAudioStreamInfo& stream_info);
#endif  // SB_API_VERSION >= 15

#endif  // STARBOARD_COMMON_MEDIA_H_
