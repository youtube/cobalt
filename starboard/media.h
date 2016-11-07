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

// Module Overview: Starboard Media module
//
// Provides media definitions that are common between the Decoder and Player
// interfaces.

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

// Types of video elementary streams that could be supported.
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

// Types of audio elementary streams that can be supported.
typedef enum SbMediaAudioCodec {
  kSbMediaAudioCodecNone,

  kSbMediaAudioCodecAac,
  kSbMediaAudioCodecOpus,
  kSbMediaAudioCodecVorbis,
} SbMediaAudioCodec;

// Indicates how confident the device is that it can play media resources
// of the given type. The values are a direct map of the canPlayType() method
// specified at the following link:
// https://www.w3.org/TR/2011/WD-html5-20110113/video.html#dom-navigator-canplaytype
typedef enum SbMediaSupportType {
  // The media type cannot be played.
  kSbMediaSupportTypeNotSupported,

  // Cannot determinate if the media type is playable without playing it.
  kSbMediaSupportTypeMaybe,

  // The media type seems to be playable.
  kSbMediaSupportTypeProbably,
} SbMediaSupportType;

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

// Possible audio frame storage types.
typedef enum SbMediaAudioFrameStorageType {
  // The samples of a multi-channel audio stream are stored in one continuous
  // buffer. Samples at the same timestamp are stored one after another. For
  // example, for a stereo stream with channels L and R that contains samples
  // with timestamps 0, 1, 2, etc., the samples are stored in one buffer as
  // "L0 R0 L1 R1 L2 R2 ...".
  kSbMediaAudioFrameStorageTypeInterleaved,

  // The samples of each channel are stored in their own continuous buffer.
  // For example, for a stereo stream with channels L and R that contains
  // samples with timestamps 0, 1, 2, etc., the samples are stored in two
  // buffers "L0 L1 L2 ..." and "R0 R1 R2 ...".
  kSbMediaAudioFrameStorageTypePlanar,
} SbMediaAudioFrameStorageType;

// The set of information required by the decoder or player for each video
// sample.
typedef struct SbMediaVideoSampleInfo {
  // Indicates whether the associated sample is a key frame (I-frame).
  // Video key frames must always start with SPS and PPS NAL units.
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

  // The type of audio connector. Will be the empty |kSbMediaAudioConnectorNone|
  // if this device cannot provide this information.
  SbMediaAudioConnector connector;

  // The expected latency of audio over this output, in microseconds, or |0| if
  // this device cannot provide this information.
  SbTime latency;

  // The type of audio coding used over this connection.
  SbMediaAudioCodingType coding_type;

  // The number of audio channels currently supported by this device output, or
  // |0| if this device cannot provide this information, in which case the
  // caller can probably assume stereo output.
  int number_of_channels;
} SbMediaAudioConfiguration;

// An audio sequence header, which is a description of a given audio stream.
// This, in hexadecimal string form, acts as a set of instructions to the audio
// decoder.
//
// The Sequence Header consists of a little-endian hexadecimal encoded
// |WAVEFORMATEX| structure followed by an Audio-specific configuration field.
// The |WAVEFORMATEX| structure is specified at:
// http://msdn.microsoft.com/en-us/library/dd390970(v=vs.85).aspx
typedef struct SbMediaAudioHeader {
  // The waveform-audio format type code.
  uint16_t format_tag;

  // The number of audio channels in this format. |1| for mono, |2| for stereo.
  uint16_t number_of_channels;

  // The sampling rate.
  uint32_t samples_per_second;

  // The number of bytes per second expected with this format.
  uint32_t average_bytes_per_second;

  // Byte block alignment, e.g, 4.
  uint16_t block_alignment;

  // The bit depth for the stream this represents, e.g. |8| or |16|.
  uint16_t bits_per_sample;

  // The size, in bytes, of the audio_specific_config.
  uint16_t audio_specific_config_size;

  // The AudioSpecificConfig, as specified in ISO/IEC-14496-3, section 1.6.2.1:
  // http://read.pudn.com/downloads98/doc/comm/401153/14496/ISO_IEC_14496-3%20Part%203%20Audio/C036083E_SUB1.PDF
  int8_t audio_specific_config[8];
} SbMediaAudioHeader;

// --- Constants -------------------------------------------------------------
// One second in SbMediaTime (90KHz ticks).
#define kSbMediaTimeSecond ((SbMediaTime)(90000))

// --- Functions -------------------------------------------------------------

// Indicates whether this platform supports decoding |video_codec| and
// |audio_codec| along with decrypting using |key_system|. If |video_codec| is
// |kSbMediaVideoCodecNone| or if |audio_codec| is |kSbMediaAudioCodecNone|,
// this function should return |true| as long as |key_system| is supported on
// the platform to decode any supported input formats.
//
// |video_codec|: The |SbMediaVideoCodec| being checked for platform
//   compatibility.
// |audio_codec|: The |SbMediaAudioCodec| being checked for platform
//   compatibility.
// |key_system|: The key system being checked for platform compatibility.
SB_EXPORT bool SbMediaIsSupported(SbMediaVideoCodec video_codec,
                                  SbMediaAudioCodec audio_codec,
                                  const char* key_system);

// Indicates whether a given combination of
// (|frame_width| x |frame_height|) frames at |bitrate| and |fps| is supported
// on this platform with |video_codec|. If |video_codec| is not supported under
// any condition, this function returns |false|.
//
// Setting any of the parameters to |0| indicates that they shouldn't be
// considered.
//
// |video_codec|: The video codec used in the media content.
// |frame_width|: The frame width of the media content.
// |frame_height|: The frame height of the media content.
// |bitrate|: The bitrate of the media content.
// |fps|: The number of frames per second in the media content.
SB_EXPORT bool SbMediaIsVideoSupported(SbMediaVideoCodec video_codec,
                                       int frame_width,
                                       int frame_height,
                                       int64_t bitrate,
                                       int fps);

// Indicates whether this platform supports |audio_codec| at |bitrate|.
// If |audio_codec| is not supported under any condition, this function
// returns |false|.
//
// |audio_codec|: The media's audio codec (|SbMediaAudioCodec|).
// |bitrate|: The media's bitrate.
SB_EXPORT bool SbMediaIsAudioSupported(SbMediaVideoCodec audio_codec,
                                       int64_t bitrate);

// Returns information about whether the playback of the specific media
// described by |mime| and encrypted using |key_system| can be played.
//
// Note that neither |mime| nor |key_system| can be NULL. This function returns
// |kSbMediaSupportNotSupported| if either is NULL.
//
// |mime|: The mime information of the media in the form of |video/webm|
//   or |video/mp4; codecs="avc1.42001E"|. It may include arbitrary parameters
//   like "codecs", "channels", etc.
// |key_system|: A lowercase value in fhe form of "com.example.somesystem"
// as suggested by https://w3c.github.io/encrypted-media/#key-system
// that can be matched exactly with known DRM key systems of the platform.
// When |key_system| is an empty string, the return value is an indication for
// non-encrypted media.
SB_EXPORT SbMediaSupportType
SbMediaCanPlayMimeAndKeySystem(const char* mime, const char* key_system);

// Returns the number of audio outputs currently available on this device.
// Even if the number of outputs or their audio configurations can't be
// determined, it is expected that the platform will at least return a single
// output that supports at least stereo.
SB_EXPORT int SbMediaGetAudioOutputCount();

// Retrieves the current physical audio configuration of audio output
// |output_index| on this device and places it in |out_configuration|,
// which must not be NULL.
//
// This function returns |false| if nothing could be determined on this
// platform or if |output_index| does not exist on this device.
//
// |out_configuration|: The variable that holds the audio configuration
//   information.
SB_EXPORT bool SbMediaGetAudioConfiguration(
    int output_index,
    SbMediaAudioConfiguration* out_configuration);

// Indicates whether output copy protection is currently enabled on all capable
// outputs. If |true|, then non-protection-capable outputs are expected to be
// blanked.
SB_EXPORT bool SbMediaIsOutputProtected();

// Enables or disables output copy protection on all capable outputs. If
// enabled, then non-protection-capable outputs are expected to be blanked.
//
// The return value indicates whether the operation was successful, and the
// function returns a success even if the call is redundant in that it doesn't
// change the current value.
//
// |enabled|: Indicates whether output protection is enabled (|true|) or
//   disabled.
SB_EXPORT bool SbMediaSetOutputProtection(bool enabled);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_MEDIA_H_
