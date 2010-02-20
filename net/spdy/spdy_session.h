// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_H_
#define NET_SPDY_SPDY_SESSION_H_

#include <deque>
#include <list>
#include <map>
#include <queue>
#include <string>

#include "base/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/ssl_config_service.h"
#include "net/base/upload_data_stream.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_handle.h"
#include "testing/platform_test.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_io_buffer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

class SpdyStream;
class HttpNetworkSession;
class HttpRequestInfo;
class HttpResponseInfo;
class LoadLog;
class SSLInfo;

class SpdySession : public base::RefCounted<SpdySession>,
                    public spdy::SpdyFramerVisitorInterface {
 public:
  // Get the domain for this SpdySession.
  const std::string& domain() const { return domain_; }

  // Connect the Spdy Socket.
  // Returns net::Error::OK on success.
  // Note that this call does not wait for the connect to complete. Callers can
  // immediately start using the SpdySession while it connects.
  net::Error Connect(const std::string& group_name,
                     const HostResolver::RequestInfo& host,
                     RequestPriority priority,
                     LoadLog* load_log);

  // Get a stream for a given |request|.  In the typical case, this will involve
  // the creation of a new stream (and will send the SYN frame).  If the server
  // initiates a stream, it might already exist for a given path.  The server
  // might also not have initiated the stream yet, but indicated it will via
  // X-Associated-Content.
  // Returns the new or existing stream.  Never returns NULL.
  scoped_refptr<SpdyStream> GetOrCreateStream(const HttpRequestInfo& request,
      const UploadDataStream* upload_data, LoadLog* log);

  // Write a data frame to the stream.
  // Used to create and queue a data frame for the given stream.
  int WriteStreamData(spdy::SpdyStreamId stream_id, net::IOBuffer* data,
                      int len);

  // Cancel a stream.
  bool CancelStream(spdy::SpdyStreamId stream_id);

  // Check if a stream is active.
  bool IsStreamActive(spdy::SpdyStreamId stream_id) const;

  // The LoadState is used for informing the user of the current network
  // status, such as "resolving host", "connecting", etc.
  LoadState GetLoadState() const;

  // Enable or disable SSL.
  static void SetSSLMode(bool enable) { use_ssl_ = enable; }
  static bool SSLMode() { return use_ssl_; }

 protected:
  friend class SpdySessionPool;

  enum State {
    IDLE,
    CONNECTING,
    CONNECTED,
    CLOSED
  };

  // Provide access to the framer for testing.
  spdy::SpdyFramer* GetFramer() { return &spdy_framer_; }

  // Create a new SpdySession.
  // |host| is the hostname that this session connects to.
  SpdySession(const std::string& host, HttpNetworkSession* session);

  // Closes all open streams.  Used as part of shutdown.
  void CloseAllStreams(net::Error code);

 private:
  friend class base::RefCounted<SpdySession>;

  typedef std::map<int, scoped_refptr<SpdyStream> > ActiveStreamMap;
  typedef std::list<scoped_refptr<SpdyStream> > ActiveStreamList;
  typedef std::map<std::string, scoped_refptr<SpdyStream> > PendingStreamMap;
  typedef std::priority_queue<SpdyIOBuffer> OutputQueue;

  virtual ~SpdySession();

  // Used by SpdySessionPool to initialize with a pre-existing SSL socket.
  void InitializeWithSSLSocket(ClientSocketHandle* connection);

  // SpdyFramerVisitorInterface
  virtual void OnError(spdy::SpdyFramer*);
  virtual void OnStreamFrameData(spdy::SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len);
  virtual void OnControl(const spdy::SpdyControlFrame* frame);

  // Control frame handlers.
  void OnSyn(const spdy::SpdySynStreamControlFrame* frame,
             const spdy::SpdyHeaderBlock* headers);
  void OnSynReply(const spdy::SpdySynReplyControlFrame* frame,
                  const spdy::SpdyHeaderBlock* headers);
  void OnFin(const spdy::SpdyRstStreamControlFrame* frame);

  // IO Callbacks
  void OnTCPConnect(int result);
  void OnSSLConnect(int result);
  void OnReadComplete(int result);
  void OnWriteComplete(int result);

  // Start reading from the socket.
  void ReadSocket();

  // Write current data to the socket.
  void WriteSocketLater();
  void WriteSocket();

  // Get a new stream id.
  int GetNewStreamId();

  // Closes this session.  This will close all active streams and mark
  // the session as permanently closed.
  // |err| should not be OK; this function is intended to be called on
  // error.
  void CloseSessionOnError(net::Error err);

  // Track active streams in the active stream list.
  void ActivateStream(SpdyStream* stream);
  void DeactivateStream(spdy::SpdyStreamId id);

  // Check if we have a pending pushed-stream for this url
  // Returns the stream if found (and returns it from the pending
  // list), returns NULL otherwise.
  scoped_refptr<SpdyStream> GetPushStream(const std::string& url);

  void GetSSLInfo(SSLInfo* ssl_info);

  // Callbacks for the Spdy session.
  CompletionCallbackImpl<SpdySession> connect_callback_;
  CompletionCallbackImpl<SpdySession> ssl_connect_callback_;
  CompletionCallbackImpl<SpdySession> read_callback_;
  CompletionCallbackImpl<SpdySession> write_callback_;

  // The domain this session is connected to.
  std::string domain_;

  SSLConfig ssl_config_;

  scoped_refptr<HttpNetworkSession> session_;

  // The socket handle for this session.
  scoped_ptr<ClientSocketHandle> connection_;

  // The read buffer used to read data from the socket.
  scoped_refptr<IOBuffer> read_buffer_;
  bool read_pending_;

  int stream_hi_water_mark_;  // The next stream id to use.

  // TODO(mbelshe): We need to track these stream lists better.
  //                I suspect it is possible to remove a stream from
  //                one list, but not the other.

  // Map from stream id to all active streams.  Streams are active in the sense
  // that they have a consumer (typically SpdyNetworkTransaction and regardless
  // of whether or not there is currently any ongoing IO [might be waiting for
  // the server to start pushing the stream]) or there are still network events
  // incoming even though the consumer has already gone away (cancellation).
  // TODO(willchan): Perhaps we should separate out cancelled streams and move
  // them into a separate ActiveStreamMap, and not deliver network events to
  // them?
  ActiveStreamMap active_streams_;
  // List of all the streams that have already started to be pushed by the
  // server, but do not have consumers yet.
  ActiveStreamList pushed_streams_;
  // List of streams declared in X-Associated-Content headers, but do not have
  // consumers yet.
  // The key is a string representing the path of the URI being pushed.
  PendingStreamMap pending_streams_;

  // As we gather data to be sent, we put it into the output queue.
  OutputQueue queue_;

  // The packet we are currently sending.
  bool write_pending_;            // Will be true when a write is in progress.
  SpdyIOBuffer in_flight_write_;  // This is the write buffer in progress.

  // Flag if we have a pending message scheduled for WriteSocket.
  bool delayed_write_pending_;

  // Flag if we're using an SSL connection for this SpdySession.
  bool is_secure_;

  // Spdy Frame state.
  spdy::SpdyFramer spdy_framer_;

  // If an error has occurred on the session, the session is effectively
  // dead.  Record this error here.  When no error has occurred, |error_| will
  // be OK.
  net::Error error_;
  State state_;

  // Some statistics counters for the session.
  int streams_initiated_count_;
  int streams_pushed_count_;
  int streams_pushed_and_claimed_count_;
  int streams_abandoned_count_;

  static bool use_ssl_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_H_
