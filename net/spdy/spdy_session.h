// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_H_
#define NET_SPDY_SPDY_SESSION_H_
#pragma once

#include <deque>
#include <list>
#include <map>
#include <queue>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/linked_ptr.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "net/base/io_buffer.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/request_priority.h"
#include "net/base/ssl_config_service.h"
#include "net/base/upload_data_stream.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_io_buffer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

// This is somewhat arbitrary and not really fixed, but it will always work
// reasonably with ethernet. Chop the world into 2-packet chunks.  This is
// somewhat arbitrary, but is reasonably small and ensures that we elicit
// ACKs quickly from TCP (because TCP tries to only ACK every other packet).
const int kMss = 1430;
const int kMaxSpdyFrameChunkSize = (2 * kMss) - spdy::SpdyFrame::size();

class BoundNetLog;
class SpdySettingsStorage;
class SpdyStream;
class SSLInfo;

class SpdySession : public base::RefCounted<SpdySession>,
                    public spdy::SpdyFramerVisitorInterface {
 public:
  // Create a new SpdySession.
  // |host_port_proxy_pair| is the host/port that this session connects to, and
  // the proxy configuration settings that it's using.
  // |spdy_session_pool| is the SpdySessionPool that owns us.  Its lifetime must
  // strictly be greater than |this|.
  // |session| is the HttpNetworkSession.  |net_log| is the NetLog that we log
  // network events to.
  SpdySession(const HostPortProxyPair& host_port_proxy_pair,
              SpdySessionPool* spdy_session_pool,
              SpdySettingsStorage* spdy_settings,
              NetLog* net_log);

  const HostPortPair& host_port_pair() const {
    return host_port_proxy_pair_.first;
  }
  const HostPortProxyPair& host_port_proxy_pair() const {
    return host_port_proxy_pair_;
  }

  // Get a pushed stream for a given |url|.
  // If the server initiates a stream, it might already exist for a given path.
  // The server might also not have initiated the stream yet, but indicated it
  // will via X-Associated-Content.  Writes the stream out to |spdy_stream|.
  // Returns a net error code.
  int GetPushStream(
      const GURL& url,
      scoped_refptr<SpdyStream>* spdy_stream,
      const BoundNetLog& stream_net_log);

  // Create a new stream for a given |url|.  Writes it out to |spdy_stream|.
  // Returns a net error code, possibly ERR_IO_PENDING.
  int CreateStream(
      const GURL& url,
      RequestPriority priority,
      scoped_refptr<SpdyStream>* spdy_stream,
      const BoundNetLog& stream_net_log,
      CompletionCallback* callback);

  // Remove PendingCreateStream objects on transaction deletion
  void CancelPendingCreateStreams(const scoped_refptr<SpdyStream>* spdy_stream);

  // Used by SpdySessionPool to initialize with a pre-existing SSL socket. For
  // testing, setting is_secure to false allows initialization with a
  // pre-existing TCP socket.
  // Returns OK on success, or an error on failure.
  net::Error InitializeWithSocket(ClientSocketHandle* connection,
                                  bool is_secure,
                                  int certificate_error_code);

  // Send the SYN frame for |stream_id|.
  int WriteSynStream(
      spdy::SpdyStreamId stream_id,
      RequestPriority priority,
      spdy::SpdyControlFlags flags,
      const linked_ptr<spdy::SpdyHeaderBlock>& headers);

  // Write a data frame to the stream.
  // Used to create and queue a data frame for the given stream.
  int WriteStreamData(spdy::SpdyStreamId stream_id, net::IOBuffer* data,
                      int len,
                      spdy::SpdyDataFlags flags);

  // Close a stream.
  void CloseStream(spdy::SpdyStreamId stream_id, int status);

  // Reset a stream by sending a RST_STREAM frame with given status code.
  // Also closes the stream.  Was not piggybacked to CloseStream since not
  // all of the calls to CloseStream necessitate sending a RST_STREAM.
  void ResetStream(spdy::SpdyStreamId stream_id, spdy::SpdyStatusCodes status);

  // Check if a stream is active.
  bool IsStreamActive(spdy::SpdyStreamId stream_id) const;

  // The LoadState is used for informing the user of the current network
  // status, such as "resolving host", "connecting", etc.
  LoadState GetLoadState() const;

  // Fills SSL info in |ssl_info| and returns true when SSL is in use.
  bool GetSSLInfo(SSLInfo* ssl_info, bool* was_npn_negotiated);

  // Fills SSL Certificate Request info |cert_request_info| and returns
  // true when SSL is in use.
  bool GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info);

  // Enable or disable SSL.
  static void SetSSLMode(bool enable) { use_ssl_ = enable; }
  static bool SSLMode() { return use_ssl_; }

  // Enable or disable flow control.
  static void set_flow_control(bool enable) { use_flow_control_ = enable; }
  static bool flow_control() { return use_flow_control_; }

  // Sets the max concurrent streams per session.
  static void set_max_concurrent_streams(size_t value) {
    max_concurrent_stream_limit_ = value;
  }
  static size_t max_concurrent_streams() {
      return max_concurrent_stream_limit_;
  }

  // Send WINDOW_UPDATE frame, called by a stream whenever receive window
  // size is increased.
  void SendWindowUpdate(spdy::SpdyStreamId stream_id, int delta_window_size);

  // If session is closed, no new streams/transactions should be created.
  bool IsClosed() const { return state_ == CLOSED; }

  // Closes this session.  This will close all active streams and mark
  // the session as permanently closed.
  // |err| should not be OK; this function is intended to be called on
  // error.
  // |remove_from_pool| indicates whether to also remove the session from the
  // session pool.
  void CloseSessionOnError(net::Error err, bool remove_from_pool);

  // Retrieves information on the current state of the SPDY session as a
  // Value.  Caller takes possession of the returned value.
  Value* GetInfoAsValue() const;

  // Indicates whether the session is being reused after having successfully
  // used to send/receive data in the past.
  bool IsReused() const {
    return frames_received_ > 0;
  }

  // Returns true if the underlying transport socket ever had any reads or
  // writes.
  bool WasEverUsed() const {
    return connection_->socket()->WasEverUsed();
  }

  void set_spdy_session_pool(SpdySessionPool* pool) {
    spdy_session_pool_ = NULL;
  }

  // Access to the number of active and pending streams.  These are primarily
  // available for testing and diagnostics.
  size_t num_active_streams() const { return active_streams_.size(); }
  size_t num_unclaimed_pushed_streams() const {
      return unclaimed_pushed_streams_.size();
  }

  const BoundNetLog& net_log() const { return net_log_; }

  int GetPeerAddress(AddressList* address) const {
    return connection_->socket()->GetPeerAddress(address);
  }

 private:
  friend class base::RefCounted<SpdySession>;
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, GetActivePushStream);

  struct PendingCreateStream {
    PendingCreateStream(const GURL& url, RequestPriority priority,
                        scoped_refptr<SpdyStream>* spdy_stream,
                        const BoundNetLog& stream_net_log,
                        CompletionCallback* callback)
        : url(&url), priority(priority), spdy_stream(spdy_stream),
          stream_net_log(&stream_net_log), callback(callback) { }

    const GURL* url;
    RequestPriority priority;
    scoped_refptr<SpdyStream>* spdy_stream;
    const BoundNetLog* stream_net_log;
    CompletionCallback* callback;
  };
  typedef std::queue<PendingCreateStream, std::list< PendingCreateStream> >
      PendingCreateStreamQueue;
  typedef std::map<int, scoped_refptr<SpdyStream> > ActiveStreamMap;
  // Only HTTP push a stream.
  typedef std::map<std::string, scoped_refptr<SpdyStream> > PushedStreamMap;
  typedef std::priority_queue<SpdyIOBuffer> OutputQueue;

  struct CallbackResultPair {
    CallbackResultPair() : callback(NULL), result(OK) {}
    CallbackResultPair(CompletionCallback* callback_in, int result_in)
        : callback(callback_in), result(result_in) {}

    CompletionCallback* callback;
    int result;
  };

  typedef std::map<const scoped_refptr<SpdyStream>*, CallbackResultPair>
      PendingCallbackMap;

  enum State {
    IDLE,
    CONNECTING,
    CONNECTED,
    CLOSED
  };

  enum { kDefaultMaxConcurrentStreams = 10 };

  virtual ~SpdySession();

  void ProcessPendingCreateStreams();
  int CreateStreamImpl(
      const GURL& url,
      RequestPriority priority,
      scoped_refptr<SpdyStream>* spdy_stream,
      const BoundNetLog& stream_net_log);

  // Control frame handlers.
  void OnSyn(const spdy::SpdySynStreamControlFrame& frame,
             const linked_ptr<spdy::SpdyHeaderBlock>& headers);
  void OnSynReply(const spdy::SpdySynReplyControlFrame& frame,
                  const linked_ptr<spdy::SpdyHeaderBlock>& headers);
  void OnHeaders(const spdy::SpdyHeadersControlFrame& frame,
                 const linked_ptr<spdy::SpdyHeaderBlock>& headers);
  void OnRst(const spdy::SpdyRstStreamControlFrame& frame);
  void OnGoAway(const spdy::SpdyGoAwayControlFrame& frame);
  void OnSettings(const spdy::SpdySettingsControlFrame& frame);
  void OnWindowUpdate(const spdy::SpdyWindowUpdateControlFrame& frame);

  // IO Callbacks
  void OnReadComplete(int result);
  void OnWriteComplete(int result);

  // Send relevant SETTINGS.  This is generally called on connection setup.
  void SendSettings();

  // Handle SETTINGS.  Either when we send settings, or when we receive a
  // SETTINGS ontrol frame, update our SpdySession accordingly.
  void HandleSettings(const spdy::SpdySettings& settings);

  // Start reading from the socket.
  // Returns OK on success, or an error on failure.
  net::Error ReadSocket();

  // Write current data to the socket.
  void WriteSocketLater();
  void WriteSocket();

  // Get a new stream id.
  int GetNewStreamId();

  // Queue a frame for sending.
  // |frame| is the frame to send.
  // |priority| is the priority for insertion into the queue.
  // |stream| is the stream which this IO is associated with (or NULL).
  void QueueFrame(spdy::SpdyFrame* frame, spdy::SpdyPriority priority,
                  SpdyStream* stream);

  // Track active streams in the active stream list.
  void ActivateStream(SpdyStream* stream);
  void DeleteStream(spdy::SpdyStreamId id, int status);

  // Removes this session from the session pool.
  void RemoveFromPool();

  // Check if we have a pending pushed-stream for this url
  // Returns the stream if found (and returns it from the pending
  // list), returns NULL otherwise.
  scoped_refptr<SpdyStream> GetActivePushStream(const std::string& url);

  // Calls OnResponseReceived().
  // Returns true if successful.
  bool Respond(const spdy::SpdyHeaderBlock& headers,
               const scoped_refptr<SpdyStream> stream);

  void RecordHistograms();

  // Closes all streams.  Used as part of shutdown.
  void CloseAllStreams(net::Error status);

  // Invokes a user callback for stream creation.  We provide this method so it
  // can be deferred to the MessageLoop, so we avoid re-entrancy problems.
  void InvokeUserStreamCreationCallback(scoped_refptr<SpdyStream>* stream);

  // SpdyFramerVisitorInterface:
  virtual void OnError(spdy::SpdyFramer*);
  virtual void OnStreamFrameData(spdy::SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len);
  virtual void OnControl(const spdy::SpdyControlFrame* frame);

  // Callbacks for the Spdy session.
  CompletionCallbackImpl<SpdySession> read_callback_;
  CompletionCallbackImpl<SpdySession> write_callback_;

  // Used for posting asynchronous IO tasks.  We use this even though
  // SpdySession is refcounted because we don't need to keep the SpdySession
  // alive if the last reference is within a RunnableMethod.  Just revoke the
  // method.
  ScopedRunnableMethodFactory<SpdySession> method_factory_;

  // Map of the SpdyStreams for which we have a pending Task to invoke a
  // callback.  This is necessary since, before we invoke said callback, it's
  // possible that the request is cancelled.
  PendingCallbackMap pending_callback_map_;

  // The domain this session is connected to.
  const HostPortProxyPair host_port_proxy_pair_;

  // |spdy_session_pool_| owns us, therefore its lifetime must exceed ours.  We
  // set this to NULL after we are removed from the pool.
  SpdySessionPool* spdy_session_pool_;
  SpdySettingsStorage* const spdy_settings_;

  // The socket handle for this session.
  scoped_ptr<ClientSocketHandle> connection_;

  // The read buffer used to read data from the socket.
  scoped_refptr<IOBuffer> read_buffer_;
  bool read_pending_;

  int stream_hi_water_mark_;  // The next stream id to use.

  // Queue, for each priority, of pending Create Streams that have not
  // yet been satisfied
  PendingCreateStreamQueue create_stream_queues_[NUM_PRIORITIES];

  // Map from stream id to all active streams.  Streams are active in the sense
  // that they have a consumer (typically SpdyNetworkTransaction and regardless
  // of whether or not there is currently any ongoing IO [might be waiting for
  // the server to start pushing the stream]) or there are still network events
  // incoming even though the consumer has already gone away (cancellation).
  // TODO(willchan): Perhaps we should separate out cancelled streams and move
  // them into a separate ActiveStreamMap, and not deliver network events to
  // them?
  ActiveStreamMap active_streams_;
  // Map of all the streams that have already started to be pushed by the
  // server, but do not have consumers yet.
  PushedStreamMap unclaimed_pushed_streams_;

  // As we gather data to be sent, we put it into the output queue.
  OutputQueue queue_;

  // The packet we are currently sending.
  bool write_pending_;            // Will be true when a write is in progress.
  SpdyIOBuffer in_flight_write_;  // This is the write buffer in progress.

  // Flag if we have a pending message scheduled for WriteSocket.
  bool delayed_write_pending_;

  // Flag if we're using an SSL connection for this SpdySession.
  bool is_secure_;

  // Certificate error code when using a secure connection.
  int certificate_error_code_;

  // Spdy Frame state.
  spdy::SpdyFramer spdy_framer_;

  // If an error has occurred on the session, the session is effectively
  // dead.  Record this error here.  When no error has occurred, |error_| will
  // be OK.
  net::Error error_;
  State state_;

  // Limits
  size_t max_concurrent_streams_;  // 0 if no limit

  // Some statistics counters for the session.
  int streams_initiated_count_;
  int streams_pushed_count_;
  int streams_pushed_and_claimed_count_;
  int streams_abandoned_count_;
  int frames_received_;
  int bytes_received_;
  bool sent_settings_;      // Did this session send settings when it started.
  bool received_settings_;  // Did this session receive at least one settings
                            // frame.
  int stalled_streams_;     // Count of streams that were ever stalled.

  // Initial send window size for the session; can be changed by an
  // arriving SETTINGS frame; newly created streams use this value for the
  // initial send window size.
  int initial_send_window_size_;

  // Initial receive window size for the session; there are plans to add a
  // command line switch that would cause a SETTINGS frame with window size
  // announcement to be sent on startup; newly created streams will use
  // this value for the initial receive window size.
  int initial_recv_window_size_;

  BoundNetLog net_log_;

  static bool use_ssl_;
  static bool use_flow_control_;
  static size_t max_concurrent_stream_limit_;
};

class NetLogSpdySynParameter : public NetLog::EventParameters {
 public:
  NetLogSpdySynParameter(const linked_ptr<spdy::SpdyHeaderBlock>& headers,
                         spdy::SpdyControlFlags flags,
                         spdy::SpdyStreamId id,
                         spdy::SpdyStreamId associated_stream);

  const linked_ptr<spdy::SpdyHeaderBlock>& GetHeaders() const {
    return headers_;
  }

  virtual Value* ToValue() const;

 private:
  virtual ~NetLogSpdySynParameter();

  const linked_ptr<spdy::SpdyHeaderBlock> headers_;
  const spdy::SpdyControlFlags flags_;
  const spdy::SpdyStreamId id_;
  const spdy::SpdyStreamId associated_stream_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdySynParameter);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_H_
