// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/quic_client_push_promise_index.h"

#include <string>

#include "quiche/quic/core/http/quic_client_promised_info.h"
#include "quiche/quic/core/http/spdy_server_push_utils.h"

namespace quic {

QuicClientPushPromiseIndex::QuicClientPushPromiseIndex() {}

QuicClientPushPromiseIndex::~QuicClientPushPromiseIndex() {}

QuicClientPushPromiseIndex::TryHandle::~TryHandle() {}

QuicClientPromisedInfo* QuicClientPushPromiseIndex::GetPromised(
    const std::string& url) {
  auto it = promised_by_url_.find(url);
  if (it == promised_by_url_.end()) {
    return nullptr;
  }
  return it->second;
}

QuicAsyncStatus QuicClientPushPromiseIndex::Try(
    const spdy::Http2HeaderBlock& request,
    QuicClientPushPromiseIndex::Delegate* delegate, TryHandle** handle) {
  std::string url(SpdyServerPushUtils::GetPromisedUrlFromHeaders(request));
  auto it = promised_by_url_.find(url);
  if (it != promised_by_url_.end()) {
    QuicClientPromisedInfo* promised = it->second;
    QuicAsyncStatus rv = promised->HandleClientRequest(request, delegate);
    if (rv == QUIC_PENDING) {
      *handle = promised;
    }
    return rv;
  }
  return QUIC_FAILURE;
}

}  // namespace quic
