// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/quic_spdy_server_stream_base.h"

#include "absl/strings/string_view.h"
#include "quiche/quic/core/http/quic_spdy_session.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/platform/api/quic_flag_utils.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/common/quiche_text_utils.h"

namespace quic {

QuicSpdyServerStreamBase::QuicSpdyServerStreamBase(QuicStreamId id,
                                                   QuicSpdySession* session,
                                                   StreamType type)
    : QuicSpdyStream(id, session, type) {}

QuicSpdyServerStreamBase::QuicSpdyServerStreamBase(PendingStream* pending,
                                                   QuicSpdySession* session)
    : QuicSpdyStream(pending, session) {}

void QuicSpdyServerStreamBase::CloseWriteSide() {
  if (!fin_received() && !rst_received() && sequencer()->ignore_read_data() &&
      !rst_sent()) {
    // Early cancel the stream if it has stopped reading before receiving FIN
    // or RST.
    QUICHE_DCHECK(fin_sent() || !session()->connection()->connected());
    // Tell the peer to stop sending further data.
    QUIC_DVLOG(1) << " Server: Send QUIC_STREAM_NO_ERROR on stream " << id();
    MaybeSendStopSending(QUIC_STREAM_NO_ERROR);
  }

  QuicSpdyStream::CloseWriteSide();
}

void QuicSpdyServerStreamBase::StopReading() {
  if (!fin_received() && !rst_received() && write_side_closed() &&
      !rst_sent()) {
    QUICHE_DCHECK(fin_sent());
    // Tell the peer to stop sending further data.
    QUIC_DVLOG(1) << " Server: Send QUIC_STREAM_NO_ERROR on stream " << id();
    MaybeSendStopSending(QUIC_STREAM_NO_ERROR);
  }
  QuicSpdyStream::StopReading();
}

bool QuicSpdyServerStreamBase::AreHeadersValid(
    const QuicHeaderList& header_list) const {
  if (!GetQuicReloadableFlag(quic_verify_request_headers_2)) {
    return true;
  }
  QUIC_RELOADABLE_FLAG_COUNT_N(quic_verify_request_headers_2, 2, 3);
  if (!QuicSpdyStream::AreHeadersValid(header_list)) {
    return false;
  }

  bool saw_connect = false;
  bool saw_protocol = false;
  bool saw_path = false;
  bool saw_scheme = false;
  bool saw_method = false;
  bool saw_authority = false;
  bool is_extended_connect = false;
  // Check if it is missing any required headers and if there is any disallowed
  // ones.
  for (const std::pair<std::string, std::string>& pair : header_list) {
    if (pair.first == ":method") {
      saw_method = true;
      if (pair.second == "CONNECT") {
        saw_connect = true;
        if (saw_protocol) {
          is_extended_connect = true;
        }
      }
    } else if (pair.first == ":protocol") {
      saw_protocol = true;
      if (saw_connect) {
        is_extended_connect = true;
      }
    } else if (pair.first == ":scheme") {
      saw_scheme = true;
    } else if (pair.first == ":path") {
      saw_path = true;
    } else if (pair.first == ":authority") {
      saw_authority = true;
    } else if (absl::StrContains(pair.first, ":")) {
      QUIC_DLOG(ERROR) << "Unexpected ':' in header " << pair.first << ".";
      return false;
    }
    if (is_extended_connect) {
      if (!spdy_session()->allow_extended_connect()) {
        QUIC_DLOG(ERROR)
            << "Received extended-CONNECT request while it is disabled.";
        return false;
      }
    } else if (saw_method && !saw_connect) {
      if (saw_protocol) {
        QUIC_DLOG(ERROR) << "Receive non-CONNECT request with :protocol.";
        return false;
      }
    }
  }

  if (is_extended_connect) {
    if (saw_scheme && saw_path && saw_authority) {
      // Saw all the required pseudo headers.
      return true;
    }
    QUIC_DLOG(ERROR) << "Missing required pseudo headers for extended-CONNECT.";
    return false;
  }
  // This is a vanilla CONNECT or non-CONNECT request.
  if (saw_connect) {
    // Check vanilla CONNECT.
    if (saw_path || saw_scheme) {
      QUIC_DLOG(ERROR)
          << "Received invalid CONNECT request with disallowed pseudo header.";
      return false;
    }
    return true;
  }
  // Check non-CONNECT request.
  if (saw_method && saw_authority && saw_path && saw_scheme) {
    return true;
  }
  QUIC_LOG(ERROR) << "Missing required pseudo headers.";
  return false;
}

}  // namespace quic
