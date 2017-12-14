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

// SMPTE 2086 mastering data
//   http://ieeexplore.ieee.org/document/7291707/
// This standard specifies the metadata items to specify the color
// volume (the color primaries, white point, and luminance range) of
// the display that was used in mastering video content. The metadata
// is specified as a set of values independent of any specific digital
// representation.
// Also see the WebM container guidelines:
//   https://www.webmproject.org/docs/container/
typedef struct SbMediaMasteringMetadata {
  // Red X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
  float primary_r_chromaticity_x;

  // Red Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
  float primary_r_chromaticity_y;

  // Green X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
  float primary_g_chromaticity_x;

  // Green Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
  float primary_g_chromaticity_y;

  // Blue X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
  float primary_b_chromaticity_x;

  // Blue Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
  float primary_b_chromaticity_y;

  // White X chromaticity coordinate as defined by CIE 1931. In range [0, 1].
  float white_point_chromaticity_x;

  // White Y chromaticity coordinate as defined by CIE 1931. In range [0, 1].
  float white_point_chromaticity_y;

  // Maximum luminance. Shall be represented in candelas per square
  // meter (cd/m^2). In range [0, 9999.99].
  float luminance_max;

  // Minimum luminance. Shall be represented in candelas per square
  // meter (cd/m^2). In range [0, 9999.99].
  float luminance_min;
} SbMediaMasteringMetadata;

typedef enum SbMediaPrimaryId {
  // The first 0-255 values should match the H264 specification (see Table E-3
  // Colour Primaries in https://www.itu.int/rec/T-REC-H.264/en).
  kSbMediaPrimaryIdReserved0 = 0,
  kSbMediaPrimaryIdBt709 = 1,
  kSbMediaPrimaryIdUnspecified = 2,
  kSbMediaPrimaryIdReserved = 3,
  kSbMediaPrimaryIdBt470M = 4,
  kSbMediaPrimaryIdBt470Bg = 5,
  kSbMediaPrimaryIdSmpte170M = 6,
  kSbMediaPrimaryIdSmpte240M = 7,
  kSbMediaPrimaryIdFilm = 8,
  kSbMediaPrimaryIdBt2020 = 9,
  kSbMediaPrimaryIdSmpteSt4281 = 10,
  kSbMediaPrimaryIdSmpteSt4312 = 11,
  kSbMediaPrimaryIdSmpteSt4321 = 12,

  kSbMediaPrimaryIdLastStandardValue = kSbMediaPrimaryIdSmpteSt4321,

  // Chrome-specific values start at 1000.
  kSbMediaPrimaryIdUnknown = 1000,
  kSbMediaPrimaryIdXyzD50,
  kSbMediaPrimaryIdCustom,
  kSbMediaPrimaryIdLast = kSbMediaPrimaryIdCustom
} SbMediaPrimaryId;

typedef enum SbMediaTransferId {
  // The first 0-255 values should match the H264 specification (see Table E-4
  // Transfer Characteristics in https://www.itu.int/rec/T-REC-H.264/en).
  kSbMediaTransferIdReserved0 = 0,
  kSbMediaTransferIdBt709 = 1,
  kSbMediaTransferIdUnspecified = 2,
  kSbMediaTransferIdReserved = 3,
  kSbMediaTransferIdGamma22 = 4,
  kSbMediaTransferIdGamma28 = 5,
  kSbMediaTransferIdSmpte170M = 6,
  kSbMediaTransferIdSmpte240M = 7,
  kSbMediaTransferIdLinear = 8,
  kSbMediaTransferIdLog = 9,
  kSbMediaTransferIdLogSqrt = 10,
  kSbMediaTransferIdIec6196624 = 11,
  kSbMediaTransferIdBt1361Ecg = 12,
  kSbMediaTransferIdIec6196621 = 13,
  kSbMediaTransferId10BitBt2020 = 14,
  kSbMediaTransferId12BitBt2020 = 15,
  kSbMediaTransferIdSmpteSt2084 = 16,
  kSbMediaTransferIdSmpteSt4281 = 17,
  kSbMediaTransferIdAribStdB67 = 18,  // AKA hybrid-log gamma, HLG.

  kSbMediaTransferIdLastStandardValue = kSbMediaTransferIdSmpteSt4281,

  // Chrome-specific values start at 1000.
  kSbMediaTransferIdUnknown = 1000,
  kSbMediaTransferIdGamma24,

  // This is an ad-hoc transfer function that decodes SMPTE 2084 content
  // into a 0-1 range more or less suitable for viewing on a non-hdr
  // display.
  kSbMediaTransferIdSmpteSt2084NonHdr,

  // TODO: Need to store an approximation of the gamma function(s).
  kSbMediaTransferIdCustom,
  kSbMediaTransferIdLast = kSbMediaTransferIdCustom,
} SbMediaTransferId;

typedef enum SbMediaMatrixId {
  // The first 0-255 values should match the H264 specification (see Table E-5
  // Matrix Coefficients in https://www.itu.int/rec/T-REC-H.264/en).
  kSbMediaMatrixIdRgb = 0,
  kSbMediaMatrixIdBt709 = 1,
  kSbMediaMatrixIdUnspecified = 2,
  kSbMediaMatrixIdReserved = 3,
  kSbMediaMatrixIdFcc = 4,
  kSbMediaMatrixIdBt470Bg = 5,
  kSbMediaMatrixIdSmpte170M = 6,
  kSbMediaMatrixIdSmpte240M = 7,
  kSbMediaMatrixIdYCgCo = 8,
  kSbMediaMatrixIdBt2020NonconstantLuminance = 9,
  kSbMediaMatrixIdBt2020ConstantLuminance = 10,
  kSbMediaMatrixIdYDzDx = 11,

  kSbMediaMatrixIdLastStandardValue = kSbMediaMatrixIdYDzDx,

  // Chrome-specific values start at 1000
  kSbMediaMatrixIdUnknown = 1000,
  kSbMediaMatrixIdLast = kSbMediaMatrixIdUnknown,
} SbMediaMatrixId;

// This corresponds to the WebM Range enum which is part of WebM color data
// (see http://www.webmproject.org/docs/container/#Range).
// H.264 only uses a bool, which corresponds to the LIMITED/FULL values.
// Chrome-specific values start at 1000.
typedef enum SbMediaRangeId {
  // Range is not explicitly specified / unknown.
  kSbMediaRangeIdUnspecified = 0,

  // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
  kSbMediaRangeIdLimited = 1,

  // Full RGB color range with RGB valees from 0 to 255.
  kSbMediaRangeIdFull = 2,

  // Range is defined by TransferId/MatrixId.
  kSbMediaRangeIdDerived = 3,

  kSbMediaRangeIdLast = kSbMediaRangeIdDerived
} SbMediaRangeId;

// HDR (High Dynamic Range) Metadata common for HDR10 and
// WebM/VP9-based HDR formats, together with the ColorSpace. HDR
// reproduces a greater dynamic range of luminosity than is possible
// with standard digital imaging. See the Consumer Electronics
// Association press release:
// https://www.cta.tech/News/Press-Releases/2015/August/CEA-Defines-%E2%80%98HDR-Compatible%E2%80%99-Displays.aspx
typedef struct SbMediaColorMetadata {
  // Number of decoded bits per channel. A value of 0 indicates that
  // the BitsPerChannel is unspecified.
  unsigned int bits_per_channel;

  // The amount of pixels to remove in the Cr and Cb channels for
  // every pixel not removed horizontally. Example: For video with
  // 4:2:0 chroma subsampling, the |chroma_subsampling_horizontal| should be set
  // to 1.
  unsigned int chroma_subsampling_horizontal;

  // The amount of pixels to remove in the Cr and Cb channels for
  // every pixel not removed vertically. Example: For video with
  // 4:2:0 chroma subsampling, the |chroma_subsampling_vertical| should be set
  // to 1.
  unsigned int chroma_subsampling_vertical;

  // The amount of pixels to remove in the Cb channel for every pixel
  // not removed horizontally. This is additive with
  // ChromaSubsamplingHorz. Example: For video with 4:2:1 chroma
  // subsampling, the |chroma_subsampling_horizontal| should be set to 1 and
  // |cb_subsampling_horizontal| should be set to 1.
  unsigned int cb_subsampling_horizontal;

  // The amount of pixels to remove in the Cb channel for every pixel
  // not removed vertically. This is additive with
  // |chroma_subsampling_vertical|.
  unsigned int cb_subsampling_vertical;

  // How chroma is subsampled horizontally. (0: Unspecified, 1: Left
  // Collocated, 2: Half)
  unsigned int chroma_siting_horizontal;

  // How chroma is subsampled vertically. (0: Unspecified, 1: Top
  // Collocated, 2: Half)
  unsigned int chroma_siting_vertical;

  // [HDR Metadata field] SMPTE 2086 mastering data.
  SbMediaMasteringMetadata mastering_metadata;

  // [HDR Metadata field] Maximum brightness of a single pixel (Maximum
  // Content Light Level) in candelas per square meter (cd/m^2).
  unsigned int max_cll;

  // [HDR Metadata field] Maximum brightness of a single full frame
  // (Maximum Frame-Average Light Level) in candelas per square meter
  // (cd/m^2).
  unsigned int max_fall;

  // [Color Space field] The colour primaries of the video. For
  // clarity, the value and meanings for Primaries are adopted from
  // Table 2 of ISO/IEC 23001-8:2013/DCOR1. (0: Reserved, 1: ITU-R
  // BT.709, 2: Unspecified, 3: Reserved, 4: ITU-R BT.470M, 5: ITU-R
  // BT.470BG, 6: SMPTE 170M, 7: SMPTE 240M, 8: FILM, 9: ITU-R
  // BT.2020, 10: SMPTE ST 428-1, 22: JEDEC P22 phosphors)
  SbMediaPrimaryId primaries;

  // [Color Space field] The transfer characteristics of the
  // video. For clarity, the value and meanings for
  // TransferCharacteristics 1-15 are adopted from Table 3 of ISO/IEC
  // 23001-8:2013/DCOR1. TransferCharacteristics 16-18 are proposed
  // values. (0: Reserved, 1: ITU-R BT.709, 2: Unspecified, 3:
  // Reserved, 4: Gamma 2.2 curve, 5: Gamma 2.8 curve, 6: SMPTE 170M,
  // 7: SMPTE 240M, 8: Linear, 9: Log, 10: Log Sqrt, 11: IEC
  // 61966-2-4, 12: ITU-R BT.1361 Extended Colour Gamut, 13: IEC
  // 61966-2-1, 14: ITU-R BT.2020 10 bit, 15: ITU-R BT.2020 12 bit,
  // 16: SMPTE ST 2084, 17: SMPTE ST 428-1 18: ARIB STD-B67 (HLG))
  SbMediaTransferId transfer;

  // [Color Space field] The Matrix Coefficients of the video used to
  // derive luma and chroma values from red, green, and blue color
  // primaries. For clarity, the value and meanings for
  // MatrixCoefficients are adopted from Table 4 of ISO/IEC
  // 23001-8:2013/DCOR1. (0:GBR, 1: BT709, 2: Unspecified, 3:
  // Reserved, 4: FCC, 5: BT470BG, 6: SMPTE 170M, 7: SMPTE 240M, 8:
  // YCOCG, 9: BT2020 Non-constant Luminance, 10: BT2020 Constant
  // Luminance)
  SbMediaMatrixId matrix;

  // [Color Space field] Clipping of the color ranges. (0:
  // Unspecified, 1: Broadcast Range, 2: Full range (no clipping), 3:
  // Defined by MatrixCoefficients/TransferCharacteristics)
  SbMediaRangeId range;

  // [Color Space field] Only used if primaries ==
  // kSbMediaPrimaryIdCustom.  This a row-major ordered 3 x 4
  // submatrix of the 4 x 4 transform matrix.  The 4th row is
  // completed as (0, 0, 0, 1).
  float custom_primary_matrix[12];
} SbMediaColorMetadata;

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

  // HDR metadata common for HDR10 and WebM/VP9-based HDR formats as
  // well as the Color Space, and Color elements: MatrixCoefficients,
  // BitsPerChannel, ChromaSubsamplingHorz, ChromaSubsamplingVert,
  // CbSubsamplingHorz, CbSubsamplingVert, ChromaSitingHorz,
  // ChromaSitingVert, Range, TransferCharacteristics, and Primaries
  // described here: https://matroska.org/technical/specs/index.html .
  // This will only be specified on frames where the HDR metadata and
  // color / color space might have changed (e.g. keyframes).
  SbMediaColorMetadata* color_metadata;
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
#if SB_API_VERSION >= 6
  const void* audio_specific_config;
#else   // SB_API_VERSION >= 6
  int8_t audio_specific_config[8];
#endif  // SB_API_VERSION >= 6
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

// Returns information about whether the playback of the specific media
// described by |mime| and encrypted using |key_system| can be played.
//
// Note that neither |mime| nor |key_system| can be NULL. This function returns
// |kSbMediaSupportNotSupported| if either is NULL.
//
// |mime|: The mime information of the media in the form of |video/webm|
//   or |video/mp4; codecs="avc1.42001E"|. It may include arbitrary parameters
//   like "codecs", "channels", etc.  Note that the "codecs" parameter may
//   contain more than one codec, delimited by comma.
// |key_system|: A lowercase value in fhe form of "com.example.somesystem"
//   as suggested by https://w3c.github.io/encrypted-media/#key-system
//   that can be matched exactly with known DRM key systems of the platform.
//   When |key_system| is an empty string, the return value is an indication for
//   non-encrypted media.
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
