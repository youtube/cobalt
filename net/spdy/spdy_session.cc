// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session.h"

#include "base/basictypes.h"
#include "base/linked_ptr.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stats_counters.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/http/http_network_session.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_settings_storage.h"
#include "net/spdy/spdy_stream.h"

namespace net {

namespace {

const int kReadBufferSize = 8 * 1024;

void AdjustSocketBufferSizes(ClientSocket* socket) {
  // Adjust socket buffer sizes.
  // SPDY uses one socket, and we want a really big buffer.
  // This greatly helps on links with packet loss - we can even
  // outperform Vista's dynamic window sizing algorithm.
  // TODO(mbelshe): more study.
  const int kSocketBufferSize = 512 * 1024;
  socket->SetReceiveBufferSize(kSocketBufferSize);
  socket->SetSendBufferSize(kSocketBufferSize);
}

class NetLogSpdySessionParameter : public NetLog::EventParameters {
 public:
  NetLogSpdySessionParameter(const HostPortProxyPair& host_pair)
      : host_pair_(host_pair) {}
  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->Set("host", new StringValue(host_pair_.first.ToString()));
    dict->Set("proxy", new StringValue(host_pair_.second.ToPacString()));
    return dict;
  }
 private:
  const HostPortProxyPair host_pair_;
  DISALLOW_COPY_AND_ASSIGN(NetLogSpdySessionParameter);
};

class NetLogSpdySynParameter : public NetLog::EventParameters {
 public:
  NetLogSpdySynParameter(const linked_ptr<spdy::SpdyHeaderBlock>& headers,
                         spdy::SpdyControlFlags flags,
                         spdy::SpdyStreamId id)
      : headers_(headers), flags_(flags), id_(id) {}

  Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    ListValue* headers_list = new ListValue();
    for (spdy::SpdyHeaderBlock::const_iterator it = headers_->begin();
         it != headers_->end(); ++it) {
      headers_list->Append(new StringValue(base::StringPrintf(
          "%s: %s", it->first.c_str(), it->second.c_str())));
    }
    dict->SetInteger("flags", flags_);
    dict->Set("headers", headers_list);
    dict->SetInteger("id", id_);
    return dict;
  }

 private:
  ~NetLogSpdySynParameter() {}

  const linked_ptr<spdy::SpdyHeaderBlock> headers_;
  const spdy::SpdyControlFlags flags_;
  const spdy::SpdyStreamId id_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdySynParameter);
};

class NetLogSpdySettingsParameter : public NetLog::EventParameters {
 public:
  explicit NetLogSpdySettingsParameter(const spdy::SpdySettings& settings)
      : settings_(settings) {}

  Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    ListValue* settings = new ListValue();
    for (spdy::SpdySettings::const_iterator it = settings_.begin();
         it != settings_.end(); ++it) {
      settings->Append(new StringValue(
          base::StringPrintf("[%u:%u]", it->first.id(), it->second)));
    }
    dict->Set("settings", settings);
    return dict;
  }

 private:
  ~NetLogSpdySettingsParameter() {}
  const spdy::SpdySettings settings_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdySettingsParameter);
};

class NetLogSpdyWindowUpdateParameter : public NetLog::EventParameters {
 public:
  NetLogSpdyWindowUpdateParameter(spdy::SpdyStreamId stream_id,
                                  int delta,
                                  int window_size)
      : stream_id_(stream_id), delta_(delta), window_size_(window_size) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("stream_id", static_cast<int>(stream_id_));
    dict->SetInteger("delta", delta_);
    dict->SetInteger("window_size", window_size_);
    return dict;
  }

 private:
  ~NetLogSpdyWindowUpdateParameter() {}
  const spdy::SpdyStreamId stream_id_;
  const int delta_;
  const int window_size_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdyWindowUpdateParameter);
};

class NetLogSpdyDataParameter : public NetLog::EventParameters {
 public:
  NetLogSpdyDataParameter(spdy::SpdyStreamId stream_id,
                          int size,
                          spdy::SpdyDataFlags flags)
      : stream_id_(stream_id), size_(size), flags_(flags) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("stream_id", static_cast<int>(stream_id_));
    dict->SetInteger("size", size_);
    dict->SetInteger("flags", static_cast<int>(flags_));
    return dict;
  }

 private:
  ~NetLogSpdyDataParameter() {}
  const spdy::SpdyStreamId stream_id_;
  const int size_;
  const spdy::SpdyDataFlags flags_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdyDataParameter);
};

class NetLogSpdyRstParameter : public NetLog::EventParameters {
 public:
  NetLogSpdyRstParameter(spdy::SpdyStreamId stream_id, int status)
      : stream_id_(stream_id), status_(status) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("stream_id", static_cast<int>(stream_id_));
    dict->SetInteger("status", status_);
    return dict;
  }

 private:
  ~NetLogSpdyRstParameter() {}
  const spdy::SpdyStreamId stream_id_;
  const int status_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdyRstParameter);
};

class NetLogSpdyGoAwayParameter : public NetLog::EventParameters {
 public:
  NetLogSpdyGoAwayParameter(spdy::SpdyStreamId last_stream_id,
                            int active_streams,
                            int unclaimed_streams)
      : last_stream_id_(last_stream_id),
        active_streams_(active_streams),
        unclaimed_streams_(unclaimed_streams) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("last_accepted_stream_id",
                     static_cast<int>(last_stream_id_));
    dict->SetInteger("active_streams", active_streams_);
    dict->SetInteger("unclaimed_streams", unclaimed_streams_);
    return dict;
  }

 private:
  ~NetLogSpdyGoAwayParameter() {}
  const spdy::SpdyStreamId last_stream_id_;
  const int active_streams_;
  const int unclaimed_streams_;

  DISALLOW_COPY_AND_ASSIGN(NetLogSpdyGoAwayParameter);
};

}  // namespace

// static
bool SpdySession::use_ssl_ = true;

// static
bool SpdySession::use_flow_control_ = false;

SpdySession::SpdySession(const HostPortProxyPair& host_port_proxy_pair,
                         SpdySessionPool* spdy_session_pool,
                         SpdySettingsStorage* spdy_settings,
                         NetLog* net_log)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &SpdySession::OnReadComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &SpdySession::OnWriteComplete)),
      host_port_proxy_pair_(host_port_proxy_pair),
      spdy_session_pool_(spdy_session_pool),
      spdy_settings_(spdy_settings),
      connection_(new ClientSocketHandle),
      read_buffer_(new IOBuffer(kReadBufferSize)),
      read_pending_(false),
      stream_hi_water_mark_(1),  // Always start at 1 for the first stream id.
      write_pending_(false),
      delayed_write_pending_(false),
      is_secure_(false),
      certificate_error_code_(OK),
      error_(OK),
      state_(IDLE),
      max_concurrent_streams_(kDefaultMaxConcurrentStreams),
      streams_initiated_count_(0),
      streams_pushed_count_(0),
      streams_pushed_and_claimed_count_(0),
      streams_abandoned_count_(0),
      frames_received_(0),
      sent_settings_(false),
      received_settings_(false),
      in_session_pool_(true),
      initial_send_window_size_(spdy::kInitialWindowSize),
      initial_recv_window_size_(spdy::kInitialWindowSize),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SPDY_SESSION)) {
  net_log_.BeginEvent(
      NetLog::TYPE_SPDY_SESSION,
      new NetLogSpdySessionParameter(host_port_proxy_pair_));

  // TODO(mbelshe): consider randomization of the stream_hi_water_mark.

  spdy_framer_.set_visitor(this);

  SendSettings();
}

SpdySession::~SpdySession() {
  state_ = CLOSED;

  // Cleanup all the streams.
  CloseAllStreams(net::ERR_ABORTED);

  if (connection_->is_initialized()) {
    // With Spdy we can't recycle sockets.
    connection_->socket()->Disconnect();
  }

  // Streams should all be gone now.
  DCHECK_EQ(0u, num_active_streams());
  DCHECK_EQ(0u, num_unclaimed_pushed_streams());

  RecordHistograms();

  net_log_.EndEvent(NetLog::TYPE_SPDY_SESSION, NULL);
}

net::Error SpdySession::InitializeWithSocket(
    ClientSocketHandle* connection,
    bool is_secure,
    int certificate_error_code) {
  static StatsCounter spdy_sessions("spdy.sessions");
  spdy_sessions.Increment();

  AdjustSocketBufferSizes(connection->socket());

  state_ = CONNECTED;
  connection_.reset(connection);
  is_secure_ = is_secure;
  certificate_error_code_ = certificate_error_code;

  // This is a newly initialized session that no client should have a handle to
  // yet, so there's no need to start writing data as in OnTCPConnect(), but we
  // should start reading data.
  net::Error error = ReadSocket();
  if (error == ERR_IO_PENDING)
    return OK;
  return error;
}

int SpdySession::GetPushStream(
    const GURL& url,
    scoped_refptr<SpdyStream>* stream,
    const BoundNetLog& stream_net_log) {
  CHECK_NE(state_, CLOSED);

  *stream = NULL;

  // Don't allow access to secure push streams over an unauthenticated, but
  // encrypted SSL socket.
  if (is_secure_ && certificate_error_code_ != OK &&
      (url.SchemeIs("https") || url.SchemeIs("wss"))) {
    LOG(DFATAL) << "Tried to get pushed spdy stream for secure content over an "
                << "unauthenticated session.";
    return certificate_error_code_;
  }

  const std::string& path = url.PathForRequest();

  *stream = GetActivePushStream(path);
  if (stream->get()) {
    DCHECK(streams_pushed_and_claimed_count_ < streams_pushed_count_);
    streams_pushed_and_claimed_count_++;
    return OK;
  }
  return 0;
}

int SpdySession::CreateStream(
    const GURL& url,
    RequestPriority priority,
    scoped_refptr<SpdyStream>* spdy_stream,
    const BoundNetLog& stream_net_log,
    CompletionCallback* callback) {
  if (!max_concurrent_streams_ ||
      active_streams_.size() < max_concurrent_streams_) {
    return CreateStreamImpl(url, priority, spdy_stream, stream_net_log);
  }

  net_log().AddEvent(NetLog::TYPE_SPDY_SESSION_STALLED_MAX_STREAMS, NULL);
  create_stream_queues_[priority].push(
      PendingCreateStream(url, priority, spdy_stream,
                          stream_net_log, callback));
  return ERR_IO_PENDING;
}

void SpdySession::ProcessPendingCreateStreams() {
  while (!max_concurrent_streams_ ||
         active_streams_.size() < max_concurrent_streams_) {
    bool no_pending_create_streams = true;
    for (int i = 0;i < NUM_PRIORITIES;++i) {
      if (!create_stream_queues_[i].empty()) {
        PendingCreateStream pending_create = create_stream_queues_[i].front();
        create_stream_queues_[i].pop();
        no_pending_create_streams = false;
        int error = CreateStreamImpl(*pending_create.url,
                                     pending_create.priority,
                                     pending_create.spdy_stream,
                                     *pending_create.stream_net_log);
        pending_create.callback->Run(error);
        break;
      }
    }
    if (no_pending_create_streams)
      return;  // there were no streams in any queue
  }
}

void SpdySession::CancelPendingCreateStreams(
    const scoped_refptr<SpdyStream>* spdy_stream) {
  for (int i = 0;i < NUM_PRIORITIES;++i) {
    PendingCreateStreamQueue tmp;
    // Make a copy removing this trans
    while (!create_stream_queues_[i].empty()) {
      PendingCreateStream pending_create = create_stream_queues_[i].front();
      create_stream_queues_[i].pop();
      if (pending_create.spdy_stream != spdy_stream)
        tmp.push(pending_create);
    }
    // Now copy it back
    while (!tmp.empty()) {
      create_stream_queues_[i].push(tmp.front());
      tmp.pop();
    }
  }
}

int SpdySession::CreateStreamImpl(
    const GURL& url,
    RequestPriority priority,
    scoped_refptr<SpdyStream>* spdy_stream,
    const BoundNetLog& stream_net_log) {
  // Make sure that we don't try to send https/wss over an unauthenticated, but
  // encrypted SSL socket.
  if (is_secure_ && certificate_error_code_ != OK &&
      (url.SchemeIs("https") || url.SchemeIs("wss"))) {
    LOG(DFATAL) << "Tried to create spdy stream for secure content over an "
                << "unauthenticated session.";
    return certificate_error_code_;
  }

  const std::string& path = url.PathForRequest();

  const spdy::SpdyStreamId stream_id = GetNewStreamId();

  *spdy_stream = new SpdyStream(this,
                                stream_id,
                                false,
                                stream_net_log);
  const scoped_refptr<SpdyStream>& stream = *spdy_stream;

  stream->set_priority(priority);
  stream->set_path(path);
  stream->set_send_window_size(initial_send_window_size_);
  stream->set_recv_window_size(initial_recv_window_size_);
  ActivateStream(stream);

  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyPriorityCount",
      static_cast<int>(priority), 0, 10, 11);

  // TODO(mbelshe): Optimize memory allocations
  DCHECK(priority >= net::HIGHEST && priority < net::NUM_PRIORITIES);

  DCHECK_EQ(active_streams_[stream_id].get(), stream.get());
  return OK;
}

int SpdySession::WriteSynStream(
    spdy::SpdyStreamId stream_id,
    RequestPriority priority,
    spdy::SpdyControlFlags flags,
    const linked_ptr<spdy::SpdyHeaderBlock>& headers) {
  // Find our stream
  if (!IsStreamActive(stream_id))
    return ERR_INVALID_SPDY_STREAM;
  const scoped_refptr<SpdyStream>& stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);

  scoped_ptr<spdy::SpdySynStreamControlFrame> syn_frame(
      spdy_framer_.CreateSynStream(
          stream_id, 0,
          ConvertRequestPriorityToSpdyPriority(priority),
          flags, false, headers.get()));
  QueueFrame(syn_frame.get(), priority, stream);

  static StatsCounter spdy_requests("spdy.requests");
  spdy_requests.Increment();
  streams_initiated_count_++;

  if (net_log().IsLoggingAll()) {
    net_log().AddEvent(
        NetLog::TYPE_SPDY_SESSION_SYN_STREAM,
        new NetLogSpdySynParameter(headers, flags, stream_id));
  }

  return ERR_IO_PENDING;
}

int SpdySession::WriteStreamData(spdy::SpdyStreamId stream_id,
                                 net::IOBuffer* data, int len,
                                 spdy::SpdyDataFlags flags) {
  // Find our stream
  DCHECK(IsStreamActive(stream_id));
  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);
  if (!stream)
    return ERR_INVALID_SPDY_STREAM;

  if (len > kMaxSpdyFrameChunkSize) {
    len = kMaxSpdyFrameChunkSize;
    flags = static_cast<spdy::SpdyDataFlags>(flags & ~spdy::DATA_FLAG_FIN);
  }

  // Obey send window size of the stream if flow control is enabled.
  if (use_flow_control_) {
    if (stream->send_window_size() <= 0) {
      // Because we queue frames onto the session, it is possible that
      // a stream was not flow controlled at the time it attempted the
      // write, but when we go to fulfill the write, it is now flow
      // controlled.  This is why we need the session to mark the stream
      // as stalled - because only the session knows for sure when the
      // stall occurs.
      stream->set_stalled_by_flow_control(true);
      net_log().AddEvent(NetLog::TYPE_SPDY_SESSION_STALLED_ON_SEND_WINDOW,
                         new NetLogIntegerParameter("stream_id", stream_id));
      return ERR_IO_PENDING;
    }
    int new_len = std::min(len, stream->send_window_size());
    if (new_len < len) {
      len = new_len;
      flags = static_cast<spdy::SpdyDataFlags>(flags & ~spdy::DATA_FLAG_FIN);
    }
    stream->DecreaseSendWindowSize(len);
  }

  if (net_log().IsLoggingAll())
    net_log().AddEvent(NetLog::TYPE_SPDY_SESSION_SEND_DATA,
                       new NetLogSpdyDataParameter(stream_id, len, flags));

  // TODO(mbelshe): reduce memory copies here.
  scoped_ptr<spdy::SpdyDataFrame> frame(
      spdy_framer_.CreateDataFrame(stream_id, data->data(), len, flags));
  QueueFrame(frame.get(), stream->priority(), stream);
  return ERR_IO_PENDING;
}

void SpdySession::CloseStream(spdy::SpdyStreamId stream_id, int status) {
  // TODO(mbelshe): We should send a RST_STREAM control frame here
  //                so that the server can cancel a large send.

  DeleteStream(stream_id, status);
}

void SpdySession::ResetStream(
    spdy::SpdyStreamId stream_id, spdy::SpdyStatusCodes status) {

  net_log().AddEvent(
      NetLog::TYPE_SPDY_SESSION_SEND_RST_STREAM,
      new NetLogSpdyRstParameter(stream_id, status));

  scoped_ptr<spdy::SpdyRstStreamControlFrame> rst_frame(
      spdy_framer_.CreateRstStream(stream_id, status));

  // Default to lowest priority unless we know otherwise.
  int priority = 3;
  if(IsStreamActive(stream_id)) {
    scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
    priority = stream->priority();
  }
  QueueFrame(rst_frame.get(), priority, NULL);

  DeleteStream(stream_id, ERR_SPDY_PROTOCOL_ERROR);
}

bool SpdySession::IsStreamActive(spdy::SpdyStreamId stream_id) const {
  return ContainsKey(active_streams_, stream_id);
}

LoadState SpdySession::GetLoadState() const {
  // NOTE: The application only queries the LoadState via the
  //       SpdyNetworkTransaction, and details are only needed when
  //       we're in the process of connecting.

  // If we're connecting, defer to the connection to give us the actual
  // LoadState.
  if (state_ == CONNECTING)
    return connection_->GetLoadState();

  // Just report that we're idle since the session could be doing
  // many things concurrently.
  return LOAD_STATE_IDLE;
}

void SpdySession::OnReadComplete(int bytes_read) {
  // Parse a frame.  For now this code requires that the frame fit into our
  // buffer (32KB).
  // TODO(mbelshe): support arbitrarily large frames!

  read_pending_ = false;

  if (bytes_read <= 0) {
    // Session is tearing down.
    net::Error error = static_cast<net::Error>(bytes_read);
    if (bytes_read == 0)
      error = ERR_CONNECTION_CLOSED;
    CloseSessionOnError(error, true);
    return;
  }

  // The SpdyFramer will use callbacks onto |this| as it parses frames.
  // When errors occur, those callbacks can lead to teardown of all references
  // to |this|, so maintain a reference to self during this call for safe
  // cleanup.
  scoped_refptr<SpdySession> self(this);

  char *data = read_buffer_->data();
  while (bytes_read &&
         spdy_framer_.error_code() == spdy::SpdyFramer::SPDY_NO_ERROR) {
    uint32 bytes_processed = spdy_framer_.ProcessInput(data, bytes_read);
    bytes_read -= bytes_processed;
    data += bytes_processed;
    if (spdy_framer_.state() == spdy::SpdyFramer::SPDY_DONE)
      spdy_framer_.Reset();
  }

  if (state_ != CLOSED)
    ReadSocket();
}

void SpdySession::OnWriteComplete(int result) {
  DCHECK(write_pending_);
  DCHECK(in_flight_write_.size());

  write_pending_ = false;

  scoped_refptr<SpdyStream> stream = in_flight_write_.stream();

  if (result >= 0) {
    // It should not be possible to have written more bytes than our
    // in_flight_write_.
    DCHECK_LE(result, in_flight_write_.buffer()->BytesRemaining());

    in_flight_write_.buffer()->DidConsume(result);

    // We only notify the stream when we've fully written the pending frame.
    if (!in_flight_write_.buffer()->BytesRemaining()) {
      if (stream) {
        // Report the number of bytes written to the caller, but exclude the
        // frame size overhead.  NOTE: if this frame was compressed the
        // reported bytes written is the compressed size, not the original
        // size.
        if (result > 0) {
          result = in_flight_write_.buffer()->size();
          DCHECK_GT(result, static_cast<int>(spdy::SpdyFrame::size()));
          result -= static_cast<int>(spdy::SpdyFrame::size());
        }

        // It is possible that the stream was cancelled while we were writing
        // to the socket.
        if (!stream->cancelled())
          stream->OnWriteComplete(result);
      }

      // Cleanup the write which just completed.
      in_flight_write_.release();
    }

    // Write more data.  We're already in a continuation, so we can
    // go ahead and write it immediately (without going back to the
    // message loop).
    WriteSocketLater();
  } else {
    in_flight_write_.release();

    // The stream is now errored.  Close it down.
    CloseSessionOnError(static_cast<net::Error>(result), true);
  }
}

net::Error SpdySession::ReadSocket() {
  if (read_pending_)
    return OK;

  if (state_ == CLOSED) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  CHECK(connection_.get());
  CHECK(connection_->socket());
  int bytes_read = connection_->socket()->Read(read_buffer_.get(),
                                               kReadBufferSize,
                                               &read_callback_);
  switch (bytes_read) {
    case 0:
      // Socket is closed!
      CloseSessionOnError(ERR_CONNECTION_CLOSED, true);
      return ERR_CONNECTION_CLOSED;
    case net::ERR_IO_PENDING:
      // Waiting for data.  Nothing to do now.
      read_pending_ = true;
      return ERR_IO_PENDING;
    default:
      // Data was read, process it.
      // Schedule the work through the message loop to avoid recursive
      // callbacks.
      read_pending_ = true;
      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &SpdySession::OnReadComplete, bytes_read));
      break;
  }
  return OK;
}

void SpdySession::WriteSocketLater() {
  if (delayed_write_pending_)
    return;

  if (state_ < CONNECTED)
    return;

  delayed_write_pending_ = true;
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SpdySession::WriteSocket));
}

void SpdySession::WriteSocket() {
  // This function should only be called via WriteSocketLater.
  DCHECK(delayed_write_pending_);
  delayed_write_pending_ = false;

  // If the socket isn't connected yet, just wait; we'll get called
  // again when the socket connection completes.  If the socket is
  // closed, just return.
  if (state_ < CONNECTED || state_ == CLOSED)
    return;

  if (write_pending_)   // Another write is in progress still.
    return;

  // Loop sending frames until we've sent everything or until the write
  // returns error (or ERR_IO_PENDING).
  while (in_flight_write_.buffer() || !queue_.empty()) {
    if (!in_flight_write_.buffer()) {
      // Grab the next SpdyFrame to send.
      SpdyIOBuffer next_buffer = queue_.top();
      queue_.pop();

      // We've deferred compression until just before we write it to the socket,
      // which is now.  At this time, we don't compress our data frames.
      spdy::SpdyFrame uncompressed_frame(next_buffer.buffer()->data(), false);
      size_t size;
      if (spdy_framer_.IsCompressible(uncompressed_frame)) {
        scoped_ptr<spdy::SpdyFrame> compressed_frame(
            spdy_framer_.CompressFrame(uncompressed_frame));
        if (!compressed_frame.get()) {
          LOG(ERROR) << "SPDY Compression failure";
          CloseSessionOnError(net::ERR_SPDY_PROTOCOL_ERROR, true);
          return;
        }

        size = compressed_frame->length() + spdy::SpdyFrame::size();

        DCHECK_GT(size, 0u);

        // TODO(mbelshe): We have too much copying of data here.
        IOBufferWithSize* buffer = new IOBufferWithSize(size);
        memcpy(buffer->data(), compressed_frame->data(), size);

        // Attempt to send the frame.
        in_flight_write_ = SpdyIOBuffer(buffer, size, 0, next_buffer.stream());
      } else {
        size = uncompressed_frame.length() + spdy::SpdyFrame::size();
        in_flight_write_ = next_buffer;
      }
    } else {
      DCHECK(in_flight_write_.buffer()->BytesRemaining());
    }

    write_pending_ = true;
    int rv = connection_->socket()->Write(in_flight_write_.buffer(),
        in_flight_write_.buffer()->BytesRemaining(), &write_callback_);
    if (rv == net::ERR_IO_PENDING)
      break;

    // We sent the frame successfully.
    OnWriteComplete(rv);

    // TODO(mbelshe):  Test this error case.  Maybe we should mark the socket
    //                 as in an error state.
    if (rv < 0)
      break;
  }
}

void SpdySession::CloseAllStreams(net::Error status) {
  static StatsCounter abandoned_streams("spdy.abandoned_streams");
  static StatsCounter abandoned_push_streams("spdy.abandoned_push_streams");

  if (!active_streams_.empty())
    abandoned_streams.Add(active_streams_.size());
  if (!unclaimed_pushed_streams_.empty()) {
    streams_abandoned_count_ += unclaimed_pushed_streams_.size();
    abandoned_push_streams.Add(unclaimed_pushed_streams_.size());
    unclaimed_pushed_streams_.clear();
  }

  for (int i = 0;i < NUM_PRIORITIES;++i) {
    while (!create_stream_queues_[i].empty()) {
      PendingCreateStream pending_create = create_stream_queues_[i].front();
      create_stream_queues_[i].pop();
      pending_create.callback->Run(ERR_ABORTED);
    }
  }

  while (!active_streams_.empty()) {
    ActiveStreamMap::iterator it = active_streams_.begin();
    const scoped_refptr<SpdyStream>& stream = it->second;
    DCHECK(stream);
    LOG(WARNING) << "ABANDONED (stream_id=" << stream->stream_id()
                 << "): " << stream->path();
    DeleteStream(stream->stream_id(), status);
  }

  // We also need to drain the queue.
  while (queue_.size())
    queue_.pop();
}

int SpdySession::GetNewStreamId() {
  int id = stream_hi_water_mark_;
  stream_hi_water_mark_ += 2;
  if (stream_hi_water_mark_ > 0x7fff)
    stream_hi_water_mark_ = 1;
  return id;
}

void SpdySession::QueueFrame(spdy::SpdyFrame* frame,
                             spdy::SpdyPriority priority,
                             SpdyStream* stream) {
  int length = spdy::SpdyFrame::size() + frame->length();
  IOBuffer* buffer = new IOBuffer(length);
  memcpy(buffer->data(), frame->data(), length);
  queue_.push(SpdyIOBuffer(buffer, length, priority, stream));

  WriteSocketLater();
}

void SpdySession::CloseSessionOnError(net::Error err, bool remove_from_pool) {
  // Closing all streams can have a side-effect of dropping the last reference
  // to |this|.  Hold a reference through this function.
  scoped_refptr<SpdySession> self(this);

  DCHECK_LT(err, OK);
  net_log_.AddEvent(NetLog::TYPE_SPDY_SESSION_CLOSE,
                    new NetLogIntegerParameter("status", err));

  // Don't close twice.  This can occur because we can have both
  // a read and a write outstanding, and each can complete with
  // an error.
  if (state_ != CLOSED) {
    state_ = CLOSED;
    error_ = err;
    if (remove_from_pool)
      RemoveFromPool();
    CloseAllStreams(err);
  }
}

void SpdySession::ActivateStream(SpdyStream* stream) {
  const spdy::SpdyStreamId id = stream->stream_id();
  DCHECK(!IsStreamActive(id));

  active_streams_[id] = stream;
}

void SpdySession::DeleteStream(spdy::SpdyStreamId id, int status) {
  // For push streams, if they are being deleted normally, we leave
  // the stream in the unclaimed_pushed_streams_ list.  However, if
  // the stream is errored out, clean it up entirely.
  if (status != OK) {
    PushedStreamMap::iterator it;
    for (it = unclaimed_pushed_streams_.begin();
         it != unclaimed_pushed_streams_.end(); ++it) {
      scoped_refptr<SpdyStream> curr = it->second;
      if (id == curr->stream_id()) {
        unclaimed_pushed_streams_.erase(it);
        break;
      }
    }
  }

  // The stream might have been deleted.
  ActiveStreamMap::iterator it2 = active_streams_.find(id);
  if (it2 == active_streams_.end())
    return;

  // If this is an active stream, call the callback.
  const scoped_refptr<SpdyStream> stream(it2->second);
  active_streams_.erase(it2);
  if (stream)
    stream->OnClose(status);
  ProcessPendingCreateStreams();
}

void SpdySession::RemoveFromPool() {
  if (in_session_pool_) {
    spdy_session_pool_->Remove(this);
    in_session_pool_ = false;
  }
}

scoped_refptr<SpdyStream> SpdySession::GetActivePushStream(
    const std::string& path) {
  static StatsCounter used_push_streams("spdy.claimed_push_streams");

  PushedStreamMap::iterator it = unclaimed_pushed_streams_.find(path);
  if (it != unclaimed_pushed_streams_.end()) {
    net_log_.AddEvent(NetLog::TYPE_SPDY_STREAM_ADOPTED_PUSH_STREAM, NULL);
    scoped_refptr<SpdyStream> stream = it->second;
    unclaimed_pushed_streams_.erase(it);
    used_push_streams.Increment();
    return stream;
  }
  else {
    return NULL;
  }
}

bool SpdySession::GetSSLInfo(SSLInfo* ssl_info, bool* was_npn_negotiated) {
  if (is_secure_) {
    SSLClientSocket* ssl_socket =
        reinterpret_cast<SSLClientSocket*>(connection_->socket());
    ssl_socket->GetSSLInfo(ssl_info);
    *was_npn_negotiated = ssl_socket->was_npn_negotiated();
    return true;
  }
  return false;
}

bool SpdySession::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  if (is_secure_) {
    SSLClientSocket* ssl_socket =
        reinterpret_cast<SSLClientSocket*>(connection_->socket());
    ssl_socket->GetSSLCertRequestInfo(cert_request_info);
    return true;
  }
  return false;
}

void SpdySession::OnError(spdy::SpdyFramer* framer) {
  CloseSessionOnError(net::ERR_SPDY_PROTOCOL_ERROR, true);
}

void SpdySession::OnStreamFrameData(spdy::SpdyStreamId stream_id,
                                    const char* data,
                                    size_t len) {
  if (net_log().IsLoggingAll())
    net_log().AddEvent(NetLog::TYPE_SPDY_SESSION_RECV_DATA,
        new NetLogSpdyDataParameter(stream_id, len, spdy::SpdyDataFlags()));

  if (!IsStreamActive(stream_id)) {
    // NOTE:  it may just be that the stream was cancelled.
    LOG(WARNING) << "Received data frame for invalid stream " << stream_id;
    return;
  }

  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  stream->OnDataReceived(data, len);
}

bool SpdySession::Respond(const spdy::SpdyHeaderBlock& headers,
                          const scoped_refptr<SpdyStream> stream) {
  int rv = OK;

  rv = stream->OnResponseReceived(headers);
  if (rv < 0) {
    DCHECK_NE(rv, ERR_IO_PENDING);
    const spdy::SpdyStreamId stream_id = stream->stream_id();
    DeleteStream(stream_id, rv);
    return false;
  }
  return true;
}

void SpdySession::OnSyn(const spdy::SpdySynStreamControlFrame& frame,
                        const linked_ptr<spdy::SpdyHeaderBlock>& headers) {
  spdy::SpdyStreamId stream_id = frame.stream_id();
  spdy::SpdyStreamId associated_stream_id = frame.associated_stream_id();

  if (net_log_.IsLoggingAll()) {
    net_log_.AddEvent(
        NetLog::TYPE_SPDY_SESSION_PUSHED_SYN_STREAM,
        new NetLogSpdySynParameter(
            headers, static_cast<spdy::SpdyControlFlags>(frame.flags()),
            stream_id));
  }

  // Server-initiated streams should have even sequence numbers.
  if ((stream_id & 0x1) != 0) {
    LOG(ERROR) << "Received invalid OnSyn stream id " << stream_id;
    return;
  }

  if (IsStreamActive(stream_id)) {
    LOG(ERROR) << "Received OnSyn for active stream " << stream_id;
    return;
  }

  if (associated_stream_id == 0) {
    LOG(ERROR) << "Received invalid OnSyn associated stream id "
               << associated_stream_id
               << " for stream " << stream_id;
    ResetStream(stream_id, spdy::INVALID_STREAM);
    return;
  }

  streams_pushed_count_++;

  // TODO(mbelshe): DCHECK that this is a GET method?

  const std::string& path = ContainsKey(*headers, "path") ?
      headers->find("path")->second : "";

  // Verify that the response had a URL for us.
  if (path.empty()) {
    ResetStream(stream_id, spdy::PROTOCOL_ERROR);
    LOG(WARNING) << "Pushed stream did not contain a path.";
    return;
  }

  if (!IsStreamActive(associated_stream_id)) {
    LOG(ERROR) << "Received OnSyn with inactive associated stream "
               << associated_stream_id;
    ResetStream(stream_id, spdy::INVALID_ASSOCIATED_STREAM);
    return;
  }

  // TODO(erikchen): Actually do something with the associated id.

  // There should not be an existing pushed stream with the same path.
  PushedStreamMap::iterator it = unclaimed_pushed_streams_.find(path);
  if (it != unclaimed_pushed_streams_.end()) {
    LOG(ERROR) << "Received duplicate pushed stream with path: " << path;
    ResetStream(stream_id, spdy::PROTOCOL_ERROR);
    return;
  }

  scoped_refptr<SpdyStream> stream =
      new SpdyStream(this, stream_id, true, net_log_);

  stream->set_path(path);

  unclaimed_pushed_streams_[path] = stream;

  ActivateStream(stream);
  stream->set_response_received();

  // Parse the headers.
  if (!Respond(*headers, stream))
    return;

  static StatsCounter push_requests("spdy.pushed_streams");
  push_requests.Increment();
}

void SpdySession::OnSynReply(const spdy::SpdySynReplyControlFrame& frame,
                             const linked_ptr<spdy::SpdyHeaderBlock>& headers) {
  spdy::SpdyStreamId stream_id = frame.stream_id();

  bool valid_stream = IsStreamActive(stream_id);
  if (!valid_stream) {
    // NOTE:  it may just be that the stream was cancelled.
    LOG(WARNING) << "Received SYN_REPLY for invalid stream " << stream_id;
    return;
  }

  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);
  CHECK(!stream->cancelled());

  if (stream->response_received()) {
    LOG(WARNING) << "Received duplicate SYN_REPLY for stream " << stream_id;
    CloseStream(stream->stream_id(), ERR_SPDY_PROTOCOL_ERROR);
    return;
  }
  stream->set_response_received();

  if (net_log().IsLoggingAll()) {
    net_log().AddEvent(
        NetLog::TYPE_SPDY_SESSION_SYN_REPLY,
        new NetLogSpdySynParameter(
            headers, static_cast<spdy::SpdyControlFlags>(frame.flags()),
            stream_id));
  }

  Respond(*headers, stream);
}

void SpdySession::OnControl(const spdy::SpdyControlFrame* frame) {
  const linked_ptr<spdy::SpdyHeaderBlock> headers(new spdy::SpdyHeaderBlock);
  uint32 type = frame->type();
  if (type == spdy::SYN_STREAM || type == spdy::SYN_REPLY) {
    if (!spdy_framer_.ParseHeaderBlock(frame, headers.get())) {
      LOG(WARNING) << "Could not parse Spdy Control Frame Header.";
      int stream_id = 0;
      if (type == spdy::SYN_STREAM)
        stream_id = (reinterpret_cast<const spdy::SpdySynStreamControlFrame*>
                     (frame))->stream_id();
      if (type == spdy::SYN_REPLY)
        stream_id = (reinterpret_cast<const spdy::SpdySynReplyControlFrame*>
                     (frame))->stream_id();
      if(IsStreamActive(stream_id))
        ResetStream(stream_id, spdy::PROTOCOL_ERROR);
      return;
    }
  }

  frames_received_++;

  switch (type) {
    case spdy::GOAWAY:
      OnGoAway(*reinterpret_cast<const spdy::SpdyGoAwayControlFrame*>(frame));
      break;
    case spdy::SETTINGS:
      OnSettings(
          *reinterpret_cast<const spdy::SpdySettingsControlFrame*>(frame));
      break;
    case spdy::RST_STREAM:
      OnRst(*reinterpret_cast<const spdy::SpdyRstStreamControlFrame*>(frame));
      break;
    case spdy::SYN_STREAM:
      OnSyn(*reinterpret_cast<const spdy::SpdySynStreamControlFrame*>(frame),
            headers);
      break;
    case spdy::SYN_REPLY:
      OnSynReply(
          *reinterpret_cast<const spdy::SpdySynReplyControlFrame*>(frame),
          headers);
      break;
    case spdy::WINDOW_UPDATE:
      OnWindowUpdate(
          *reinterpret_cast<const spdy::SpdyWindowUpdateControlFrame*>(frame));
      break;
    default:
      DCHECK(false);  // Error!
  }
}

void SpdySession::OnRst(const spdy::SpdyRstStreamControlFrame& frame) {
  spdy::SpdyStreamId stream_id = frame.stream_id();

  net_log().AddEvent(
      NetLog::TYPE_SPDY_SESSION_RST_STREAM,
      new NetLogSpdyRstParameter(stream_id, frame.status()));

  bool valid_stream = IsStreamActive(stream_id);
  if (!valid_stream) {
    // NOTE:  it may just be that the stream was cancelled.
    LOG(WARNING) << "Received RST for invalid stream" << stream_id;
    return;
  }
  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);
  CHECK(!stream->cancelled());

  if (frame.status() == 0) {
    stream->OnDataReceived(NULL, 0);
  } else {
    LOG(ERROR) << "Spdy stream closed: " << frame.status();
    // TODO(mbelshe): Map from Spdy-protocol errors to something sensical.
    //                For now, it doesn't matter much - it is a protocol error.
    DeleteStream(stream_id, ERR_SPDY_PROTOCOL_ERROR);
  }
}

void SpdySession::OnGoAway(const spdy::SpdyGoAwayControlFrame& frame) {
  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_GOAWAY,
      new NetLogSpdyGoAwayParameter(frame.last_accepted_stream_id(),
                                    active_streams_.size(),
                                    unclaimed_pushed_streams_.size()));
  RemoveFromPool();
  CloseAllStreams(net::ERR_ABORTED);

  // TODO(willchan): Cancel any streams that are past the GoAway frame's
  // |last_accepted_stream_id|.

  // Don't bother killing any streams that are still reading.  They'll either
  // complete successfully or get an ERR_CONNECTION_CLOSED when the socket is
  // closed.
}

void SpdySession::OnSettings(const spdy::SpdySettingsControlFrame& frame) {
  spdy::SpdySettings settings;
  if (spdy_framer_.ParseSettings(&frame, &settings)) {
    HandleSettings(settings);
    spdy_settings_->Set(host_port_pair(), settings);
  }

  received_settings_ = true;

  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_RECV_SETTINGS,
      new NetLogSpdySettingsParameter(settings));
}

void SpdySession::OnWindowUpdate(
    const spdy::SpdyWindowUpdateControlFrame& frame) {
  spdy::SpdyStreamId stream_id = frame.stream_id();
  if (!IsStreamActive(stream_id)) {
    LOG(WARNING) << "Received WINDOW_UPDATE for invalid stream " << stream_id;
    return;
  }

  int delta_window_size = static_cast<int>(frame.delta_window_size());
  if (delta_window_size < 1) {
    LOG(WARNING) << "Received WINDOW_UPDATE with an invalid delta_window_size "
                 << delta_window_size;
    ResetStream(stream_id, spdy::FLOW_CONTROL_ERROR);
    return;
  }

  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);
  CHECK(!stream->cancelled());

  if (use_flow_control_)
    stream->IncreaseSendWindowSize(delta_window_size);

  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_SEND_WINDOW_UPDATE,
      new NetLogSpdyWindowUpdateParameter(stream_id,
                                          delta_window_size,
                                          stream->send_window_size()));
}

void SpdySession::SendWindowUpdate(spdy::SpdyStreamId stream_id,
                                   int delta_window_size) {
  DCHECK(IsStreamActive(stream_id));
  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);

  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_RECV_WINDOW_UPDATE,
      new NetLogSpdyWindowUpdateParameter(stream_id,
                                          delta_window_size,
                                          stream->recv_window_size()));

  scoped_ptr<spdy::SpdyWindowUpdateControlFrame> window_update_frame(
      spdy_framer_.CreateWindowUpdate(stream_id, delta_window_size));
  QueueFrame(window_update_frame.get(), stream->priority(), stream);
}

void SpdySession::SendSettings() {
  const spdy::SpdySettings& settings = spdy_settings_->Get(host_port_pair());
  if (settings.empty())
    return;
  HandleSettings(settings);

  net_log_.AddEvent(
      NetLog::TYPE_SPDY_SESSION_SEND_SETTINGS,
      new NetLogSpdySettingsParameter(settings));

  // Create the SETTINGS frame and send it.
  scoped_ptr<spdy::SpdySettingsControlFrame> settings_frame(
      spdy_framer_.CreateSettings(settings));
  sent_settings_ = true;
  QueueFrame(settings_frame.get(), 0, NULL);
}

void SpdySession::HandleSettings(const spdy::SpdySettings& settings) {
  for (spdy::SpdySettings::const_iterator i = settings.begin(),
           end = settings.end(); i != end; ++i) {
    const uint32 id = i->first.id();
    const uint32 val = i->second;
    switch (id) {
      case spdy::SETTINGS_MAX_CONCURRENT_STREAMS:
        max_concurrent_streams_ = val;
        ProcessPendingCreateStreams();
        break;
    }
  }
}

void SpdySession::RecordHistograms() {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyStreamsPerSession",
                              streams_initiated_count_,
                              0, 300, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyStreamsPushedPerSession",
                              streams_pushed_count_,
                              0, 300, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyStreamsPushedAndClaimedPerSession",
                              streams_pushed_and_claimed_count_,
                              0, 300, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyStreamsAbandonedPerSession",
                              streams_abandoned_count_,
                              0, 300, 50);
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySettingsSent",
                            sent_settings_ ? 1 : 0, 2);
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySettingsReceived",
                            received_settings_ ? 1 : 0, 2);

  if (received_settings_) {
    // Enumerate the saved settings, and set histograms for it.
    const spdy::SpdySettings& settings = spdy_settings_->Get(host_port_pair());

    spdy::SpdySettings::const_iterator it;
    for (it = settings.begin(); it != settings.end(); ++it) {
      const spdy::SpdySetting setting = *it;
      switch (setting.first.id()) {
        case spdy::SETTINGS_CURRENT_CWND:
          UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdySettingsCwnd",
                                      setting.second,
                                      1, 200, 100);
          break;
        case spdy::SETTINGS_ROUND_TRIP_TIME:
          UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdySettingsRTT",
                                      setting.second,
                                      1, 1200, 100);
          break;
        case spdy::SETTINGS_DOWNLOAD_RETRANS_RATE:
          UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdySettingsRetransRate",
                                      setting.second,
                                      1, 100, 50);
          break;
      }
    }
  }
}

}  // namespace net
