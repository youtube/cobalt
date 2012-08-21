// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_STREAM_H_

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "net/base/completion_callback.h"

namespace net {

class BoundNetLog;
class HttpRequestHeaders;
struct HttpRequestInfo;
class HttpResponseInfo;
struct WebSocketFrameChunk;

// WebSocketStream is a transport-agnostic interface for reading and writing
// WebSocket frames. This class provides an abstraction for WebSocket streams
// based on various transport layer, such as normal WebSocket connections
// (WebSocket protocol upgraded from HTTP handshake), SPDY transports, or
// WebSocket connections with multiplexing extension. Subtypes of
// WebSocketStream are responsible for managing the underlying transport
// appropriately.
//
// All functions except Close() can be asynchronous. If an operation cannot
// be finished synchronously, the function returns ERR_IO_PENDING, and
// |callback| will be called when the operation is finished. Non-null |callback|
// must be provided to these functions.

class WebSocketStream {
 public:
  WebSocketStream() {}

  // Derived classes must make sure Close() is called when the stream is not
  // closed on destruction.
  virtual ~WebSocketStream() {}

  // Initializes stream. Must be called before calling SendHandshakeRequest().
  // Returns a net error code, possibly ERR_IO_PENDING, as stated above.
  //
  // |request_info.url| must be a URL starting with "ws://" or "wss://".
  // |request_info.method| must be "GET". |request_info.upload_data| is
  // ignored.
  virtual int InitializeStream(const HttpRequestInfo& request_info,
                               const BoundNetLog& net_log,
                               const CompletionCallback& callback) = 0;

  // Writes WebSocket handshake request to the underlying socket. Must be
  // called after InitializeStream() completes and before
  // ReadHandshakeResponse() is called.
  //
  // |response_info| must remain valid until the stream is destroyed.
  virtual int SendHandshakeRequest(const HttpRequestHeaders& headers,
                                   HttpResponseInfo* response_info,
                                   const CompletionCallback& callback) = 0;

  // Reads WebSocket handshake response from the underlying socket. This
  // function completes when the response headers have been completely
  // received. Must be called after SendHandshakeRequest() completes.
  virtual int ReadHandshakeResponse(const CompletionCallback& callback) = 0;

  // Reads WebSocket frame data. This operation finishes when new frame data
  // becomes available. Each frame message might be chopped off in the middle
  // as specified in the description of WebSocketFrameChunk struct.
  // |frame_chunks| must be valid until the operation completes or Close()
  // is called.
  //
  // This function can be called after ReadHandshakeResponse(). This function
  // should not be called while the previous call of ReadFrames() is still
  // pending.
  virtual int ReadFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                         const CompletionCallback& callback) = 0;

  // Writes WebSocket frame data. |frame_chunks| must obey the rule specified
  // in the documentation of WebSocketFrameChunk struct: the first chunk of
  // a WebSocket frame must contain non-NULL |header|, and the last chunk must
  // have |final_chunk| field set to true. Series of chunks representing a
  // WebSocket frame must be consistent (the total length of |data| fields must
  // match |header->payload_length|). |frame_chunks| must be valid until the
  // operation completes or Close() is called.
  //
  // This function can be called after ReadHandshakeResponse(). This function
  // should not be called while previous call of WriteFrames() is still pending.
  virtual int WriteFrames(ScopedVector<WebSocketFrameChunk>* frame_chunks,
                          const CompletionCallback& callback) = 0;

  // Closes the stream. All pending I/O operations (if any) are canceled
  // at this point, so |frame_chunks| can be freed.
  virtual void Close() = 0;

  // TODO(yutak): Add following interfaces:
  // - RenewStreamForAuth for authentication (is this necessary?)
  // - GetSSLInfo, GetSSLCertRequsetInfo for SSL

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketStream);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_STREAM_H_
