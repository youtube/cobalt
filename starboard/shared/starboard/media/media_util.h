// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_UTIL_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_UTIL_H_

#include <ostream>
#include <string>

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

bool IsAudioOutputSupported(SbMediaAudioCodingType coding_type, int channels);

bool IsSDRVideo(int bit_depth,
                SbMediaPrimaryId primary_id,
                SbMediaTransferId transfer_id,
                SbMediaMatrixId matrix_id);

// Turns |eotf| into value of SbMediaTransferId.  If |eotf| isn't recognized the
// function returns kSbMediaTransferIdReserved0.
// This function supports all eotfs required by YouTube TV HTML5 Technical
// Requirements (2018).
SbMediaTransferId GetTransferIdFromString(const std::string& eotf);

int GetBytesPerSample(SbMediaAudioSampleType sample_type);

// Calls to canPlayType() and isTypeSupported() are redirected to this function.
// Following are some example inputs:
//   canPlayType(video/mp4)
//   canPlayType(video/mp4; codecs="avc1.42001E, mp4a.40.2")
//   canPlayType(video/webm)
//   isTypeSupported(video/webm; codecs="vp9")
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; width=640)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; width=99999)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; height=360)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; height=99999)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; framerate=30)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; framerate=9999)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; bitrate=300000)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; bitrate=2000000000)
//   isTypeSupported(audio/mp4; codecs="mp4a.40.2")
//   isTypeSupported(audio/webm; codecs="vorbis")
//   isTypeSupported(video/webm; codecs="vp9")
//   isTypeSupported(video/webm; codecs="vp9")
//   isTypeSupported(audio/webm; codecs="opus")
//   isTypeSupported(audio/mp4; codecs="mp4a.40.2"; channels=2)
//   isTypeSupported(audio/mp4; codecs="mp4a.40.2"; channels=99)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; decode-to-texture=true)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; decode-to-texture=false)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; decode-to-texture=invalid)
SbMediaSupportType CanPlayMimeAndKeySystem(const MimeType& mime_type,
                                           const char* key_system);

const char* GetCodecName(SbMediaAudioCodec codec);
const char* GetCodecName(SbMediaVideoCodec codec);
const char* GetPrimaryIdName(SbMediaPrimaryId primary_id);
const char* GetTransferIdName(SbMediaTransferId transfer_id);
const char* GetMatrixIdName(SbMediaMatrixId matrix_id);
const char* GetRangeIdName(SbMediaRangeId range_id);

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

bool operator==(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2);
bool operator==(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2);

bool operator!=(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2);
bool operator!=(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2);

// For logging use only.
std::ostream& operator<<(std::ostream& os,
                         const SbMediaMasteringMetadata& metadata);
std::ostream& operator<<(std::ostream& os,
                         const SbMediaColorMetadata& metadata);
std::ostream& operator<<(std::ostream& os,
                         const SbMediaVideoSampleInfo& sample_info);

std::string GetHexRepresentation(const uint8_t* data, int size);
std::string GetStringRepresentation(const uint8_t* data, int size);
std::string GetMixedRepresentation(const uint8_t* data,
                                   int size,
                                   int bytes_per_line);

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_UTIL_H_
