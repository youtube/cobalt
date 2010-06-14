// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_HTTP_STREAM_H_
#define NET_SPDY_SPDY_HTTP_STREAM_H_

#include <list>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_stream.h"

namespace net {

class HttpResponseInfo;
class IOBuffer;
class SpdySession;
class UploadData;
class UploadDataStream;

// The SpdyHttpStream is a HTTP-specific type of stream known to a SpdySession.
class SpdyHttpStream : public SpdyStream {
 public:
  // SpdyHttpStream constructor
  SpdyHttpStream(
      SpdySession* session, spdy::SpdyStreamId stream_id, bool pushed);

  // ===================================================
  // Interface for [Http|Spdy]NetworkTransaction to use.

  // Sends the request.  If |upload_data| is non-NULL, sends that in the request
  // body.  |callback| is used when this completes asynchronously.  Note that
  // the actual SYN_STREAM packet will have already been sent by this point.
  // Also note that SpdyStream takes ownership of |upload_data|.
  int SendRequest(UploadDataStream* upload_data,
                  HttpResponseInfo* response,
                  CompletionCallback* callback);

  // Reads the response headers.  Returns a net error code.
  int ReadResponseHeaders(CompletionCallback* callback);

  // Reads the response body.  Returns a net error code or the number of bytes
  // read.
  int ReadResponseBody(
      IOBuffer* buf, int buf_len, CompletionCallback* callback);

  // Cancels the stream.  Note that this does not immediately cause deletion of
  // the stream.  This function is used to cancel any callbacks from being
  // invoked.  TODO(willchan): It should also free up any memory associated with
  // the stream, such as IOBuffers.
  void Cancel();

  // Returns the number of bytes uploaded.
  uint64 GetUploadProgress() const;

  // ===================================================
  // Interface for SpdySession to use.

  // Called by the SpdySession when a response (e.g. a SYN_REPLY) has been
  // received for this stream.
  // SpdyHttpSession calls back |callback| set by SendRequest or
  // ReadResponseHeaders.
  virtual int OnResponseReceived(const HttpResponseInfo& response);

  // Called by the SpdySession when response data has been received for this
  // stream.  This callback may be called multiple times as data arrives
  // from the network, and will never be called prior to OnResponseReceived.
  // SpdyHttpSession schedule to call back |callback| set by ReadResponseBody.
  virtual bool OnDataReceived(const char* buffer, int bytes);

  // Called by the SpdySession when a write has completed.  This callback
  // will be called multiple times for each write which completes.  Writes
  // include the SYN_STREAM write and also DATA frame writes.
  virtual void OnWriteComplete(int status);

  // Called by the SpdySession when the request is finished.  This callback
  // will always be called at the end of the request and signals to the
  // stream that the stream has no more network events.  No further callbacks
  // to the stream will be made after this call.
  // SpdyHttpSession call back |callback| set by SendRequest,
  // ReadResponseHeaders or ReadResponseBody.
  virtual void OnClose(int status);

 private:
  friend class base::RefCounted<SpdyHttpStream>;
  virtual ~SpdyHttpStream();

  // Call the user callback.
  void DoCallback(int rv);

  void ScheduleBufferedReadCallback();
  void DoBufferedReadCallback();
  bool ShouldWaitForMoreBufferedData() const;

  bool download_finished_;

  // We buffer the response body as it arrives asynchronously from the stream.
  // TODO(mbelshe):  is this infinite buffering?
  std::list<scoped_refptr<IOBufferWithSize> > response_body_;

  CompletionCallback* user_callback_;

  // User provided buffer for the ReadResponseBody() response.
  scoped_refptr<IOBuffer> user_buffer_;
  int user_buffer_len_;

  // Is there a scheduled read callback pending.
  bool buffered_read_callback_pending_;
  // Has more data been received from the network during the wait for the
  // scheduled read callback.
  bool more_read_data_pending_;

  DISALLOW_COPY_AND_ASSIGN(SpdyHttpStream);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_HTTP_STREAM_H_
