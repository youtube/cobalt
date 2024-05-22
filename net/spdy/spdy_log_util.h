// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_LOG_UTIL_H_
#define NET_SPDY_SPDY_LOG_UTIL_H_

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/http/http_log_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/third_party/quiche/src/quiche/spdy/core/http2_header_block.h"

namespace net {

// Given an HTTP/2 GOAWAY frame |debug_data|, returns the elided version
// according to |capture_mode|.
NET_EXPORT_PRIVATE base::Value ElideGoAwayDebugDataForNetLog(
    NetLogCaptureMode capture_mode,
    base::StringPiece debug_data);

// Given a spdy::Http2HeaderBlock, return its base::Value::List representation.
NET_EXPORT_PRIVATE base::Value::List ElideHttp2HeaderBlockForNetLog(
    const spdy::Http2HeaderBlock& headers,
    NetLogCaptureMode capture_mode);

// Converts a spdy::Http2HeaderBlock into NetLog event parameters.
NET_EXPORT_PRIVATE base::Value::Dict Http2HeaderBlockNetLogParams(
    const spdy::Http2HeaderBlock* headers,
    NetLogCaptureMode capture_mode);

}  // namespace net

#endif  // NET_SPDY_SPDY_LOG_UTIL_H_
