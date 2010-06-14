// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_STREAM_H_
#define NET_SPDY_SPDY_STREAM_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/bandwidth_metrics.h"
#include "net/base/io_buffer.h"
#include "net/base/net_log.h"
#include "net/http/http_request_info.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

class HttpResponseInfo;
class SpdySession;
class UploadDataStream;

// The SpdyStream is used by the SpdySession to represent each stream known
// on the SpdySession.  This class provides interfaces for SpdySession to use
// and base implementations for the interfaces.
// Streams can be created either by the client or by the server.  When they
// are initiated by the client, both the SpdySession and client object (such as
// a SpdyNetworkTransaction) will maintain a reference to the stream.  When
// initiated by the server, only the SpdySession will maintain any reference,
// until such a time as a client object requests a stream for the path.
class SpdyStream : public base::RefCounted<SpdyStream> {
 public:
  // SpdyStream constructor
  SpdyStream(SpdySession* session, spdy::SpdyStreamId stream_id, bool pushed);

  // Is this stream a pushed stream from the server.
  bool pushed() const { return pushed_; }

  spdy::SpdyStreamId stream_id() const { return stream_id_; }
  void set_stream_id(spdy::SpdyStreamId stream_id) { stream_id_ = stream_id; }

  // For pushed streams, we track a path to identify them.
  const std::string& path() const { return path_; }
  void set_path(const std::string& path) { path_ = path; }

  int priority() const { return priority_; }
  void set_priority(int priority) { priority_ = priority; }

  const BoundNetLog& net_log() const { return net_log_; }
  void set_net_log(const BoundNetLog& log) { net_log_ = log; }

  const HttpResponseInfo* GetResponseInfo() const;
  const HttpRequestInfo* GetRequestInfo() const;
  void SetRequestInfo(const HttpRequestInfo& request);
  base::Time GetRequestTime() const;
  void SetRequestTime(base::Time t);

  // Called by the SpdySession when a response (e.g. a SYN_REPLY) has been
  // received for this stream.  |path| is the path of the URL for a server
  // initiated stream, otherwise is empty.
  // Returns a status code.
  virtual int OnResponseReceived(const HttpResponseInfo& response) = 0;

  // Called by the SpdySession when response data has been received for this
  // stream.  This callback may be called multiple times as data arrives
  // from the network, and will never be called prior to OnResponseReceived.
  // |buffer| contains the data received.  The stream must copy any data
  //          from this buffer before returning from this callback.
  // |length| is the number of bytes received or an error.
  //         A zero-length count does not indicate end-of-stream.
  // Returns true on success and false on error.
  virtual bool OnDataReceived(const char* buffer, int bytes) = 0;

  // Called by the SpdySession when a write has completed.  This callback
  // will be called multiple times for each write which completes.  Writes
  // include the SYN_STREAM write and also DATA frame writes.
  // |result| is the number of bytes written or a net error code.
  virtual void OnWriteComplete(int status) = 0;

  // Called by the SpdySession when the request is finished.  This callback
  // will always be called at the end of the request and signals to the
  // stream that the stream has no more network events.  No further callbacks
  // to the stream will be made after this call.
  // |status| is an error code or OK.
  virtual void OnClose(int status) = 0;

  virtual void Cancel() = 0;
  bool cancelled() const { return cancelled_; }

  void SetPushResponse(HttpResponseInfo* response_info);

 protected:
  friend class base::RefCounted<SpdyStream>;
  virtual ~SpdyStream();

  int DoOnResponseReceived(const HttpResponseInfo& response);
  bool DoOnDataReceived(const char* buffer,int bytes);
  void DoOnWriteComplete(int status);
  void DoOnClose(int status);

  void DoCancel();

  // Sends the request.  If |upload_data| is non-NULL, sends that in the
  // request body.  Note that the actual SYN_STREAM packet will have already
  // been sent by this point.
  // Note that SpdyStream takes ownership of |upload_data|.
  // TODO(ukai): move out HTTP-specific thing to SpdyHttpStream.
  int DoSendRequest(UploadDataStream* upload_data,
                    HttpResponseInfo* response_info);

  // Reads response headers. If the SpdyStream have already received
  // the response headers, return OK and response headers filled in
  // |response_info| given in SendRequest.
  // Otherwise, return ERR_IO_PENDING.
  int DoReadResponseHeaders();

  const UploadDataStream* request_body_stream() const {
    return request_body_stream_.get();
  }

  bool is_idle() const { return io_state_ == STATE_NONE; }
  bool response_complete() const { return response_complete_; }
  void set_response_complete(bool response_complete) {
    response_complete_ = response_complete;
  }
  int response_status() const { return response_status_; }

 private:
  enum State {
    STATE_NONE,
    STATE_SEND_HEADERS,
    STATE_SEND_HEADERS_COMPLETE,
    STATE_SEND_BODY,
    STATE_SEND_BODY_COMPLETE,
    STATE_READ_HEADERS,
    STATE_READ_HEADERS_COMPLETE,
    STATE_READ_BODY,
    STATE_READ_BODY_COMPLETE,
    STATE_DONE
  };

  // Try to make progress sending/receiving the request/response.
  int DoLoop(int result);

  // The implementations of each state of the state machine.
  int DoSendHeaders();
  int DoSendHeadersComplete(int result);
  int DoSendBody();
  int DoSendBodyComplete(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);
  int DoReadBody();
  int DoReadBodyComplete(int result);

  // Update the histograms.  Can safely be called repeatedly, but should only
  // be called after the stream has completed.
  void UpdateHistograms();

  spdy::SpdyStreamId stream_id_;
  std::string path_;
  int priority_;
  const bool pushed_;
  ScopedBandwidthMetrics metrics_;

  scoped_refptr<SpdySession> session_;

  // The request to send.
  HttpRequestInfo request_;

  // The time at which the request was made that resulted in this response.
  // For cached responses, this time could be "far" in the past.
  base::Time request_time_;

  // The push_response_ is the HTTP response data which is part of
  // a server-initiated SYN_STREAM. If a client request comes in
  // which matches the push stream, the data in push_response_ will
  // be copied over to the response_ object owned by the caller
  // of the request.
  scoped_ptr<HttpResponseInfo> push_response_;

  // response_ is the HTTP response data object which is filled in
  // when a SYN_REPLY comes in for the stream. It is not owned by this
  // stream object.
  HttpResponseInfo* response_;

  scoped_ptr<UploadDataStream> request_body_stream_;

  bool response_complete_;  // TODO(mbelshe): fold this into the io_state.
  State io_state_;

  // Since we buffer the response, we also buffer the response status.
  // Not valid until response_complete_ is true.
  int response_status_;

  bool cancelled_;

  BoundNetLog net_log_;

  base::TimeTicks send_time_;
  base::TimeTicks recv_first_byte_time_;
  base::TimeTicks recv_last_byte_time_;
  int send_bytes_;
  int recv_bytes_;
  bool histograms_recorded_;

  DISALLOW_COPY_AND_ASSIGN(SpdyStream);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_STREAM_H_
