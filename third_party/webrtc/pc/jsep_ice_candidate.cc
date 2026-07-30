/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/jsep_ice_candidate.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "pc/webrtc_sdp.h"

// This file contains JsepIceCandidate-related functions that are not
// included in api/jsep_ice_candidate.cc. Some of these link to SDP
// parsing/serializing functions, which some users may not want.
// TODO(bugs.webrtc.org/12330): Merge the two .cc files somehow.

namespace webrtc {
namespace {
// The sdpMLineIndex property is an unsigned short, a zero based index of the
// m-line associated with the candidate. This function ensures we consistently
// set the property to -1 for out-of-bounds values, to make candidate
// comparisons more robust.
int EnsureValidMLineIndex(int sdp_mline_index) {
  if (sdp_mline_index < 0 ||
      sdp_mline_index > std::numeric_limits<uint16_t>::max())
    return -1;
  return sdp_mline_index;
}
}  // namespace

IceCandidateInterface* CreateIceCandidate(const std::string& sdp_mid,
                                          int sdp_mline_index,
                                          const std::string& sdp,
                                          SdpParseError* error) {
  std::unique_ptr<JsepIceCandidate> jsep_ice =
      JsepIceCandidate::Create(sdp_mid, sdp_mline_index, sdp, error);
  if (!jsep_ice) {
    return nullptr;
  }
  return jsep_ice.release();
}

std::unique_ptr<IceCandidateInterface> CreateIceCandidate(
    const std::string& sdp_mid,
    int sdp_mline_index,
    const Candidate& candidate) {
  return std::make_unique<JsepIceCandidate>(sdp_mid, sdp_mline_index,
                                            candidate);
}

// static
std::unique_ptr<JsepIceCandidate> JsepIceCandidate::Create(
    absl::string_view mid,
    int sdp_mline_index,
    absl::string_view sdp,
    SdpParseError* absl_nullable error /*= nullptr*/) {
  Candidate candidate;
  if (!SdpDeserializeCandidate(mid, sdp, &candidate, error)) {
    return nullptr;
  }
  return std::make_unique<JsepIceCandidate>(mid, sdp_mline_index, candidate);
}

JsepIceCandidate::JsepIceCandidate(absl::string_view sdp_mid,
                                   int sdp_mline_index,
                                   const Candidate& candidate)
    : sdp_mid_(sdp_mid),
      sdp_mline_index_(EnsureValidMLineIndex(sdp_mline_index)),
      candidate_(candidate) {}

JsepIceCandidate::~JsepIceCandidate() {}

JsepCandidateCollection JsepCandidateCollection::Clone() const {
  JsepCandidateCollection new_collection;
  for (const auto& candidate : candidates_) {
    new_collection.candidates_.push_back(std::make_unique<JsepIceCandidate>(
        candidate->sdp_mid(), candidate->sdp_mline_index(),
        candidate->candidate()));
  }
  return new_collection;
}

bool JsepIceCandidate::ToString(std::string* out) const {
  if (!out)
    return false;
  *out = SdpSerializeCandidate(*this);
  return !out->empty();
}

}  // namespace webrtc
