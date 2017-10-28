// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_EME_CONSTANTS_H_
#define COBALT_MEDIA_BASE_EME_CONSTANTS_H_

#include "starboard/types.h"

namespace cobalt {
namespace media {

// Defines values that specify registered Initialization Data Types used
// in Encrypted Media Extensions (EME).
// http://w3c.github.io/encrypted-media/initdata-format-registry.html#registry
enum EmeInitDataType {
  kEmeInitDataTypeUnknown,
  kEmeInitDataTypeCenc,
  kEmeInitDataTypeFairplay,
  kEmeInitDataTypeKeyIds,
  kEmeInitDataTypeWebM,
};

// Defines bitmask values that specify codecs used in Encrypted Media Extension
// (EME). Each value represents a codec within a specific container.
// The mask values are stored in a SupportedCodecs.
enum EmeCodec {
  // *_ALL values should only be used for masking, do not use them to specify
  // codec support because they may be extended to include more codecs.
  EME_CODEC_NONE = 0,
  EME_CODEC_WEBM_OPUS = 1 << 0,
  EME_CODEC_WEBM_VORBIS = 1 << 1,
  EME_CODEC_WEBM_AUDIO_ALL = EME_CODEC_WEBM_OPUS | EME_CODEC_WEBM_VORBIS,
  EME_CODEC_WEBM_VP8 = 1 << 2,
  EME_CODEC_WEBM_VP9 = 1 << 3,
  EME_CODEC_WEBM_VIDEO_ALL = (EME_CODEC_WEBM_VP8 | EME_CODEC_WEBM_VP9),
  EME_CODEC_WEBM_ALL = (EME_CODEC_WEBM_AUDIO_ALL | EME_CODEC_WEBM_VIDEO_ALL),
  EME_CODEC_MP4_AAC = 1 << 4,
  EME_CODEC_MP4_AUDIO_ALL = EME_CODEC_MP4_AAC,
  EME_CODEC_MP4_AVC1 = 1 << 5,
  EME_CODEC_MP4_VP9 = 1 << 6,
  EME_CODEC_MP4_HEVC = 1 << 7,
  EME_CODEC_MP4_VIDEO_ALL =
      (EME_CODEC_MP4_AVC1 | EME_CODEC_MP4_VP9 | EME_CODEC_MP4_HEVC),
  EME_CODEC_MP4_ALL = (EME_CODEC_MP4_AUDIO_ALL | EME_CODEC_MP4_VIDEO_ALL),
  EME_CODEC_AUDIO_ALL = (EME_CODEC_WEBM_AUDIO_ALL | EME_CODEC_MP4_AUDIO_ALL),
  EME_CODEC_VIDEO_ALL = (EME_CODEC_WEBM_VIDEO_ALL | EME_CODEC_MP4_VIDEO_ALL),
  EME_CODEC_ALL = (EME_CODEC_WEBM_ALL | EME_CODEC_MP4_ALL),
};

typedef uint32_t SupportedCodecs;

enum EmeSessionTypeSupport {
  // Invalid default value.
  kEmeSessionTypeSupportInvalid,
  // The session type is not supported.
  kEmeSessionTypeSupportNotSupported,
  // The session type is supported if a distinctive identifier is available.
  kEmeSessionTypeSupportSupportedWithIdentifier,
  // The session type is always supported.
  kEmeSessionTypeSupportSupported,
};

// Used to declare support for distinctive identifier and persistent state.
// These are purposefully limited to not allow one to require the other, so that
// transitive requirements are not possible. Non-trivial refactoring would be
// required to support transitive requirements.
enum EmeFeatureSupport {
  // Invalid default value.
  kEmeFeatureSupportInvalid,
  // Access to the feature is not supported at all.
  kEmeFeatureSupportNotSupported,
  // Access to the feature may be requested.
  kEmeFeatureSupportRequestable,
  // Access to the feature cannot be blocked.
  kEmeFeatureSupportAlwaysEnable,
};

enum EmeMediaType {
  kEmeMediaTypeAudio,
  kEmeMediaTypeVideo,
};

// Configuration rules indicate the configuration state required to support a
// configuration option (note: a configuration option may be disallowing a
// feature). Configuration rules are used to answer queries about distinctive
// identifier, persistent state, and robustness requirements, as well as to
// describe support for different session types.
//
// If in the future there are reasons to request user permission other than
// access to a distinctive identifier, then additional rules should be added.
// Rules are implemented in ConfigState and are otherwise opaque.
enum EmeConfigRule {
  // The configuration option is not supported.
  kEmeConfigRuleNotSupported,
  // The configuration option prevents use of a distinctive identifier.
  kEmeConfigRuleIdentifierNotAllowed,
  // The configuration option is supported if a distinctive identifier is
  // available.
  kEmeConfigRuleIdentifiedRequired,
  // The configuration option is supported, but the user experience may be
  // improved if a distinctive identifier is available.
  kEmeConfigRuleIdentifierRecommended,
  // The configuration option prevents use of persistent state.
  kEmeConfigRulePersistNotAllowed,
  // The configuration option is supported if persistent state is available.
  kEmeConfigRulePersistRequired,
  // The configuration option is supported if both a distinctive identifier and
  // persistent state are available.
  kEmeConfigRuleIdentifierAndPersistenceRequired,
  // The configuration option prevents use of hardware-secure codecs.
  // This rule only has meaning on platforms that distinguish hardware-secure
  // codecs (ie. Android).
  kEmeConfigRuleHwSecureCodecsNotAllowed,
  // The configuration option is supported if hardware-secure codecs are used.
  // This rule only has meaning on platforms that distinguish hardware-secure
  // codecs (ie. Android).
  kEmeConfigRuleHwSecureCodecsRequired,
  // The configuration option is supported without conditions.
  kEmeConfigRuleSupported,
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_EME_CONSTANTS_H_
