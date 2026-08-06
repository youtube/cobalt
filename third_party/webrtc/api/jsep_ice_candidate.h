/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(deadbeef): Move this out of api/; it's an implementation detail and
// shouldn't be used externally.

#ifndef API_JSEP_ICE_CANDIDATE_H_
#define API_JSEP_ICE_CANDIDATE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/candidate.h"
#include "api/jsep.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Implementation of IceCandidateInterface.
class RTC_EXPORT JsepIceCandidate : public IceCandidateInterface {
 public:
  JsepIceCandidate(absl::string_view sdp_mid,
                   int sdp_mline_index,
                   const Candidate& candidate);
  JsepIceCandidate(const JsepIceCandidate&) = delete;
  JsepIceCandidate& operator=(const JsepIceCandidate&) = delete;
  ~JsepIceCandidate() override;

  static std::unique_ptr<JsepIceCandidate> Create(
      absl::string_view mid,
      int sdp_mline_index,
      absl::string_view sdp,
      SdpParseError* absl_nullable error = nullptr);

  std::string sdp_mid() const override;
  int sdp_mline_index() const override;
  const Candidate& candidate() const override;

  std::string server_url() const override;

  bool ToString(std::string* out) const override;

 private:
  const std::string sdp_mid_;
  const int sdp_mline_index_;
  const Candidate candidate_;
};

// Implementation of IceCandidateCollection which stores JsepIceCandidates.
class JsepCandidateCollection : public IceCandidateCollection {
 public:
  JsepCandidateCollection();
  // Move constructor is defined so that a vector of JsepCandidateCollections
  // can be resized.
  JsepCandidateCollection(JsepCandidateCollection&& o);

  JsepCandidateCollection(const JsepCandidateCollection&) = delete;
  JsepCandidateCollection& operator=(const JsepCandidateCollection&) = delete;

  // Returns a copy of the candidate collection.
  JsepCandidateCollection Clone() const;
  size_t count() const override;
  bool HasCandidate(const IceCandidateInterface* candidate) const override;
  // Adds and takes ownership of the JsepIceCandidate.
  // TODO(deadbeef): Make this use an std::unique_ptr<>, so ownership logic is
  // more clear.
  virtual void add(JsepIceCandidate* candidate);
  const IceCandidateInterface* at(size_t index) const override;
  // Removes the candidate that has a matching address and protocol.
  //
  // Returns the number of candidates that were removed.
  size_t remove(const Candidate& candidate);

 private:
  std::vector<std::unique_ptr<JsepIceCandidate>> candidates_;
};

}  // namespace webrtc

#endif  // API_JSEP_ICE_CANDIDATE_H_
