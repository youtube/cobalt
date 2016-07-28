// Copyright 2016 Google Inc. All Rights Reserved.
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

// Media definitions that are common between the Decoder and Player interfaces.

#ifndef STARBOARD_MEDIA_H_
#define STARBOARD_MEDIA_H_

#include "starboard/drm.h"
#include "starboard/export.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Types -----------------------------------------------------------------

// Time represented in 90KHz ticks.
typedef int64_t SbMediaTime;

// Types of media component streams.
typedef enum SbMediaType {
  // Value used for audio streams.
  kSbMediaTypeAudio,

  // Value used for video streams.
  kSbMediaTypeVideo,
} SbMediaType;

// Possibly supported types of video elementary streams.
typedef enum SbMediaVideoCodec {
  kSbMediaVideoCodecNone,

  kSbMediaVideoCodecH264,
  kSbMediaVideoCodecH265,
  kSbMediaVideoCodecMpeg2,
  kSbMediaVideoCodecTheora,
  kSbMediaVideoCodecVc1,
  kSbMediaVideoCodecVp10,
  kSbMediaVideoCodecVp8,
  kSbMediaVideoCodecVp9,
} SbMediaVideoCodec;

// Possibly supported types of audio elementary streams.
typedef enum SbMediaAudioCodec {
  kSbMediaAudioCodecNone,

  kSbMediaAudioCodecAac,
  kSbMediaAudioCodecOpus,
  kSbMediaAudioCodecVorbis,
} SbMediaAudioCodec;

// Possible audio connector types.
typedef enum SbMediaAudioConnector {
  kSbMediaAudioConnectorNone,

  kSbMediaAudioConnectorAnalog,
  kSbMediaAudioConnectorBluetooth,
  kSbMediaAudioConnectorHdmi,
  kSbMediaAudioConnectorNetwork,
  kSbMediaAudioConnectorSpdif,
  kSbMediaAudioConnectorUsb,
} SbMediaAudioConnector;

// Possible audio coding types.
typedef enum SbMediaAudioCodingType {
  kSbMediaAudioCodingTypeNone,

  kSbMediaAudioCodingTypeAac,
  kSbMediaAudioCodingTypeAc3,
  kSbMediaAudioCodingTypeAtrac,
  kSbMediaAudioCodingTypeBitstream,
  kSbMediaAudioCodingTypeDolbyDigitalPlus,
  kSbMediaAudioCodingTypeDts,
  kSbMediaAudioCodingTypeMpeg1,
  kSbMediaAudioCodingTypeMpeg2,
  kSbMediaAudioCodingTypeMpeg3,
  kSbMediaAudioCodingTypePcm,
} SbMediaAudioCodingType;

// Possible audio sample types.
typedef enum SbMediaAudioSampleType {
  kSbMediaAudioSampleTypeInt16,
  kSbMediaAudioSampleTypeFloat32,
} SbMediaAudioSampleType;

// Possible audio frame storage types.  Interleaved means the samples of a
// multi-channel audio stream are stored in one continuous buffer, samples at
// the same timestamp are stored one after another.  Planar means the samples
// of each channel are stored in their own continuous buffer. For example, for
// stereo stream with channels L and R that contains samples with timestamp
// 0, 1, ..., interleaved means the samples are stored in one buffer as
// "L0 R0 L1 R1 L2 R2 ...".  Planar means the samples are stored in two buffers
// "L0 L1 L2 ..." and "R0 R1 R2 ...".
typedef enum SbMediaAudioFrameStorageType {
  kSbMediaAudioFrameStorageTypeInterleaved,
  kSbMediaAudioFrameStorageTypePlanar,
} SbMediaAudioFrameStorageType;

// The set of information required by the decoder or player for each video
// sample.
typedef struct SbMediaVideoSampleInfo {
  // Whether the associated sample is a key frame (I-frame). Video key frames
  // must always start with SPS and PPS NAL units.
  bool is_key_frame;

  // The frame width of this sample, in pixels. Also could be parsed from the
  // Sequence Parameter Set (SPS) NAL Unit. Frame dimensions must only change on
  // key frames, but may change on any key frame.
  int frame_width;

  // The frame height of this sample, in pixels. Also could be parsed from the
  // Sequence Parameter Set (SPS) NAL Unit. Frame dimensions must only change on
  // key frames, but may change on any key frame.
  int frame_height;
} SbMediaVideoSampleInfo;

// A structure describing the audio configuration parameters of a single audio
// output.
typedef struct SbMediaAudioConfiguration {
  // The platform-defined index of the associated audio output.
  int index;

  // The type of audio connector. Will be the empty kSbMediaAudioConnectorNone
  // if this device cannot provide this information.
  SbMediaAudioConnector connector;

  // The expected latency of audio over this output, in microseconds, or 0 if
  // this device cannot provide this information.
  SbTime latency;

  // The type of audio coding used over this connection.
  SbMediaAudioCodingType coding_type;

  // The number of audio channels currently supported by this device output, or
  // 0 if this device cannot provide this information, in which case the caller
  // can probably assume stereo output.
  int number_of_channels;
} SbMediaAudioConfiguration;

// An audio sequence header, which is a description of a given audio stream.
// This, in hexadecimal string form, acts as a set of instructions to the audio
// decoder.
//
// The Sequence Header consists of a little-endian hexadecimal encoded
// WAVEFORMATEX structure followed by an Audio-specific configuration field.
//
// Specification of WAVEFORMATEX structure:
// http://msdn.microsoft.com/en-us/library/dd390970(v=vs.85).aspx
//
// AudioSpecificConfig defined here in section 1.6.2.1:
// http://read.pudn.com/downloads98/doc/comm/401153/14496/ISO_IEC_14496-3%20Part%203%20Audio/C036083E_SUB1.PDF
typedef struct SbMediaAudioHeader {
  // The waveform-audio format type code.
  uint16_t format_tag;

  // The number of audio channels in this format. 1 for mono, 2 for stereo.
  uint16_t number_of_channels;

  // The sampling rate.
  uint32_t samples_per_second;

  // The number of bytes per second expected with this format.
  uint32_t average_bytes_per_second;

  // Byte block alignment, e.g, 4.
  uint16_t block_alignment;

  // The bit depth for the stream this represents, e.g. 8 or 16.
  uint16_t bits_per_sample;

  // The size, in bytes, of the audio_specific_config.
  uint16_t audio_specific_config_size;

  // The AudioSpecificConfig, as specified in ISO/IEC-14496-3.
  int8_t audio_specific_config[8];
} SbMediaAudioHeader;

// --- Constants -------------------------------------------------------------
// One second in SbMediaTime (90KHz ticks).
#define kSbMediaTimeSecond ((SbMediaTime)(90000))

// --- Functions -------------------------------------------------------------

// Returns whether decoding |video_codec|, |audio_codec|, and decrypting using
// |key_system| is supported together by this platform.  If |video_codec| is
// kSbMediaVideoCodecNone or |audio_codec| is kSbMediaAudioCodecNone, this
// function should return true if |key_system| is supported on the platform to
// decode any supported input formats.
SB_EXPORT bool SbMediaIsSupported(SbMediaVideoCodec video_codec,
                                  SbMediaAudioCodec audio_codec,
                                  const char* key_system);

// Returns whether a given combination of |frame_width| x |frame_height| frames
// at |bitrate| and |fps| is supported on this platform with |video_codec|. If
// |video_codec| is not supported under any condition, this function will always
// return false.
//
// Any of the parameters may be set to 0 to mean that they shouldn't be
// considered.
SB_EXPORT bool SbMediaIsVideoSupported(SbMediaVideoCodec video_codec,
                                       int frame_width,
                                       int frame_height,
                                       int64_t bitrate,
                                       int fps);

// Returns whether |audio_codec| is supported on this platform at |bitrate|. If
// |audio_codec| is not supported under any condition, this function will always
// return false.
SB_EXPORT bool SbMediaIsAudioSupported(SbMediaVideoCodec audio_codec,
                                       int64_t bitrate);

// Returns the number of audio outputs currently available on this device.  It
// is expected that, even if the number of outputs or their audio configurations
// can't be determined, the platform will at least return a single output that
// supports at least stereo.
SB_EXPORT int SbMediaGetAudioOutputCount();

// Places the current physical audio configuration of audio output
// |output_index| on this device into |out_configuration|, which must not be
// NULL, or returns false if nothing could be determined on this platform, or if
// |output_index| doesn't exist on this device.
SB_EXPORT bool SbMediaGetAudioConfiguration(
    int output_index,
    SbMediaAudioConfiguration* out_configuration);

// Returns whether output copy protection is currently enabled on all capable
// outputs. If true, then non-protection-capable outputs are expected to be
// blanked.
SB_EXPORT bool SbMediaIsOutputProtected();

// Enables or disables output copy protection on all capable outputs. If
// enabled, then non-protection-capable outputs are expected to be blanked.
// Returns whether the operation was successful. Returns a success even if the
// call was redundant.
SB_EXPORT bool SbMediaSetOutputProtection(bool enabled);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_MEDIA_H_
