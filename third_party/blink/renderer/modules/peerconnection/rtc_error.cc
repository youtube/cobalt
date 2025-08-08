// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_error.h"

#include <utility>

#include "base/notreached.h"

namespace blink {

namespace {

String RTCErrorDetailToString(webrtc::RTCErrorDetailType detail) {
  switch (detail) {
    case webrtc::RTCErrorDetailType::NONE:
      // This should not happen, it indicates an error in webrtc
      LOG(ERROR) << "RTCError: RTCErrorDetail is NONE";
      return "";
    case webrtc::RTCErrorDetailType::DATA_CHANNEL_FAILURE:
      return "data-channel-failure";
    case webrtc::RTCErrorDetailType::DTLS_FAILURE:
      return "dtls-failure";
    case webrtc::RTCErrorDetailType::FINGERPRINT_FAILURE:
      return "fingerprint-failure";
    case webrtc::RTCErrorDetailType::SCTP_FAILURE:
      return "sctp-failure";
    case webrtc::RTCErrorDetailType::SDP_SYNTAX_ERROR:
      return "sdp-syntax-error";
    case webrtc::RTCErrorDetailType::HARDWARE_ENCODER_NOT_AVAILABLE:
      return "hardware-encoder-not-available";
    case webrtc::RTCErrorDetailType::HARDWARE_ENCODER_ERROR:
      return "hardware-encoder-error";
    default:
      // Included to ease introduction of new errors at the webrtc layer.
      NOTREACHED();
      return "";
  }
}
}  // namespace

// static
RTCError* RTCError::Create(const RTCErrorInit* init, String message) {
  return MakeGarbageCollected<RTCError>(init, std::move(message));
}

RTCError::RTCError(const RTCErrorInit* init, String message)
    : DOMException(DOMExceptionCode::kOperationError, std::move(message)),
      error_detail_(init->errorDetail()),
      sdp_line_number_(init->hasSdpLineNumber()
                           ? absl::optional<int32_t>(init->sdpLineNumber())
                           : absl::nullopt),
      http_request_status_code_(
          init->hasHttpRequestStatusCode()
              ? absl::optional<int32_t>(init->httpRequestStatusCode())
              : absl::nullopt),
      sctp_cause_code_(init->hasSctpCauseCode()
                           ? absl::optional<int32_t>(init->sctpCauseCode())
                           : absl::nullopt),
      received_alert_(init->hasReceivedAlert()
                          ? absl::optional<uint32_t>(init->receivedAlert())
                          : absl::nullopt),
      sent_alert_(init->hasSentAlert()
                      ? absl::optional<uint32_t>(init->sentAlert())
                      : absl::nullopt) {}

RTCError::RTCError(webrtc::RTCError err)
    : DOMException(DOMExceptionCode::kOperationError, err.message()),
      error_detail_(RTCErrorDetailToString(err.error_detail())),
      sctp_cause_code_(err.sctp_cause_code()
                           ? absl::optional<int32_t>(*err.sctp_cause_code())
                           : absl::nullopt) {}

const String& RTCError::errorDetail() const {
  return error_detail_;
}

}  // namespace blink
