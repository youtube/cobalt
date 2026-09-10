/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_DATAGRAM_CONNECTION_H_
#define API_DATAGRAM_CONNECTION_H_

#include <cstddef>
#include <cstdint>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/candidate.h"
#include "api/ref_count.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Experimental class to support prototyping of a packet-level web API
// "RtcTransport" being discussed in the w3c working group.
// Subject to dramatic change without notice.
//
// All interactions should be on the same thread which is also used for
// networking internals.
class RTC_EXPORT DatagramConnection : public RefCountInterface {
 public:
  enum class WireProtocol {
    kDtls,
    kDtlsSrtp,
  };

  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnCandidateGathered(const Candidate& candidate) = 0;
    virtual void OnPacketReceived(ArrayView<const uint8_t> data) = 0;
    // Notification of an asynchronous failure to an earlier call to SendPacket.
    // TODO(crbug.com/443019066): Associate this with a specific send call.
    virtual void OnSendError() = 0;
    // Notification of an error unrelated to sending. Observers should
    // check the current state of the connection.
    virtual void OnConnectionError() = 0;

    virtual void OnWritableChange() = 0;
  };

  virtual ~DatagramConnection() = default;

  virtual void SetRemoteIceParameters(const IceParameters& ice_parameters) = 0;
  virtual void AddRemoteCandidate(const Candidate& candidate) = 0;

  // Whether SendPacket calls should be expected to succeed.
  // See also Observer::OnWritableChange().
  virtual bool Writable() = 0;

  enum class SSLRole { kClient, kServer };
  virtual void SetRemoteDtlsParameters(absl::string_view digestAlgorithm,
                                       const uint8_t* digest,
                                       size_t digest_len,
                                       SSLRole ssl_role) = 0;
  // SendPacket on this connection, returning whether the send succeeded.
  virtual bool SendPacket(ArrayView<const uint8_t> data) = 0;

  // Initiate closing connection and releasing resources. Must be called before
  // destruction.
  virtual void Terminate(
      absl::AnyInvocable<void()> terminate_complete_callback) = 0;
};

}  // namespace webrtc
#endif  // API_DATAGRAM_CONNECTION_H_
