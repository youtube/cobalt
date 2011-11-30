// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "net/base/io_buffer.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/request_priority.h"
#include "net/base/ssl_config_service.h"
#include "net/base/upload_data_stream.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/stream_socket.h"
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

class NET_EXPORT SpdySession : public base::RefCounted<SpdySession>,
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
              HttpServerProperties* http_server_properties,
              bool verify_domain_authentication,
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
      OldCompletionCallback* callback);

  // Remove PendingCreateStream objects on transaction deletion
  void CancelPendingCreateStreams(const scoped_refptr<SpdyStream>* spdy_stream);

  // Used by SpdySessionPool to initialize with a pre-existing SSL socket. For
  // testing, setting is_secure to false allows initialization with a
  // pre-existing TCP socket.
  // Returns OK on success, or an error on failure.
  net::Error InitializeWithSocket(ClientSocketHandle* connection,
                                  bool is_secure,
                                  int certificate_error_code);

  // Check to see if this SPDY session can support an additional domain.
  // If the session is un-authenticated, then this call always returns true.
  // For SSL-based sessions, verifies that the server certificate in use by
  // this session provides authentication for the domain and no client
  // certificate was sent to the original server during the SSL handshake.
  // NOTE:  This function can have false negatives on some platforms.
  // TODO(wtc): rename this function and the Net.SpdyIPPoolDomainMatch
  // histogram because this function does more than verifying domain
  // authentication now.
  bool VerifyDomainAuthentication(const std::string& domain);

  // Send the SYN frame for |stream_id|. This also sends PING message to check
  // the status of the connection.
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

  // Sets the max concurrent streams per session, as a ceiling on any server
  // specific SETTINGS value.
  static void set_max_concurrent_streams(size_t value) {
    max_concurrent_stream_limit_ = value;
  }
  static size_t max_concurrent_streams() {
    return max_concurrent_stream_limit_;
  }

  // Enable sending of PING frame with each request.
  static void set_enable_ping_based_connection_checking(bool enable) {
    enable_ping_based_connection_checking_ = enable;
  }
  static bool enable_ping_based_connection_checking() {
    return enable_ping_based_connection_checking_;
  }

  // The initial max concurrent streams per session, can be overridden by the
  // server via SETTINGS.
  static void set_init_max_concurrent_streams(size_t value) {
    init_max_concurrent_streams_ =
        std::min(value, max_concurrent_stream_limit_);
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
  base::Value* GetInfoAsValue() const;

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

  // Returns true if session is not currently active
  bool is_active() const {
    return !active_streams_.empty();
  }

  // Access to the number of active and pending streams.  These are primarily
  // available for testing and diagnostics.
  size_t num_active_streams() const { return active_streams_.size(); }
  size_t num_unclaimed_pushed_streams() const {
      return unclaimed_pushed_streams_.size();
  }

  const BoundNetLog& net_log() const { return net_log_; }

  int GetPeerAddress(AddressList* address) const;
  int GetLocalAddress(IPEndPoint* address) const;

 private:
  friend class base::RefCounted<SpdySession>;
  // Allow tests to access our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, Ping);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, FailedPing);
  FRIEND_TEST_ALL_PREFIXES(SpdySessionTest, GetActivePushStream);

  struct PendingCreateStream {
    PendingCreateStream(const GURL& url, RequestPriority priority,
                        scoped_refptr<SpdyStream>* spdy_stream,
                        const BoundNetLog& stream_net_log,
                        OldCompletionCallback* callback)
        : url(&url), priority(priority), spdy_stream(spdy_stream),
          stream_net_log(&stream_net_log), callback(callback) { }

    const GURL* url;
    RequestPriority priority;
    scoped_refptr<SpdyStream>* spdy_stream;
    const BoundNetLog* stream_net_log;
    OldCompletionCallback* callback;
  };
  typedef std::queue<PendingCreateStream, std::list< PendingCreateStream> >
      PendingCreateStreamQueue;
  typedef std::map<int, scoped_refptr<SpdyStream> > ActiveStreamMap;
  // Only HTTP push a stream.
  typedef std::map<std::string, scoped_refptr<SpdyStream> > PushedStreamMap;
  typedef std::priority_queue<SpdyIOBuffer> OutputQueue;

  struct CallbackResultPair {
    CallbackResultPair() : callback(NULL), result(OK) {}
    CallbackResultPair(OldCompletionCallback* callback_in, int result_in)
        : callback(callback_in), result(result_in) {}

    OldCompletionCallback* callback;
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
  void OnPing(const spdy::SpdyPingControlFrame& frame);
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

  // Send the PING (preface-PING and trailing-PING) frames.
  void SendPrefacePingIfNoneInFlight();

  // Send PING if there are no PINGs in flight and we haven't heard from server.
  void SendPrefacePing();

  // Send a PING after delay. Don't post a PING if there is already
  // a trailing PING pending.
  void PlanToSendTrailingPing();

  // Send a PING if there is no |trailing_ping_pending_|. This PING verifies
  // that the requests are being received by the server.
  void SendTrailingPing();

  // Send the PING frame.
  void WritePingFrame(uint32 unique_id);

  // Post a CheckPingStatus call after delay. Don't post if there is already
  // CheckPingStatus running.
  void PlanToCheckPingStatus();

  // Check the status of the connection. It calls |CloseSessionOnError| if we
  // haven't received any data in |kHungInterval| time period.
  void CheckPingStatus(base::TimeTicks last_check_time);

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


  void RecordPingRTTHistogram(base::TimeDelta duration);
  void RecordHistograms();

  // Closes all streams.  Used as part of shutdown.
  void CloseAllStreams(net::Error status);

  // Invokes a user callback for stream creation.  We provide this method so it
  // can be deferred to the MessageLoop, so we avoid re-entrancy problems.
  void InvokeUserStreamCreationCallback(scoped_refptr<SpdyStream>* stream);

  // SpdyFramerVisitorInterface:
  virtual void OnError(spdy::SpdyFramer*) OVERRIDE;
  virtual void OnStreamFrameData(spdy::SpdyStreamId stream_id,
                                 const char* data,
                                 size_t len) OVERRIDE;
  virtual void OnControl(const spdy::SpdyControlFrame* frame) OVERRIDE;

  virtual bool OnControlFrameHeaderData(spdy::SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) OVERRIDE;

  virtual void OnDataFrameHeader(const spdy::SpdyDataFrame* frame) OVERRIDE;

  // --------------------------
  // Helper methods for testing
  // --------------------------
  static void set_connection_at_risk_of_loss_seconds(int duration) {
    connection_at_risk_of_loss_seconds_ = duration;
  }
  static int connection_at_risk_of_loss_seconds() {
    return connection_at_risk_of_loss_seconds_;
  }

  static void set_trailing_ping_delay_time_ms(int duration) {
    trailing_ping_delay_time_ms_ = duration;
  }
  static int trailing_ping_delay_time_ms() {
    return trailing_ping_delay_time_ms_;
  }

  static void set_hung_interval_ms(int duration) {
    hung_interval_ms_ = duration;
  }
  static int hung_interval_ms() {
    return hung_interval_ms_;
  }

  int64 pings_in_flight() const { return pings_in_flight_; }

  uint32 next_ping_id() const { return next_ping_id_; }

  base::TimeTicks received_data_time() const { return received_data_time_; }

  bool trailing_ping_pending() const { return trailing_ping_pending_; }

  bool check_ping_status_pending() const { return check_ping_status_pending_; }

  // Callbacks for the Spdy session.
  OldCompletionCallbackImpl<SpdySession> read_callback_;
  OldCompletionCallbackImpl<SpdySession> write_callback_;

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
  HttpServerProperties* const http_server_properties_;

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

  // Count of all pings on the wire, for which we have not gotten a response.
  int64 pings_in_flight_;

  // This is the next ping_id (unique_id) to be sent in PING frame.
  uint32 next_ping_id_;

  // This is the last time we have sent a PING.
  base::TimeTicks last_ping_sent_time_;

  // This is the last time we have received data.
  base::TimeTicks received_data_time_;

  // Indicate if we have already scheduled a delayed task to send a trailing
  // ping (and we never have more than one scheduled at a time).
  bool trailing_ping_pending_;

  // Indicate if we have already scheduled a delayed task to check the ping
  // status.
  bool check_ping_status_pending_;

  // Indicate if we need to send a ping (generally, a trailing ping). This helps
  // us to decide if we need yet another trailing ping, or if it would be a
  // waste of effort (and MUST not be done).
  bool need_to_send_ping_;

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

  // Outside of tests, this should always be true.
  bool verify_domain_authentication_;

  static bool use_ssl_;
  static bool use_flow_control_;
  static size_t init_max_concurrent_streams_;
  static size_t max_concurrent_stream_limit_;

  // This enables or disables connection health checking system.
  static bool enable_ping_based_connection_checking_;

  // |connection_at_risk_of_loss_seconds_| is an optimization to avoid sending
  // wasteful preface pings (when we just got some data).
  //
  // If it is zero (the most conservative figure), then we always send the
  // preface ping (when none are in flight).
  //
  // It is common for TCP/IP sessions to time out in about 3-5 minutes.
  // Certainly if it has been more than 3 minutes, we do want to send a preface
  // ping.
  //
  // We don't think any connection will time out in under about 10 seconds. So
  // this might as well be set to something conservative like 10 seconds. Later,
  // we could adjust it to send fewer pings perhaps.
  static int connection_at_risk_of_loss_seconds_;

  // This is the amount of time (in milliseconds) we wait before sending a
  // trailing ping. We use a trailing ping (sent after all data) to get an
  // effective acknowlegement from the server that it has indeed received all
  // (prior) data frames. With that assurance, we are willing to enter into a
  // wait state for responses to our last data frame(s) without further pings.
  static int trailing_ping_delay_time_ms_;

  // The amount of time (in milliseconds) that we are willing to tolerate with
  // no data received (of any form), while there is a ping in flight, before we
  // declare the connection to be hung.
  static int hung_interval_ms_;
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

  virtual base::Value* ToValue() const OVERRIDE;

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
