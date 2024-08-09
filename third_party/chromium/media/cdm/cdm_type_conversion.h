// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CDM_CDM_TYPE_CONVERSION_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CDM_CDM_TYPE_CONVERSION_H_

#include <vector>

#include "third_party/chromium/media/base/audio_codecs.h"
#include "third_party/chromium/media/base/audio_decoder_config.h"
#include "third_party/chromium/media/base/cdm_key_information.h"
#include "third_party/chromium/media/base/cdm_promise.h"
#include "third_party/chromium/media/base/channel_layout.h"
#include "third_party/chromium/media/base/content_decryption_module.h"
#include "third_party/chromium/media/base/decoder_buffer.h"
#include "third_party/chromium/media/base/decrypt_config.h"
#include "third_party/chromium/media/base/decryptor.h"
#include "third_party/chromium/media/base/encryption_scheme.h"
#include "third_party/chromium/media/base/media_export.h"
#include "third_party/chromium/media/base/sample_format.h"
#include "third_party/chromium/media/base/video_codecs.h"
#include "third_party/chromium/media/base/video_color_space.h"
#include "third_party/chromium/media/base/video_decoder_config.h"
#include "third_party/chromium/media/base/video_types.h"
#include "third_party/chromium/media/cdm/api/content_decryption_module.h"

namespace media_m96 {

// Color Converters

MEDIA_EXPORT cdm::ColorSpace ToCdmColorSpace(
    const VideoColorSpace& color_space);
MEDIA_EXPORT VideoColorSpace
ToMediaColorSpace(const cdm::ColorSpace& color_space);

// CDM Converters

MEDIA_EXPORT cdm::HdcpVersion ToCdmHdcpVersion(HdcpVersion hdcp_version);

MEDIA_EXPORT cdm::SessionType ToCdmSessionType(CdmSessionType session_type);
MEDIA_EXPORT CdmSessionType ToMediaSessionType(cdm::SessionType session_type);

MEDIA_EXPORT cdm::InitDataType ToCdmInitDataType(
    EmeInitDataType init_data_type);
MEDIA_EXPORT EmeInitDataType
ToEmeInitDataType(cdm::InitDataType init_data_type);

MEDIA_EXPORT CdmKeyInformation::KeyStatus ToMediaKeyStatus(
    cdm::KeyStatus status);
MEDIA_EXPORT cdm::KeyStatus ToCdmKeyStatus(CdmKeyInformation::KeyStatus status);

MEDIA_EXPORT cdm::EncryptionScheme ToCdmEncryptionScheme(
    EncryptionScheme scheme);

MEDIA_EXPORT CdmPromise::Exception ToMediaCdmPromiseException(
    cdm::Exception exception);
MEDIA_EXPORT cdm::Exception ToCdmException(CdmPromise::Exception exception);

MEDIA_EXPORT CdmMessageType ToMediaMessageType(cdm::MessageType message_type);
MEDIA_EXPORT cdm::MessageType ToCdmMessageType(CdmMessageType message_type);

MEDIA_EXPORT cdm::StreamType ToCdmStreamType(Decryptor::StreamType stream_type);

MEDIA_EXPORT Decryptor::Status ToMediaDecryptorStatus(cdm::Status status);

// Audio Converters

MEDIA_EXPORT cdm::AudioCodec ToCdmAudioCodec(AudioCodec codec);
MEDIA_EXPORT SampleFormat ToMediaSampleFormat(cdm::AudioFormat format);

// Video Converters

MEDIA_EXPORT cdm::VideoCodec ToCdmVideoCodec(VideoCodec codec);
MEDIA_EXPORT VideoCodec ToMediaVideoCodec(cdm::VideoCodec codec);

MEDIA_EXPORT cdm::VideoCodecProfile ToCdmVideoCodecProfile(
    VideoCodecProfile profile);
MEDIA_EXPORT VideoCodecProfile
ToMediaVideoCodecProfile(cdm::VideoCodecProfile profile);

MEDIA_EXPORT cdm::VideoFormat ToCdmVideoFormat(VideoPixelFormat format);
MEDIA_EXPORT VideoPixelFormat ToMediaVideoFormat(cdm::VideoFormat format);

// Aggregated Types

// Warning: The returned config contains raw pointers to the extra data in the
// input |config|. Hence, the caller must make sure the input |config| outlives
// the returned config.
MEDIA_EXPORT cdm::AudioDecoderConfig_2 ToCdmAudioDecoderConfig(
    const AudioDecoderConfig& config);

// Warning: The returned config contains raw pointers to the extra data in the
// input |config|. Hence, the caller must make sure the input |config| outlives
// the returned config.
MEDIA_EXPORT cdm::VideoDecoderConfig_3 ToCdmVideoDecoderConfig(
    const VideoDecoderConfig& config);

// Fill |input_buffer| based on the values in |encrypted|. |subsamples|
// is used to hold some of the data. |input_buffer| will contain pointers
// to data contained in |encrypted| and |subsamples|, so the lifetime of
// |input_buffer| must be <= the lifetime of |encrypted| and |subsamples|.
MEDIA_EXPORT void ToCdmInputBuffer(const DecoderBuffer& encrypted_buffer,
                                   std::vector<cdm::SubsampleEntry>* subsamples,
                                   cdm::InputBuffer_2* input_buffer);

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CDM_CDM_TYPE_CONVERSION_H_
