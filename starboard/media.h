// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
  kSbMediaVideoCodecAv1,
  kSbMediaVideoCodecVp8,
  kSbMediaVideoCodecVp9,
} SbMediaVideoCodec;

// Types of audio elementary streams that can be supported.
typedef enum SbMediaAudioCodec {
  kSbMediaAudioCodecNone,

  kSbMediaAudioCodecAac,
  kSbMediaAudioCodecAc3,
  kSbMediaAudioCodecEac3,
  kSbMediaAudioCodecOpus,
  kSbMediaAudioCodecVorbis,
} SbMediaAudioCodec;

// Indicates how confident the device is that it can play media resources of the
// given type. The values are a direct map of the canPlayType() method specified
// at the following link:
// https://www.w3.org/TR/2011/WD-html5-20110113/video.html#dom-navigator-canplaytype
typedef enum SbMediaSupportType {
  // The media type cannot be played.
  kSbMediaSupportTypeNotSupported,

  // Cannot determine if the media type is playable without playing it.
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
  kSbMediaAudioSampleTypeInt16Deprecated,
  kSbMediaAudioSampleTypeFloat32,
#if SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
  kSbMediaAudioSampleTypeInt16 = kSbMediaAudioSampleTypeInt16Deprecated,
#endif  // SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
} SbMediaAudioSampleType;

// Possible audio frame storage types.
typedef enum SbMediaAudioFrameStorageType {
  // The samples of a multi-channel audio stream are stored in one continuous
  // buffer. Samples at the same timestamp are stored one after another. For
  // example, for a stereo stream with channels L and R that contains samples
  // with timestamps 0, 1, 2, etc., the samples are stored in one buffer as
  // "L0 R0 L1 R1 L2 R2 ...".
  kSbMediaAudioFrameStorageTypeInterleaved,

  // The samples of each channel are stored in their own continuous buffer.  For
  // example, for a stereo stream with channels L and R that contains samples
  // with timestamps 0, 1, 2, etc., the samples are stored in two buffers
  // "L0 L1 L2 ..." and "R0 R1 R2 ...".
  kSbMediaAudioFrameStorageTypePlanar,
} SbMediaAudioFrameStorageType;

// SMPTE 2086 mastering data
//   http://ieeexplore.ieee.org/document/7291707/
// This standard specifies the metadata items to specify the color volume (the
// color primaries, white point, and luminance range) of the display that was
// used in mastering video content. The metadata is specified as a set of values
// independent of any specific digital representation.
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

  // Maximum luminance. Shall be represented in candelas per square meter
  // (cd/m^2). In range [0, 9999.99].
  float luminance_max;

  // Minimum luminance. Shall be represented in candelas per square meter
  // (cd/m^2). In range [0, 9999.99].
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

  // This is an ad-hoc transfer function that decodes SMPTE 2084 content into a
  // 0-1 range more or less suitable for viewing on a non-hdr display.
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

// This corresponds to the WebM Range enum which is part of WebM color data (see
// http://www.webmproject.org/docs/container/#Range).
// H.264 only uses a bool, which corresponds to the LIMITED/FULL values.
// Chrome-specific values start at 1000.
typedef enum SbMediaRangeId {
  // Range is not explicitly specified / unknown.
  kSbMediaRangeIdUnspecified = 0,

  // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
  kSbMediaRangeIdLimited = 1,

  // Full RGB color range with RGB values from 0 to 255.
  kSbMediaRangeIdFull = 2,

  // Range is defined by TransferId/MatrixId.
  kSbMediaRangeIdDerived = 3,

  kSbMediaRangeIdLast = kSbMediaRangeIdDerived
} SbMediaRangeId;

// HDR (High Dynamic Range) Metadata common for HDR10 and WebM/VP9-based HDR
// formats, together with the ColorSpace. HDR reproduces a greater dynamic range
// of luminosity than is possible with standard digital imaging. See the
// Consumer Electronics Association press release:
// https://www.cta.tech/News/Press-Releases/2015/August/CEA-Defines-%E2%80%98HDR-Compatible%E2%80%99-Displays.aspx
typedef struct SbMediaColorMetadata {
  // Number of decoded bits per channel. A value of 0 indicates that the
  // BitsPerChannel is unspecified.
  unsigned int bits_per_channel;

  // The amount of pixels to remove in the Cr and Cb channels for every pixel
  // not removed horizontally. Example: For video with 4:2:0 chroma subsampling,
  // the |chroma_subsampling_horizontal| should be set to 1.
  unsigned int chroma_subsampling_horizontal;

  // The amount of pixels to remove in the Cr and Cb channels for every pixel
  // not removed vertically. Example: For video with 4:2:0 chroma subsampling,
  // the |chroma_subsampling_vertical| should be set to 1.
  unsigned int chroma_subsampling_vertical;

  // The amount of pixels to remove in the Cb channel for every pixel not
  // removed horizontally. This is additive with ChromaSubsamplingHorz. Example:
  // For video with 4:2:1 chroma subsampling, the
  // |chroma_subsampling_horizontal| should be set to 1 and
  // |cb_subsampling_horizontal| should be set to 1.
  unsigned int cb_subsampling_horizontal;

  // The amount of pixels to remove in the Cb channel for every pixel not
  // removed vertically. This is additive with |chroma_subsampling_vertical|.
  unsigned int cb_subsampling_vertical;

  // How chroma is subsampled horizontally. (0: Unspecified, 1: Left Collocated,
  // 2: Half).
  unsigned int chroma_siting_horizontal;

  // How chroma is subsampled vertically. (0: Unspecified, 1: Top Collocated, 2:
  // Half).
  unsigned int chroma_siting_vertical;

  // [HDR Metadata field] SMPTE 2086 mastering data.
  SbMediaMasteringMetadata mastering_metadata;

  // [HDR Metadata field] Maximum brightness of a single pixel (Maximum Content
  // Light Level) in candelas per square meter (cd/m^2).
  unsigned int max_cll;

  // [HDR Metadata field] Maximum brightness of a single full frame (Maximum
  // Frame-Average Light Level) in candelas per square meter (cd/m^2).
  unsigned int max_fall;

  // [Color Space field] The colour primaries of the video. For clarity, the
  // value and meanings for Primaries are adopted from Table 2 of
  // ISO/IEC 23001-8:2013/DCOR1. (0: Reserved, 1: ITU-R BT.709, 2: Unspecified,
  // 3: Reserved, 4: ITU-R BT.470M, 5: ITU-R BT.470BG, 6: SMPTE 170M,
  // 7: SMPTE 240M, 8: FILM, 9: ITU-R BT.2020, 10: SMPTE ST 428-1,
  // 22: JEDEC P22 phosphors).
  SbMediaPrimaryId primaries;

  // [Color Space field] The transfer characteristics of the video. For clarity,
  // the value and meanings for TransferCharacteristics 1-15 are adopted from
  // Table 3 of ISO/IEC 23001-8:2013/DCOR1. TransferCharacteristics 16-18 are
  // proposed values. (0: Reserved, 1: ITU-R BT.709, 2: Unspecified,
  // 3: Reserved, 4: Gamma 2.2 curve, 5: Gamma 2.8 curve, 6: SMPTE 170M,
  // 7: SMPTE 240M, 8: Linear, 9: Log, 10: Log Sqrt, 11: IEC 61966-2-4,
  // 12: ITU-R BT.1361 Extended Colour Gamut, 13: IEC 61966-2-1,
  // 14: ITU-R BT.2020 10 bit, 15: ITU-R BT.2020 12 bit, 16: SMPTE ST 2084,
  // 17: SMPTE ST 428-1 18: ARIB STD-B67 (HLG)).
  SbMediaTransferId transfer;

  // [Color Space field] The Matrix Coefficients of the video used to derive
  // luma and chroma values from red, green, and blue color primaries. For
  // clarity, the value and meanings for MatrixCoefficients are adopted from
  // Table 4 of ISO/IEC 23001-8:2013/DCOR1. (0:GBR, 1: BT709, 2: Unspecified,
  // 3: Reserved, 4: FCC, 5: BT470BG, 6: SMPTE 170M, 7: SMPTE 240M, 8: YCOCG,
  // 9: BT2020 Non-constant Luminance, 10: BT2020 Constant Luminance).
  SbMediaMatrixId matrix;

  // [Color Space field] Clipping of the color ranges. (0: Unspecified,
  // 1: Broadcast Range, 2: Full range (no clipping), 3: Defined by
  // MatrixCoefficients/TransferCharacteristics).
  SbMediaRangeId range;

  // [Color Space field] Only used if primaries == kSbMediaPrimaryIdCustom.
  //  This a row-major ordered 3 x 4 submatrix of the 4 x 4 transform matrix.
  // The 4th row is completed as (0, 0, 0, 1).
  float custom_primary_matrix[12];
} SbMediaColorMetadata;

// The set of information required by the decoder or player for each video
// sample.
typedef struct SbMediaVideoSampleInfo {
  // The video codec of this sample.
  SbMediaVideoCodec codec;

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  // The mime of the video stream when |codec| isn't kSbMediaVideoCodecNone.  It
  // may point to an empty string if the mime is not available, and it can only
  // be set to NULL when |codec| is kSbMediaVideoCodecNone.
  const char* mime;

  // Indicates the max video capabilities required. The web app will not provide
  // a video stream exceeding the maximums described by this parameter. Allows
  // the platform to optimize playback pipeline for low quality video streams if
  // it knows that it will never adapt to higher quality streams. The string
  // uses the same format as the string passed in to
  // SbMediaCanPlayMimeAndKeySystem(), for example, when it is set to
  // "width=1920; height=1080; framerate=15;", the video will never adapt to
  // resolution higher than 1920x1080 or frame per second higher than 15 fps.
  // When the maximums are unknown, this will be set to an empty string.  It can
  // only be set to NULL when |codec| is kSbMediaVideoCodecNone.
  const char* max_video_capabilities;
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  // Indicates whether the associated sample is a key frame (I-frame). Video key
  // frames must always start with SPS and PPS NAL units.
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
  SbMediaColorMetadata color_metadata;
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

// An audio sample info, which is a description of a given audio sample.  This
// acts as a set of instructions to the audio decoder.
//
// The audio sample info consists of information found in the |WAVEFORMATEX|
// structure, as well as other information for the audio decoder, including the
// Audio-specific configuration field.  The |WAVEFORMATEX| structure is
// specified at http://msdn.microsoft.com/en-us/library/dd390970(v=vs.85).aspx.
typedef struct SbMediaAudioSampleInfo {
  // The audio codec of this sample.
  SbMediaAudioCodec codec;

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  // The mime of the audio stream when |codec| isn't kSbMediaAudioCodecNone.  It
  // may point to an empty string if the mime is not available, and it can only
  // be set to NULL when |codec| is kSbMediaAudioCodecNone.
  const char* mime;
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

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
  const void* audio_specific_config;
} SbMediaAudioSampleInfo;

// --- Functions -------------------------------------------------------------

#if SB_API_VERSION < 13
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
#endif  // SB_API_VERSION < 13

// Returns information about whether the playback of the specific media
// described by |mime| and encrypted using |key_system| can be played.
//
// Note that neither |mime| nor |key_system| can be NULL. This function returns
// |kSbMediaSupportNotSupported| if either is NULL.
//
// |mime|: The mime information of the media in the form of |video/webm| or
//   |video/mp4; codecs="avc1.42001E"|. It may include arbitrary parameters like
//   "codecs", "channels", etc.  Note that the "codecs" parameter may contain
//   more than one codec, delimited by comma.
// |key_system|: A lowercase value in the form of "com.example.somesystem" as
//   suggested by https://w3c.github.io/encrypted-media/#key-system that can be
//   matched exactly with known DRM key systems of the platform.  When
//   |key_system| is an empty string, the return value is an indication for
//   non-encrypted media.
//
//   An implementation may choose to support |key_system| with extra attributes,
//   separated by ';', like
//   |com.example.somesystem; attribute_name1="value1"; attribute_name2=value1|.
//   If |key_system| with attributes is not supported by an implementation, it
//   should treat |key_system| as if it contains only the key system, and reject
//   any input containing extra attributes, i.e. it can keep using its existing
//   implementation.
//   When an implementation supports |key_system| with attributes, it has to
//   support all attributes defined by the Starboard version the implementation
//   uses.
//   An implementation should ignore any unknown attributes, and make a decision
//   solely based on the key system and the known attributes.  For example, if
//   an implementation supports "com.widevine.alpha", it should also return
//   `kSbMediaSupportTypeProbably` when |key_system| is
//   |com.widevine.alpha; invalid_attribute="invalid_value"|.
//   Currently the only attribute has to be supported is |encryptionscheme|.  It
//   reflects the value passed to `encryptionScheme` of
//   MediaKeySystemMediaCapability, as defined in
//   https://wicg.github.io/encrypted-media-encryption-scheme/, which can take
//   value "cenc", "cbcs", or "cbcs-1-9".
//   Empty string is not a valid value for |encryptionscheme| and the
//   implementation should return `kSbMediaSupportTypeNotSupported` when
//   |encryptionscheme| is set to "".
//   The implementation should return `kSbMediaSupportTypeNotSupported` for
//   unknown values of known attributes.  For example, if an implementation
//   supports "encryptionscheme" with value "cenc", "cbcs", or "cbcs-1-9", then
//   it should return `kSbMediaSupportTypeProbably` when |key_system| is
//   |com.widevine.alpha; encryptionscheme="cenc"|, and return
//   `kSbMediaSupportTypeNotSupported` when |key_system| is
//   |com.widevine.alpha; encryptionscheme="invalid"|.
//   If an implementation supports key system with attributes on one key system,
//   it has to support key system with attributes on all key systems supported.
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

#if SB_API_VERSION < 12
// Indicates whether output copy protection is currently enabled on all capable
// outputs. If |true|, then non-protection-capable outputs are expected to be
// blanked.
//
// presubmit: allow sb_export mismatch
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
//
// presubmit: allow sb_export mismatch
SB_EXPORT bool SbMediaSetOutputProtection(bool enabled);
#endif  // SB_API_VERSION < 12

// Value used when a video's resolution is not known.
#define kSbMediaVideoResolutionDimensionInvalid 0
// Value used when a video's bits per pixel is not known.
#define kSbMediaBitsPerPixelInvalid 0

typedef enum SbMediaBufferStorageType {
  kSbMediaBufferStorageTypeMemory,
  kSbMediaBufferStorageTypeFile,
} SbMediaBufferStorageType;

// The media buffer will be allocated using the returned alignment.  Set this to
// a larger value may increase the memory consumption of media buffers.
//
// |type|: the media type of the stream (audio or video).
SB_EXPORT int SbMediaGetBufferAlignment(SbMediaType type);

// When the media stack needs more memory to store media buffers, it will
// allocate extra memory in units returned by SbMediaGetBufferAllocationUnit.
// This can return 0, in which case the media stack will allocate extra memory
// on demand.  When SbMediaGetInitialBufferCapacity and this function both
// return 0, the media stack will allocate individual buffers directly using
// SbMemory functions.
SB_EXPORT int SbMediaGetBufferAllocationUnit();

// Specifies the maximum amount of memory used by audio buffers of media source
// before triggering a garbage collection.  A large value will cause more memory
// being used by audio buffers but will also make the app less likely to
// re-download audio data.  Note that the app may experience significant
// difficulty if this value is too low.
SB_EXPORT int SbMediaGetAudioBufferBudget();

// Specifies the duration threshold of media source garbage collection.  When
// the accumulated duration in a source buffer exceeds this value, the media
// source implementation will try to eject existing buffers from the cache. This
// is usually triggered when the video being played has a simple content and the
// encoded data is small.  In such case this can limit how much is allocated for
// the book keeping data of the media buffers and avoid OOM of system heap. This
// should return 170 seconds for most of the platforms.  But it can be further
// reduced on systems with extremely low memory.
SB_EXPORT SbTime SbMediaGetBufferGarbageCollectionDurationThreshold();

// The amount of memory that will be used to store media buffers allocated
// during system startup.  To allocate a large chunk at startup helps with
// reducing fragmentation and can avoid failures to allocate incrementally. This
// can return 0.
SB_EXPORT int SbMediaGetInitialBufferCapacity();

// The maximum amount of memory that will be used to store media buffers. This
// must be larger than sum of the video budget and audio budget.
//
// |codec|: the video codec associated with the buffer.
// |resolution_width|: the width of the video resolution.
// |resolution_height|: the height of the video resolution.
// |bits_per_pixel|: the bits per pixel. This value is larger for HDR than non-
//   HDR video.
SB_EXPORT int SbMediaGetMaxBufferCapacity(SbMediaVideoCodec codec,
                                          int resolution_width,
                                          int resolution_height,
                                          int bits_per_pixel);

// Extra bytes allocated at the end of a media buffer to ensure that the buffer
// can be use optimally by specific instructions like SIMD.  Set to 0 to remove
// any padding.
//
// |type|: the media type of the stream (audio or video).
SB_EXPORT int SbMediaGetBufferPadding(SbMediaType type);

// When either SbMediaGetInitialBufferCapacity or SbMediaGetBufferAllocationUnit
// isn't zero, media buffers will be allocated using a memory pool.  Set the
// following variable to true to allocate the media buffer pool memory on demand
// and return all memory to the system when there is no media buffer allocated.
// Setting the following value to false results in that Cobalt will allocate
// SbMediaGetInitialBufferCapacity bytes for media buffer on startup and will
// not release any media buffer memory back to the system even if there is no
// media buffers allocated.
SB_EXPORT bool SbMediaIsBufferPoolAllocateOnDemand();

// The memory used when playing mp4 videos that is not in DASH format.  The
// resolution of such videos shouldn't go beyond 1080p.  Its value should be
// less than the sum of SbMediaGetAudioBufferBudget and
// 'SbMediaGetVideoBufferBudget(..., 1920, 1080, ...) but not less than 8 MB.
//
// |codec|: the video codec associated with the buffer.
// |resolution_width|: the width of the video resolution.
// |resolution_height|: the height of the video resolution.
// |bits_per_pixel|: the bits per pixel. This value is larger for HDR than non-
//   HDR video.
SB_EXPORT int SbMediaGetProgressiveBufferBudget(SbMediaVideoCodec codec,
                                                int resolution_width,
                                                int resolution_height,
                                                int bits_per_pixel);

// Returns SbMediaBufferStorageType of type |SbMediaStorageTypeMemory| or
// |SbMediaStorageTypeFile|. For memory storage, the media buffers will be
// stored in main memory allocated by SbMemory functions.  For file storage, the
// media buffers will be stored in a temporary file in the system cache folder
// acquired by calling SbSystemGetPath() with "kSbSystemPathCacheDirectory".
// Note that when its value is "file" the media stack will still allocate memory
// to cache the the buffers in use.
SB_EXPORT SbMediaBufferStorageType SbMediaGetBufferStorageType();

// If SbMediaGetBufferUsingMemoryPool returns true, it indicates that media
// buffer pools should be allocated on demand, as opposed to using SbMemory*
// functions.
SB_EXPORT bool SbMediaIsBufferUsingMemoryPool();

// Specifies the maximum amount of memory used by video buffers of media source
// before triggering a garbage collection.  A large value will cause more memory
// being used by video buffers but will also make app less likely to re-download
// video data.  Note that the app may experience significant difficulty if this
// value is too low.
//
// |codec|: the video codec associated with the buffer.
// |resolution_width|: the width of the video resolution.
// |resolution_height|: the height of the video resolution.
// |bits_per_pixel|: the bits per pixel. This value is larger for HDR than non-
//   HDR video.
SB_EXPORT int SbMediaGetVideoBufferBudget(SbMediaVideoCodec codec,
                                          int resolution_width,
                                          int resolution_height,
                                          int bits_per_pixel);

// Communicate to the platform how far past |current_playback_position| the app
// will write audio samples. The app will write all samples between
// |current_playback_position| and |current_playback_position| + |duration|, as
// soon as they are available. The app may sometimes write more samples than
// that, but the app only guarantees to write |duration| past
// |current_playback_position| in general. The platform is responsible for
// guaranteeing that when only |duration| audio samples are written at a time,
// no playback issues occur (such as transient or indefinite hanging). The
// platform may assume |duration| >= 0.5 seconds.
SB_EXPORT void SbMediaSetAudioWriteDuration(SbTime duration);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_MEDIA_H_
