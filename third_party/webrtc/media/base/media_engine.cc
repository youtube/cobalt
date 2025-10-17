/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/media_engine.h"

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/array_view.h"
#include "api/field_trials_view.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "api/video/video_codec_constants.h"
#include "api/video_codecs/scalability_mode.h"
#include "media/base/codec.h"
#include "media/base/codec_comparators.h"
#include "media/base/rid_description.h"
#include "media/base/stream_params.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {
bool SupportsMode(const Codec& codec,
                  std::optional<std::string> scalability_mode) {
  if (!scalability_mode.has_value()) {
    return true;
  }
  return absl::c_any_of(codec.scalability_modes, [&](ScalabilityMode mode) {
    return ScalabilityModeToString(mode) == *scalability_mode;
  });
}

}  // namespace

RtpParameters CreateRtpParametersWithOneEncoding() {
  RtpParameters parameters;
  RtpEncodingParameters encoding;
  parameters.encodings.push_back(encoding);
  return parameters;
}

RtpParameters CreateRtpParametersWithEncodings(StreamParams sp) {
  std::vector<uint32_t> primary_ssrcs;
  sp.GetPrimarySsrcs(&primary_ssrcs);
  size_t encoding_count = primary_ssrcs.size();

  std::vector<RtpEncodingParameters> encodings(encoding_count);
  for (size_t i = 0; i < encodings.size(); ++i) {
    encodings[i].ssrc = primary_ssrcs[i];
  }

  const std::vector<RidDescription>& rids = sp.rids();
  RTC_DCHECK(rids.size() == 0 || rids.size() == encoding_count);
  for (size_t i = 0; i < rids.size(); ++i) {
    encodings[i].rid = rids[i].rid;
  }

  RtpParameters parameters;
  parameters.encodings = encodings;
  parameters.rtcp.cname = sp.cname;
  return parameters;
}

std::vector<RtpExtension> GetDefaultEnabledRtpHeaderExtensions(
    const RtpHeaderExtensionQueryInterface& query_interface) {
  std::vector<RtpExtension> extensions;
  for (const auto& entry : query_interface.GetRtpHeaderExtensions()) {
    if (entry.direction != RtpTransceiverDirection::kStopped)
      extensions.emplace_back(entry.uri, *entry.preferred_id);
  }
  return extensions;
}

RTCError CheckScalabilityModeValues(const RtpParameters& rtp_parameters,
                                    ArrayView<Codec> send_codecs,
                                    std::optional<Codec> send_codec) {
  if (send_codecs.empty()) {
    // This is an audio sender or an extra check in the stack where the codec
    // list is not available and we can't check the scalability_mode values.
    return RTCError::OK();
  }

  for (size_t i = 0; i < rtp_parameters.encodings.size(); ++i) {
    if (rtp_parameters.encodings[i].codec) {
      bool codecFound = false;
      for (const Codec& codec : send_codecs) {
        if (IsSameRtpCodecIgnoringLevel(codec,
                                        *rtp_parameters.encodings[i].codec) &&
            SupportsMode(codec, rtp_parameters.encodings[i].scalability_mode)) {
          codecFound = true;
          send_codec = codec;
          break;
        }
      }
      if (!codecFound) {
        LOG_AND_RETURN_ERROR(
            RTCErrorType::INVALID_MODIFICATION,
            "Attempted to use an unsupported codec for layer " +
                std::to_string(i));
      }
    }
    if (rtp_parameters.encodings[i].scalability_mode) {
      if (!send_codec) {
        bool scalabilityModeFound = false;
        for (const Codec& codec : send_codecs) {
          for (const auto& scalability_mode : codec.scalability_modes) {
            if (ScalabilityModeToString(scalability_mode) ==
                *rtp_parameters.encodings[i].scalability_mode) {
              scalabilityModeFound = true;
              break;
            }
          }
          if (scalabilityModeFound)
            break;
        }

        if (!scalabilityModeFound) {
          LOG_AND_RETURN_ERROR(
              RTCErrorType::INVALID_MODIFICATION,
              "Attempted to set RtpParameters scalabilityMode "
              "to an unsupported value for the current codecs.");
        }
      } else {
        bool scalabilityModeFound = false;
        for (const auto& scalability_mode : send_codec->scalability_modes) {
          if (ScalabilityModeToString(scalability_mode) ==
              *rtp_parameters.encodings[i].scalability_mode) {
            scalabilityModeFound = true;
            break;
          }
        }
        if (!scalabilityModeFound) {
          LOG_AND_RETURN_ERROR(
              RTCErrorType::INVALID_MODIFICATION,
              "Attempted to set RtpParameters scalabilityMode "
              "to an unsupported value for the current codecs.");
        }
      }
    }
  }

  return RTCError::OK();
}

RTCError CheckRtpParametersValues(const RtpParameters& rtp_parameters,
                                  ArrayView<Codec> send_codecs,
                                  std::optional<Codec> send_codec,
                                  const FieldTrialsView& field_trials) {
  bool has_scale_resolution_down_to = false;
  for (size_t i = 0; i < rtp_parameters.encodings.size(); ++i) {
    if (rtp_parameters.encodings[i].bitrate_priority <= 0) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                           "Attempted to set RtpParameters bitrate_priority to "
                           "an invalid number. bitrate_priority must be > 0.");
    }
    if (rtp_parameters.encodings[i].scale_resolution_down_by &&
        *rtp_parameters.encodings[i].scale_resolution_down_by < 1.0) {
      LOG_AND_RETURN_ERROR(
          RTCErrorType::INVALID_RANGE,
          "Attempted to set RtpParameters scale_resolution_down_by to an "
          "invalid value. scale_resolution_down_by must be >= 1.0");
    }
    if (rtp_parameters.encodings[i].max_framerate &&
        *rtp_parameters.encodings[i].max_framerate < 0.0) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                           "Attempted to set RtpParameters max_framerate to an "
                           "invalid value. max_framerate must be >= 0.0");
    }
    if (rtp_parameters.encodings[i].min_bitrate_bps &&
        rtp_parameters.encodings[i].max_bitrate_bps) {
      if (*rtp_parameters.encodings[i].max_bitrate_bps <
          *rtp_parameters.encodings[i].min_bitrate_bps) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                             "Attempted to set RtpParameters min bitrate "
                             "larger than max bitrate.");
      }
    }
    if (rtp_parameters.encodings[i].num_temporal_layers) {
      if (*rtp_parameters.encodings[i].num_temporal_layers < 1 ||
          *rtp_parameters.encodings[i].num_temporal_layers >
              kMaxTemporalStreams) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                             "Attempted to set RtpParameters "
                             "num_temporal_layers to an invalid number.");
      }
    }

    if (rtp_parameters.encodings[i].scale_resolution_down_to.has_value()) {
      has_scale_resolution_down_to = true;
      if (rtp_parameters.encodings[i].scale_resolution_down_to->width <= 0 ||
          rtp_parameters.encodings[i].scale_resolution_down_to->height <= 0) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_MODIFICATION,
                             "The resolution dimensions must be positive.");
      }
    }

    if (!field_trials.IsEnabled("WebRTC-MixedCodecSimulcast")) {
      if (i > 0 && rtp_parameters.encodings[i - 1].codec !=
                       rtp_parameters.encodings[i].codec) {
        LOG_AND_RETURN_ERROR(RTCErrorType::UNSUPPORTED_OPERATION,
                             "Attempted to use different codec values for "
                             "different encodings.");
      }
    }
  }

  if (has_scale_resolution_down_to &&
      absl::c_any_of(rtp_parameters.encodings,
                     [](const webrtc::RtpEncodingParameters& encoding) {
                       return encoding.active &&
                              !encoding.scale_resolution_down_to.has_value();
                     })) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_MODIFICATION,
                         "If a resolution is specified on any encoding then "
                         "it must be specified on all encodings.");
  }

  return CheckScalabilityModeValues(rtp_parameters, send_codecs, send_codec);
}

RTCError CheckRtpParametersInvalidModificationAndValues(
    const RtpParameters& old_rtp_parameters,
    const RtpParameters& rtp_parameters,
    const FieldTrialsView& field_trials) {
  return CheckRtpParametersInvalidModificationAndValues(
      old_rtp_parameters, rtp_parameters, {}, std::nullopt, field_trials);
}

RTCError CheckRtpParametersInvalidModificationAndValues(
    const RtpParameters& old_rtp_parameters,
    const RtpParameters& rtp_parameters,
    ArrayView<Codec> send_codecs,
    std::optional<Codec> send_codec,
    const FieldTrialsView& field_trials) {
  if (rtp_parameters.encodings.size() != old_rtp_parameters.encodings.size()) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Attempted to set RtpParameters with different encoding count");
  }
  if (rtp_parameters.rtcp != old_rtp_parameters.rtcp) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Attempted to set RtpParameters with modified RTCP parameters");
  }
  if (rtp_parameters.header_extensions !=
      old_rtp_parameters.header_extensions) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Attempted to set RtpParameters with modified header extensions");
  }
  if (!absl::c_equal(old_rtp_parameters.encodings, rtp_parameters.encodings,
                     [](const webrtc::RtpEncodingParameters& encoding1,
                        const webrtc::RtpEncodingParameters& encoding2) {
                       return encoding1.rid == encoding2.rid;
                     })) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_MODIFICATION,
                         "Attempted to change RID values in the encodings.");
  }
  if (!absl::c_equal(old_rtp_parameters.encodings, rtp_parameters.encodings,
                     [](const webrtc::RtpEncodingParameters& encoding1,
                        const webrtc::RtpEncodingParameters& encoding2) {
                       return encoding1.ssrc == encoding2.ssrc;
                     })) {
    LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_MODIFICATION,
                         "Attempted to set RtpParameters with modified SSRC");
  }

  return CheckRtpParametersValues(rtp_parameters, send_codecs, send_codec,
                                  field_trials);
}

CompositeMediaEngine::CompositeMediaEngine(
    std::unique_ptr<FieldTrialsView> trials,
    std::unique_ptr<VoiceEngineInterface> audio_engine,
    std::unique_ptr<VideoEngineInterface> video_engine)
    : trials_(std::move(trials)),
      voice_engine_(std::move(audio_engine)),
      video_engine_(std::move(video_engine)) {}

CompositeMediaEngine::CompositeMediaEngine(
    std::unique_ptr<VoiceEngineInterface> audio_engine,
    std::unique_ptr<VideoEngineInterface> video_engine)
    : CompositeMediaEngine(nullptr,
                           std::move(audio_engine),
                           std::move(video_engine)) {}

CompositeMediaEngine::~CompositeMediaEngine() = default;

bool CompositeMediaEngine::Init() {
  voice().Init();
  return true;
}

VoiceEngineInterface& CompositeMediaEngine::voice() {
  return *voice_engine_.get();
}

VideoEngineInterface& CompositeMediaEngine::video() {
  return *video_engine_.get();
}

const VoiceEngineInterface& CompositeMediaEngine::voice() const {
  return *voice_engine_.get();
}

const VideoEngineInterface& CompositeMediaEngine::video() const {
  return *video_engine_.get();
}

}  // namespace webrtc
