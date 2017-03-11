// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_errors.h"

#include "base/logging.h"

namespace net {

Error WebSocketErrorToNetError(WebSocketError error) {
  switch (error) {
    case kWebSocketNormalClosure:
      return OK;

    case kWebSocketErrorGoingAway:  // TODO(ricea): More specific code?
    case kWebSocketErrorProtocolError:
    case kWebSocketErrorUnsupportedData:
    case kWebSocketErrorInvalidFramePayloadData:
    case kWebSocketErrorPolicyViolation:
    case kWebSocketErrorMandatoryExtension:
    case kWebSocketErrorInternalServerError:
      return ERR_WS_PROTOCOL_ERROR;

    case kWebSocketErrorNoStatusReceived:
    case kWebSocketErrorAbnormalClosure:
      return ERR_CONNECTION_CLOSED;

    case kWebSocketErrorTlsHandshake:
      // This error will probably be reported with more detail at a lower layer;
      // this is the best we can do at this layer.
      return ERR_SSL_PROTOCOL_ERROR;

    case kWebSocketErrorMessageTooBig:
      return ERR_MSG_TOO_BIG;

    default:
      return ERR_UNEXPECTED;
  }
}

// That this function was adapted from Chromium's IsStrictlyValidCloseStatusCode
// They differ in that codes 1004, 1005, and 1006 are reserved codes and must
// not be set in a Close message.  Chromium's check is different since they
// check for reserved codes separately.
bool IsValidCloseStatusCode(int code) {
  static const int kInvalidRanges[] = {
      // [BAD, OK)
      0,    1000,   // 1000 is the first valid code
      1004, 1007,   // 1004-1006 are reserved.
      1014, 3000,   // 1014 unassigned; 1015 up to 2999 are reserved.
      5000, 65536,  // Codes above 5000 are invalid.
  };
  const int* const kInvalidRangesEnd =
      kInvalidRanges + arraysize(kInvalidRanges);

  DCHECK_GE(code, 0);
  DCHECK_LT(code, 65536);
  const int* upper = std::upper_bound(kInvalidRanges, kInvalidRangesEnd, code);
  DCHECK_NE(kInvalidRangesEnd, upper);
  DCHECK_GT(upper, kInvalidRanges);
  DCHECK_GT(*upper, code);
  DCHECK_LE(*(upper - 1), code);
  return ((upper - kInvalidRanges) % 2) == 0;
}

}  // namespace net
