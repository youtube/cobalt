// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
//
// Contains extension code allowing partners to provide their own demuxer.
// CobaltExtensionDemuxerApi is the main API.

#ifndef STARBOARD_EXTENSION_DEMUXER_H_
#define STARBOARD_EXTENSION_DEMUXER_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "starboard/time.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionDemuxerApi "dev.cobalt.extension.Demuxer"

// This must stay in sync with ::media::PipelineStatus. Missing values are
// either irrelevant to the demuxer or are deprecated values of PipelineStatus.
typedef enum CobaltExtensionDemuxerStatus {
  kCobaltExtensionDemuxerOk = 0,
  kCobaltExtensionDemuxerErrorNetwork = 2,
  kCobaltExtensionDemuxerErrorAbort = 5,
  kCobaltExtensionDemuxerErrorInitializationFailed = 6,
  kCobaltExtensionDemuxerErrorRead = 9,
  kCobaltExtensionDemuxerErrorInvalidState = 11,
  kCobaltExtensionDemuxerErrorCouldNotOpen = 12,
  kCobaltExtensionDemuxerErrorCouldNotParse = 13,
  kCobaltExtensionDemuxerErrorNoSupportedStreams = 14
} CobaltExtensionDemuxerStatus;

// Type of side data associated with a buffer.
typedef enum CobaltExtensionDemuxerSideDataType {
  kCobaltExtensionDemuxerUnknownSideDataType = 0,
  kCobaltExtensionDemuxerMatroskaBlockAdditional = 1,
} CobaltExtensionDemuxerSideDataType;

// This must stay in sync with ::media::AudioCodec.
typedef enum CobaltExtensionDemuxerAudioCodec {
  kCobaltExtensionDemuxerCodecUnknownAudio = 0,
  kCobaltExtensionDemuxerCodecAAC = 1,
  kCobaltExtensionDemuxerCodecMP3 = 2,
  kCobaltExtensionDemuxerCodecPCM = 3,
  kCobaltExtensionDemuxerCodecVorbis = 4,
  kCobaltExtensionDemuxerCodecFLAC = 5,
  kCobaltExtensionDemuxerCodecAMR_NB = 6,
  kCobaltExtensionDemuxerCodecAMR_WB = 7,
  kCobaltExtensionDemuxerCodecPCM_MULAW = 8,
  kCobaltExtensionDemuxerCodecGSM_MS = 9,
  kCobaltExtensionDemuxerCodecPCM_S16BE = 10,
  kCobaltExtensionDemuxerCodecPCM_S24BE = 11,
  kCobaltExtensionDemuxerCodecOpus = 12,
  kCobaltExtensionDemuxerCodecEAC3 = 13,
  kCobaltExtensionDemuxerCodecPCM_ALAW = 14,
  kCobaltExtensionDemuxerCodecALAC = 15,
  kCobaltExtensionDemuxerCodecAC3 = 16
} CobaltExtensionDemuxerAudioCodec;

// This must stay in sync with ::media::VideoCodec.
typedef enum CobaltExtensionDemuxerVideoCodec {
  kCobaltExtensionDemuxerCodecUnknownVideo = 0,
  kCobaltExtensionDemuxerCodecH264,
  kCobaltExtensionDemuxerCodecVC1,
  kCobaltExtensionDemuxerCodecMPEG2,
  kCobaltExtensionDemuxerCodecMPEG4,
  kCobaltExtensionDemuxerCodecTheora,
  kCobaltExtensionDemuxerCodecVP8,
  kCobaltExtensionDemuxerCodecVP9,
  kCobaltExtensionDemuxerCodecHEVC,
  kCobaltExtensionDemuxerCodecDolbyVision,
  kCobaltExtensionDemuxerCodecAV1,
} CobaltExtensionDemuxerVideoCodec;

// This must stay in sync with ::media::SampleFormat.
typedef enum CobaltExtensionDemuxerSampleFormat {
  kCobaltExtensionDemuxerSampleFormatUnknown = 0,
  kCobaltExtensionDemuxerSampleFormatU8,   // Unsigned 8-bit w/ bias of 128.
  kCobaltExtensionDemuxerSampleFormatS16,  // Signed 16-bit.
  kCobaltExtensionDemuxerSampleFormatS32,  // Signed 32-bit.
  kCobaltExtensionDemuxerSampleFormatF32,  // Float 32-bit.
  kCobaltExtensionDemuxerSampleFormatPlanarS16,  // Signed 16-bit planar.
  kCobaltExtensionDemuxerSampleFormatPlanarF32,  // Float 32-bit planar.
  kCobaltExtensionDemuxerSampleFormatPlanarS32,  // Signed 32-bit planar.
  kCobaltExtensionDemuxerSampleFormatS24,        // Signed 24-bit.
} CobaltExtensionDemuxerSampleFormat;

// This must stay in sync with ::media::ChannelLayout.
typedef enum CobaltExtensionDemuxerChannelLayout {
  kCobaltExtensionDemuxerChannelLayoutNone = 0,
  kCobaltExtensionDemuxerChannelLayoutUnsupported = 1,
  kCobaltExtensionDemuxerChannelLayoutMono = 2,
  kCobaltExtensionDemuxerChannelLayoutStereo = 3,
  kCobaltExtensionDemuxerChannelLayout2_1 = 4,
  kCobaltExtensionDemuxerChannelLayoutSurround = 5,
  kCobaltExtensionDemuxerChannelLayout4_0 = 6,
  kCobaltExtensionDemuxerChannelLayout2_2 = 7,
  kCobaltExtensionDemuxerChannelLayoutQuad = 8,
  kCobaltExtensionDemuxerChannelLayout5_0 = 9,
  kCobaltExtensionDemuxerChannelLayout5_1 = 10,
  kCobaltExtensionDemuxerChannelLayout5_0Back = 11,
  kCobaltExtensionDemuxerChannelLayout5_1Back = 12,
  kCobaltExtensionDemuxerChannelLayout7_0 = 13,
  kCobaltExtensionDemuxerChannelLayout7_1 = 14,
  kCobaltExtensionDemuxerChannelLayout7_1Wide = 15,
  kCobaltExtensionDemuxerChannelLayoutStereoDownmix = 16,
  kCobaltExtensionDemuxerChannelLayout2point1 = 17,
  kCobaltExtensionDemuxerChannelLayout3_1 = 18,
  kCobaltExtensionDemuxerChannelLayout4_1 = 19,
  kCobaltExtensionDemuxerChannelLayout6_0 = 20,
  kCobaltExtensionDemuxerChannelLayout6_0Front = 21,
  kCobaltExtensionDemuxerChannelLayoutHexagonal = 22,
  kCobaltExtensionDemuxerChannelLayout6_1 = 23,
  kCobaltExtensionDemuxerChannelLayout6_1Back = 24,
  kCobaltExtensionDemuxerChannelLayout6_1Front = 25,
  kCobaltExtensionDemuxerChannelLayout7_0Front = 26,
  kCobaltExtensionDemuxerChannelLayout7_1WideBack = 27,
  kCobaltExtensionDemuxerChannelLayoutOctagonal = 28,
  kCobaltExtensionDemuxerChannelLayoutDiscrete = 29,
  kCobaltExtensionDemuxerChannelLayoutStereoAndKeyboardMic = 30,
  kCobaltExtensionDemuxerChannelLayout4_1QuadSide = 31,
  kCobaltExtensionDemuxerChannelLayoutBitstream = 32
} CobaltExtensionDemuxerChannelLayout;

// This must stay in sync with ::media::VideoCodecProfile.
typedef enum CobaltExtensionDemuxerVideoCodecProfile {
  kCobaltExtensionDemuxerVideoCodecProfileUnknown = -1,
  kCobaltExtensionDemuxerH264ProfileMin = 0,
  kCobaltExtensionDemuxerH264ProfileBaseline =
      kCobaltExtensionDemuxerH264ProfileMin,
  kCobaltExtensionDemuxerH264ProfileMain = 1,
  kCobaltExtensionDemuxerH264ProfileExtended = 2,
  kCobaltExtensionDemuxerH264ProfileHigh = 3,
  kCobaltExtensionDemuxerH264ProfileHigh10Profile = 4,
  kCobaltExtensionDemuxerH264ProfileHigh422Profile = 5,
  kCobaltExtensionDemuxerH264ProfileHigh444PredictiveProfile = 6,
  kCobaltExtensionDemuxerH264ProfileScalableBaseline = 7,
  kCobaltExtensionDemuxerH264ProfileScalableHigh = 8,
  kCobaltExtensionDemuxerH264ProfileStereoHigh = 9,
  kCobaltExtensionDemuxerH264ProfileMultiviewHigh = 10,
  kCobaltExtensionDemuxerH264ProfileMax =
      kCobaltExtensionDemuxerH264ProfileMultiviewHigh,
  kCobaltExtensionDemuxerVp8ProfileMin = 11,
  kCobaltExtensionDemuxerVp8ProfileAny = kCobaltExtensionDemuxerVp8ProfileMin,
  kCobaltExtensionDemuxerVp8ProfileMax = kCobaltExtensionDemuxerVp8ProfileAny,
  kCobaltExtensionDemuxerVp9ProfileMin = 12,
  kCobaltExtensionDemuxerVp9ProfileProfile0 =
      kCobaltExtensionDemuxerVp9ProfileMin,
  kCobaltExtensionDemuxerVp9ProfileProfile1 = 13,
  kCobaltExtensionDemuxerVp9ProfileProfile2 = 14,
  kCobaltExtensionDemuxerVp9ProfileProfile3 = 15,
  kCobaltExtensionDemuxerVp9ProfileMax =
      kCobaltExtensionDemuxerVp9ProfileProfile3,
  kCobaltExtensionDemuxerHevcProfileMin = 16,
  kCobaltExtensionDemuxerHevcProfileMain =
      kCobaltExtensionDemuxerHevcProfileMin,
  kCobaltExtensionDemuxerHevcProfileMain10 = 17,
  kCobaltExtensionDemuxerHevcProfileMainStillPicture = 18,
  kCobaltExtensionDemuxerHevcProfileMax =
      kCobaltExtensionDemuxerHevcProfileMainStillPicture,
  kCobaltExtensionDemuxerDolbyVisionProfile0 = 19,
  kCobaltExtensionDemuxerDolbyVisionProfile4 = 20,
  kCobaltExtensionDemuxerDolbyVisionProfile5 = 21,
  kCobaltExtensionDemuxerDolbyVisionProfile7 = 22,
  kCobaltExtensionDemuxerTheoraProfileMin = 23,
  kCobaltExtensionDemuxerTheoraProfileAny =
      kCobaltExtensionDemuxerTheoraProfileMin,
  kCobaltExtensionDemuxerTheoraProfileMax =
      kCobaltExtensionDemuxerTheoraProfileAny,
  kCobaltExtensionDemuxerAv1ProfileMin = 24,
  kCobaltExtensionDemuxerAv1ProfileProfileMain =
      kCobaltExtensionDemuxerAv1ProfileMin,
  kCobaltExtensionDemuxerAv1ProfileProfileHigh = 25,
  kCobaltExtensionDemuxerAv1ProfileProfilePro = 26,
  kCobaltExtensionDemuxerAv1ProfileMax =
      kCobaltExtensionDemuxerAv1ProfileProfilePro,
  kCobaltExtensionDemuxerDolbyVisionProfile8 = 27,
  kCobaltExtensionDemuxerDolbyVisionProfile9 = 28,
} CobaltExtensionDemuxerVideoCodecProfile;

// This must be kept in sync with gfx::ColorSpace::RangeID.
typedef enum CobaltExtensionDemuxerColorSpaceRangeId {
  kCobaltExtensionDemuxerColorSpaceRangeIdInvalid = 0,
  kCobaltExtensionDemuxerColorSpaceRangeIdLimited = 1,
  kCobaltExtensionDemuxerColorSpaceRangeIdFull = 2,
  kCobaltExtensionDemuxerColorSpaceRangeIdDerived = 3
} CobaltExtensionDemuxerColorSpaceRangeId;

// This must be kept in sync with media::VideoDecoderConfig::AlphaMode.
typedef enum CobaltExtensionDemuxerAlphaMode {
  kCobaltExtensionDemuxerHasAlpha,
  kCobaltExtensionDemuxerIsOpaque
} CobaltExtensionDemuxerAlphaMode;

// This must be kept in sync with ::media::DemuxerStream::Type.
typedef enum CobaltExtensionDemuxerStreamType {
  kCobaltExtensionDemuxerStreamTypeUnknown,
  kCobaltExtensionDemuxerStreamTypeAudio,
  kCobaltExtensionDemuxerStreamTypeVideo,
  kCobaltExtensionDemuxerStreamTypeText
} CobaltExtensionDemuxerStreamType;

// This must be kept in sync with media::EncryptionScheme.
typedef enum CobaltExtensionDemuxerEncryptionScheme {
  kCobaltExtensionDemuxerEncryptionSchemeUnencrypted,
  kCobaltExtensionDemuxerEncryptionSchemeCenc,
  kCobaltExtensionDemuxerEncryptionSchemeCbcs,
} CobaltExtensionDemuxerEncryptionScheme;

typedef struct CobaltExtensionDemuxerAudioDecoderConfig {
  CobaltExtensionDemuxerAudioCodec codec;
  CobaltExtensionDemuxerSampleFormat sample_format;
  CobaltExtensionDemuxerChannelLayout channel_layout;
  CobaltExtensionDemuxerEncryptionScheme encryption_scheme;
  int samples_per_second;

  uint8_t* extra_data;  // Not owned by this struct.
  int64_t extra_data_size;
} CobaltExtensionDemuxerAudioDecoderConfig;

typedef struct CobaltExtensionDemuxerVideoDecoderConfig {
  CobaltExtensionDemuxerVideoCodec codec;
  CobaltExtensionDemuxerVideoCodecProfile profile;

  // These fields represent the color space.
  int color_space_primaries;
  int color_space_transfer;
  int color_space_matrix;
  CobaltExtensionDemuxerColorSpaceRangeId color_space_range_id;

  CobaltExtensionDemuxerAlphaMode alpha_mode;

  // These fields represent the coded size.
  int coded_width;
  int coded_height;

  // These fields represent the visible rectangle.
  int visible_rect_x;
  int visible_rect_y;
  int visible_rect_width;
  int visible_rect_height;

  // These fields represent the natural size.
  int natural_width;
  int natural_height;

  CobaltExtensionDemuxerEncryptionScheme encryption_scheme;

  uint8_t* extra_data;  // Not owned by this struct.
  int64_t extra_data_size;
} CobaltExtensionDemuxerVideoDecoderConfig;

typedef struct CobaltExtensionDemuxerSideData {
  uint8_t* data;  // Not owned by this struct.
  // Number of bytes in |data|.
  int64_t data_size;
  // Specifies the format of |data|.
  CobaltExtensionDemuxerSideDataType type;
} CobaltExtensionDemuxerSideData;

typedef struct CobaltExtensionDemuxerBuffer {
  // The media data for this buffer. Ownership is not transferred via this
  // struct.
  uint8_t* data;
  // Number of bytes in |data|.
  int64_t data_size;
  // An array of side data elements containing any side data for this buffer.
  // Ownership is not transferred via this struct.
  CobaltExtensionDemuxerSideData* side_data;
  // Number of elements in |side_data|.
  int64_t side_data_elements;
  // Playback time in microseconds.
  SbTime pts;
  // Duration of this buffer in microseconds.
  SbTime duration;
  // True if this buffer contains a keyframe.
  bool is_keyframe;
  // Signifies the end of the stream. If this is true, the other fields will be
  // ignored.
  bool end_of_stream;
} CobaltExtensionDemuxerBuffer;

// Note: |buffer| is the input to this function, not the output. Cobalt
// implements this function to read media data provided by the implementer of
// CobaltExtensionDemuxer.
typedef void (*CobaltExtensionDemuxerReadCB)(
    CobaltExtensionDemuxerBuffer* buffer,
    void* user_data);

// A fully synchronous demuxer API. Threading concerns are handled by the code
// that uses this API.
// When calling the defined functions, the |user_data| argument must be the
// void* user_data field stored in this struct.
typedef struct CobaltExtensionDemuxer {
  // Initialize must only be called once for a demuxer; subsequent calls can
  // fail.
  CobaltExtensionDemuxerStatus (*Initialize)(void* user_data);

  CobaltExtensionDemuxerStatus (*Seek)(SbTime seek_time, void* user_data);

  // Returns the starting time for the media file; it is always positive.
  SbTime (*GetStartTime)(void* user_data);

  // Returns the time -- in microseconds since Windows epoch -- represented by
  // presentation timestamp 0. If the timestamps are not associated with a time,
  // returns 0.
  SbTime (*GetTimelineOffset)(void* user_data);

  // Calls |read_cb| with a buffer of type |type| and the user data provided by
  // |read_cb_user_data|. |read_cb| is a synchronous function, so the data
  // passed to it can safely be freed after |read_cb| returns. |read_cb| must be
  // called exactly once, and it must be called before Read returns.
  //
  // An error can be handled in one of two ways:
  // 1. Pass a null buffer to read_cb. This will cause the pipeline to handle
  //    the situation as an error. Alternatively,
  // 2. Pass an "end of stream" buffer to read_cb. This will cause the relevant
  //    stream to end normally.
  void (*Read)(CobaltExtensionDemuxerStreamType type,
               CobaltExtensionDemuxerReadCB read_cb,
               void* read_cb_user_data,
               void* user_data);

  // Returns true and populates |audio_config| if an audio stream is present;
  // returns false otherwise. |config| must not be null.
  bool (*GetAudioConfig)(CobaltExtensionDemuxerAudioDecoderConfig* config,
                         void* user_data);

  // Returns true and populates |video_config| if a video stream is present;
  // returns false otherwise. |config| must not be null.
  bool (*GetVideoConfig)(CobaltExtensionDemuxerVideoDecoderConfig* config,
                         void* user_data);

  // Returns the duration, in microseconds.
  SbTime (*GetDuration)(void* user_data);

  // Will be passed to all functions.
  void* user_data;
} CobaltExtensionDemuxer;

typedef struct CobaltExtensionDemuxerDataSource {
  // Reads up to |bytes_requested|, writing the data into |data| and returning
  // the number of bytes read. |data| must be able to store at least
  // |bytes_requested| bytes. Calling BlockingRead advances the read position.
  int (*BlockingRead)(uint8_t* data, int bytes_requested, void* user_data);

  // Seeks to |position| (specified in bytes) in the data source.
  void (*SeekTo)(int position, void* user_data);

  // Returns the offset into the data source, in bytes.
  int64_t (*GetPosition)(void* user_data);

  // Returns the size of the data source, in bytes.
  int64_t (*GetSize)(void* user_data);

  // Whether this represents a streaming data source.
  bool is_streaming;

  // Will be passed to all functions.
  void* user_data;
} CobaltExtensionDemuxerDataSource;

typedef struct CobaltExtensionDemuxerApi {
  // Name should be the string |kCobaltExtensionDemuxerApi|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Creates a demuxer for the content provided by |data_source|. Ownership of
  // |data_source| is not transferred to this function.
  //
  // Ownership of the returned demuxer is transferred to the caller, but it must
  // be deleted via DestroyDemuxer (below). The caller must not manually delete
  // the demuxer.
  CobaltExtensionDemuxer* (*CreateDemuxer)(
      CobaltExtensionDemuxerDataSource* data_source,
      CobaltExtensionDemuxerAudioCodec* supported_audio_codecs,
      int64_t supported_audio_codecs_size,
      CobaltExtensionDemuxerVideoCodec* supported_video_codecs,
      int64_t supported_video_codecs_size);

  // Destroys |demuxer|. After calling this, |demuxer| must not be dereferenced
  // or deleted by the caller.
  void (*DestroyDemuxer)(CobaltExtensionDemuxer* demuxer);
} CobaltExtensionDemuxerApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_DEMUXER_H_
