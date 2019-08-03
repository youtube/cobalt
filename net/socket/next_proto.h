// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_NEXT_PROTO_H_
#define NET_SOCKET_NEXT_PROTO_H_

namespace net {

// Next Protocol Negotiation (NPN), if successful, results in agreement on an
// application-level string that specifies the application level protocol to
// use over the TLS connection. NextProto enumerates the application level
// protocols that we recognise.
enum NextProto {
  kProtoUnknown = 0,
  kProtoHTTP11 = 1,
  kProtoSPDY1 = 2,
  kProtoSPDY2 = 3,
  kProtoSPDY21 = 4,
  kProtoSPDY3 = 5,
  kProtoMaximumVersion = 6,
};

}  // namespace net

#endif  // NET_SOCKET_NEXT_PROTO_H_
