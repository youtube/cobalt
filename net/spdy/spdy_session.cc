// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/stats_counters.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/load_flags.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_settings_storage.h"
#include "net/spdy/spdy_stream.h"
#include "net/tools/dump_cache/url_to_filename_encoder.h"

namespace {

// Diagnostics function to dump the headers of a request.
// TODO(mbelshe): Remove this function.
void DumpSpdyHeaders(const spdy::SpdyHeaderBlock& headers) {
  // Because this function gets called on every request,
  // take extra care to optimize it away if logging is turned off.
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;

  spdy::SpdyHeaderBlock::const_iterator it = headers.begin();
  while (it != headers.end()) {
    std::string val = (*it).second;
    std::string::size_type pos = 0;
    while ((pos = val.find('\0', pos)) != val.npos)
      val[pos] = '\n';
    LOG(INFO) << (*it).first << "==" << val;
    ++it;
  }
}

}  // namespace

namespace net {

namespace {

#ifdef WIN32
// We use an artificially small buffer size on windows because the async IO
// system will artifiially delay IO completions when we use large buffers.
const int kReadBufferSize = 2 * 1024;
#else
const int kReadBufferSize = 8 * 1024;
#endif

// Convert a SpdyHeaderBlock into an HttpResponseInfo.
// |headers| input parameter with the SpdyHeaderBlock.
// |info| output parameter for the HttpResponseInfo.
// Returns true if successfully converted.  False if there was a failure
// or if the SpdyHeaderBlock was invalid.
bool SpdyHeadersToHttpResponse(const spdy::SpdyHeaderBlock& headers,
                               HttpResponseInfo* response) {
  std::string version;
  std::string status;

  // The "status" and "version" headers are required.
  spdy::SpdyHeaderBlock::const_iterator it;
  it = headers.find("status");
  if (it == headers.end()) {
    LOG(ERROR) << "SpdyHeaderBlock without status header.";
    return false;
  }
  status = it->second;

  // Grab the version.  If not provided by the server,
  it = headers.find("version");
  if (it == headers.end()) {
    LOG(ERROR) << "SpdyHeaderBlock without version header.";
    return false;
  }
  version = it->second;

  response->response_time = base::Time::Now();

  std::string raw_headers(version);
  raw_headers.push_back(' ');
  raw_headers.append(status);
  raw_headers.push_back('\0');
  for (it = headers.begin(); it != headers.end(); ++it) {
    // For each value, if the server sends a NUL-separated
    // list of values, we separate that back out into
    // individual headers for each value in the list.
    // e.g.
    //    Set-Cookie "foo\0bar"
    // becomes
    //    Set-Cookie: foo\0
    //    Set-Cookie: bar\0
    std::string value = it->second;
    size_t start = 0;
    size_t end = 0;
    do {
      end = value.find('\0', start);
      std::string tval;
      if (end != value.npos)
        tval = value.substr(start, (end - start));
      else
        tval = value.substr(start);
      raw_headers.append(it->first);
      raw_headers.push_back(':');
      raw_headers.append(tval);
      raw_headers.push_back('\0');
      start = end + 1;
    } while (end != value.npos);
  }

  response->headers = new HttpResponseHeaders(raw_headers);
  response->was_fetched_via_spdy = true;
  return true;
}

// Create a SpdyHeaderBlock for a Spdy SYN_STREAM Frame from
// a HttpRequestInfo block.
void CreateSpdyHeadersFromHttpRequest(
    const HttpRequestInfo& info, spdy::SpdyHeaderBlock* headers) {
  // TODO(willchan): It's not really necessary to convert from
  // HttpRequestHeaders to spdy::SpdyHeaderBlock.

  static const char kHttpProtocolVersion[] = "HTTP/1.1";

  HttpRequestHeaders::Iterator it(info.extra_headers);

  while (it.GetNext()) {
    std::string name = StringToLowerASCII(it.name());
    if (headers->find(name) == headers->end()) {
      (*headers)[name] = it.value();
    } else {
      std::string new_value = (*headers)[name];
      new_value.append(1, '\0');  // +=() doesn't append 0's
      new_value += it.value();
      (*headers)[name] = new_value;
    }
  }

  // TODO(mbelshe): Add Proxy headers here. (See http_network_transaction.cc)
  // TODO(mbelshe): Add authentication headers here.

  (*headers)["method"] = info.method;
  (*headers)["url"] = info.url.spec();
  (*headers)["version"] = kHttpProtocolVersion;
  if (!info.referrer.is_empty())
    (*headers)["referer"] = info.referrer.spec();

  // Honor load flags that impact proxy caches.
  if (info.load_flags & LOAD_BYPASS_CACHE) {
    (*headers)["pragma"] = "no-cache";
    (*headers)["cache-control"] = "no-cache";
  } else if (info.load_flags & LOAD_VALIDATE_CACHE) {
    (*headers)["cache-control"] = "max-age=0";
  }
}

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

}  // namespace

// static
bool SpdySession::use_ssl_ = true;

SpdySession::SpdySession(const HostPortPair& host_port_pair,
                         HttpNetworkSession* session)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          connect_callback_(this, &SpdySession::OnTCPConnect)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          ssl_connect_callback_(this, &SpdySession::OnSSLConnect)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &SpdySession::OnReadComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          write_callback_(this, &SpdySession::OnWriteComplete)),
      host_port_pair_(host_port_pair),
      session_(session),
      connection_(new ClientSocketHandle),
      read_buffer_(new IOBuffer(kReadBufferSize)),
      read_pending_(false),
      stream_hi_water_mark_(1),  // Always start at 1 for the first stream id.
      write_pending_(false),
      delayed_write_pending_(false),
      is_secure_(false),
      error_(OK),
      state_(IDLE),
      streams_initiated_count_(0),
      streams_pushed_count_(0),
      streams_pushed_and_claimed_count_(0),
      streams_abandoned_count_(0),
      in_session_pool_(true) {
  // TODO(mbelshe): consider randomization of the stream_hi_water_mark.

  spdy_framer_.set_visitor(this);

  session_->ssl_config_service()->GetSSLConfig(&ssl_config_);

  SendSettings();
}

SpdySession::~SpdySession() {
  // Cleanup all the streams.
  CloseAllStreams(net::ERR_ABORTED);

  if (connection_->is_initialized()) {
    // With Spdy we can't recycle sockets.
    connection_->socket()->Disconnect();
  }

  // Record per-session histograms here.
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
}

void SpdySession::InitializeWithSSLSocket(ClientSocketHandle* connection) {
  static StatsCounter spdy_sessions("spdy.sessions");
  spdy_sessions.Increment();

  AdjustSocketBufferSizes(connection->socket());

  state_ = CONNECTED;
  connection_.reset(connection);
  is_secure_ = true;  // |connection| contains an SSLClientSocket.

  // This is a newly initialized session that no client should have a handle to
  // yet, so there's no need to start writing data as in OnTCPConnect(), but we
  // should start reading data.
  ReadSocket();
}

net::Error SpdySession::Connect(const std::string& group_name,
                                const TCPSocketParams& destination,
                                RequestPriority priority,
                                const BoundNetLog& net_log) {
  DCHECK(priority >= SPDY_PRIORITY_HIGHEST && priority <= SPDY_PRIORITY_LOWEST);

  // If the connect process is started, let the caller continue.
  if (state_ > IDLE)
    return net::OK;

  state_ = CONNECTING;

  static StatsCounter spdy_sessions("spdy.sessions");
  spdy_sessions.Increment();

  int rv = connection_->Init(group_name, destination, priority,
                             &connect_callback_, session_->tcp_socket_pool(),
                             net_log);
  DCHECK(rv <= 0);

  // If the connect is pending, we still return ok.  The APIs enqueue
  // work until after the connect completes asynchronously later.
  if (rv == net::ERR_IO_PENDING)
    return net::OK;
  OnTCPConnect(rv);
  return static_cast<net::Error>(rv);
}

scoped_refptr<SpdyStream> SpdySession::GetOrCreateStream(
    const HttpRequestInfo& request,
    const UploadDataStream* upload_data,
    const BoundNetLog& log) {
  const GURL& url = request.url;
  const std::string& path = url.PathForRequest();

  scoped_refptr<SpdyStream> stream;

  // Check if we have a push stream for this path.
  if (request.method == "GET") {
    stream = GetPushStream(path);
    if (stream) {
      DCHECK(streams_pushed_and_claimed_count_ < streams_pushed_count_);
      // Update the request time
      stream->SetRequestTime(base::Time::Now());
      // Change the request info, updating the response's request time too
      stream->SetRequestInfo(request);
      const HttpResponseInfo* response = stream->GetResponseInfo();
      if (response && response->headers->HasHeader("vary")) {
        // TODO(ahendrickson) -- What is the right thing to do if the server
        // pushes data with a vary field?
        void* iter = NULL;
        std::string value;
        response->headers->EnumerateHeader(&iter, "vary", &value);
        LOG(ERROR) << "SpdyStream: "
                   << "Received pushed stream ID " << stream->stream_id()
                   << "with vary field value '" << value << "'";
      }
      streams_pushed_and_claimed_count_++;
      return stream;
    }
  }

  // Check if we have a pending push stream for this url.
  PendingStreamMap::iterator it;
  it = pending_streams_.find(path);
  if (it != pending_streams_.end()) {
    // Server has advertised a stream, but not yet sent it.
    DCHECK(!it->second);
    // Server will assign a stream id when the push stream arrives.  Use 0 for
    // now.
    log.AddEvent(NetLog::TYPE_SPDY_STREAM_ADOPTED_PUSH_STREAM, NULL);
    SpdyStream* stream = new SpdyStream(this, 0, true, log);
    stream->SetRequestInfo(request);
    stream->set_path(path);
    it->second = stream;
    return it->second;
  }

  const spdy::SpdyStreamId stream_id = GetNewStreamId();

  // If we still don't have a stream, activate one now.
  stream = new SpdyStream(this, stream_id, false, log);
  stream->SetRequestInfo(request);
  stream->set_priority(request.priority);
  stream->set_path(path);
  ActivateStream(stream);

  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyPriorityCount",
      static_cast<int>(request.priority), 0, 10, 11);

  LOG(INFO) << "SpdyStream: Creating stream " << stream_id << " for " << url;

  // TODO(mbelshe): Optimize memory allocations
  DCHECK(request.priority >= SPDY_PRIORITY_HIGHEST &&
         request.priority <= SPDY_PRIORITY_LOWEST);

  // Convert from HttpRequestHeaders to Spdy Headers.
  spdy::SpdyHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(request, &headers);

  spdy::SpdyControlFlags flags = spdy::CONTROL_FLAG_NONE;
  if (!request.upload_data || !upload_data->size())
    flags = spdy::CONTROL_FLAG_FIN;

  // Create a SYN_STREAM packet and add to the output queue.
  scoped_ptr<spdy::SpdySynStreamControlFrame> syn_frame(
      spdy_framer_.CreateSynStream(stream_id, 0, request.priority, flags, false,
                                   &headers));
  QueueFrame(syn_frame.get(), request.priority, stream);

  static StatsCounter spdy_requests("spdy.requests");
  spdy_requests.Increment();

  LOG(INFO) << "FETCHING: " << request.url.spec();
  streams_initiated_count_++;

  LOG(INFO) << "SPDY SYN_STREAM HEADERS ----------------------------------";
  DumpSpdyHeaders(headers);

  return stream;
}

int SpdySession::WriteStreamData(spdy::SpdyStreamId stream_id,
                                 net::IOBuffer* data, int len) {
  LOG(INFO) << "Writing Stream Data for stream " << stream_id << " (" << len
            << " bytes)";
  const int kMss = 1430;  // This is somewhat arbitrary and not really fixed,
                          // but it will always work reasonably with ethernet.
  // Chop the world into 2-packet chunks.  This is somewhat arbitrary, but
  // is reasonably small and ensures that we elicit ACKs quickly from TCP
  // (because TCP tries to only ACK every other packet).
  const int kMaxSpdyFrameChunkSize = (2 * kMss) - spdy::SpdyFrame::size();

  // Find our stream
  DCHECK(IsStreamActive(stream_id));
  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);
  if (!stream)
    return ERR_INVALID_SPDY_STREAM;

  // TODO(mbelshe):  Setting of the FIN is assuming that the caller will pass
  //                 all data to write in a single chunk.  Is this always true?

  // Set the flags on the upload.
  spdy::SpdyDataFlags flags = spdy::DATA_FLAG_FIN;
  if (len > kMaxSpdyFrameChunkSize) {
    len = kMaxSpdyFrameChunkSize;
    flags = spdy::DATA_FLAG_NONE;
  }

  // TODO(mbelshe): reduce memory copies here.
  scoped_ptr<spdy::SpdyDataFrame> frame(
      spdy_framer_.CreateDataFrame(stream_id, data->data(), len, flags));
  QueueFrame(frame.get(), stream->priority(), stream);
  return ERR_IO_PENDING;
}

bool SpdySession::CancelStream(spdy::SpdyStreamId stream_id) {
  LOG(INFO) << "Cancelling stream " << stream_id;
  if (!IsStreamActive(stream_id))
    return false;

  // TODO(mbelshe): We should send a RST_STREAM control frame here
  //                so that the server can cancel a large send.

  // TODO(mbelshe): Write a method for tearing down a stream
  //                that cleans it out of the active list, the pending list,
  //                etc.
  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  DeactivateStream(stream_id);
  return true;
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

void SpdySession::OnTCPConnect(int result) {
  LOG(INFO) << "Spdy socket connected (result=" << result << ")";

  // We shouldn't be coming through this path if we didn't just open a fresh
  // socket (or have an error trying to do so).
  DCHECK(!connection_->socket() || !connection_->is_reused());

  if (result != net::OK) {
    DCHECK_LT(result, 0);
    CloseSessionOnError(static_cast<net::Error>(result));
    return;
  } else {
    UpdateConnectionTypeHistograms(CONNECTION_SPDY);
  }

  AdjustSocketBufferSizes(connection_->socket());

  if (use_ssl_) {
    // Add a SSL socket on top of our existing transport socket.
    ClientSocket* socket = connection_->release_socket();
    // TODO(mbelshe): Fix the hostname.  This is BROKEN without having
    //                a real hostname.
    socket = session_->socket_factory()->CreateSSLClientSocket(
        socket, "" /* request_->url.HostNoBrackets() */ , ssl_config_);
    connection_->set_socket(socket);
    is_secure_ = true;
    // TODO(willchan): Plumb NetLog into SPDY code.
    int status = connection_->socket()->Connect(&ssl_connect_callback_);
    if (status != ERR_IO_PENDING)
      OnSSLConnect(status);
  } else {
    DCHECK_EQ(state_, CONNECTING);
    state_ = CONNECTED;

    // Make sure we get any pending data sent.
    WriteSocketLater();
    // Start reading
    ReadSocket();
  }
}

void SpdySession::OnSSLConnect(int result) {
  // TODO(mbelshe): We need to replicate the functionality of
  //   HttpNetworkTransaction::DoSSLConnectComplete here, where it calls
  //   HandleCertificateError() and such.
  if (IsCertificateError(result))
    result = OK;   // TODO(mbelshe): pretend we're happy anyway.

  if (result == OK) {
    DCHECK_EQ(state_, CONNECTING);
    state_ = CONNECTED;

    // After we've connected, send any data to the server, and then issue
    // our read.
    WriteSocketLater();
    ReadSocket();
  } else {
    DCHECK_LT(result, 0);  // It should be an error, not a byte count.
    CloseSessionOnError(static_cast<net::Error>(result));
  }
}

void SpdySession::OnReadComplete(int bytes_read) {
  // Parse a frame.  For now this code requires that the frame fit into our
  // buffer (32KB).
  // TODO(mbelshe): support arbitrarily large frames!

  LOG(INFO) << "Spdy socket read: " << bytes_read << " bytes";

  read_pending_ = false;

  if (bytes_read <= 0) {
    // Session is tearing down.
    net::Error error = static_cast<net::Error>(bytes_read);
    if (bytes_read == 0) {
      LOG(INFO) << "Spdy socket closed by server[" <<
          host_port_pair().ToString() << "].";
      error = ERR_CONNECTION_CLOSED;
    }
    CloseSessionOnError(error);
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
  DCHECK_NE(result, 0);  // This shouldn't happen for write.

  write_pending_ = false;

  scoped_refptr<SpdyStream> stream = in_flight_write_.stream();

  LOG(INFO) << "Spdy write complete (result=" << result << ")"
            << (stream ? std::string(" for stream ") +
                IntToString(stream->stream_id()) : "");

  if (result >= 0) {
    // It should not be possible to have written more bytes than our
    // in_flight_write_.
    DCHECK_LE(result, in_flight_write_.buffer()->BytesRemaining());

    in_flight_write_.buffer()->DidConsume(result);

    // We only notify the stream when we've fully written the pending frame.
    if (!in_flight_write_.buffer()->BytesRemaining()) {
      scoped_refptr<SpdyStream> stream = in_flight_write_.stream();
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
    CloseSessionOnError(static_cast<net::Error>(result));
  }
}

void SpdySession::ReadSocket() {
  if (read_pending_)
    return;

  if (state_ == CLOSED) {
    NOTREACHED();
    return;
  }

  CHECK(connection_.get());
  CHECK(connection_->socket());
  int bytes_read = connection_->socket()->Read(read_buffer_.get(),
                                               kReadBufferSize,
                                               &read_callback_);
  switch (bytes_read) {
    case 0:
      // Socket is closed!
      CloseSessionOnError(ERR_CONNECTION_CLOSED);
      return;
    case net::ERR_IO_PENDING:
      // Waiting for data.  Nothing to do now.
      read_pending_ = true;
      return;
    default:
      // Data was read, process it.
      // Schedule the work through the message loop to avoid recursive
      // callbacks.
      read_pending_ = true;
      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &SpdySession::OnReadComplete, bytes_read));
      break;
  }
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
      if (spdy_framer_.IsCompressible(&uncompressed_frame)) {
        scoped_ptr<spdy::SpdyFrame> compressed_frame(
            spdy_framer_.CompressFrame(&uncompressed_frame));
        if (!compressed_frame.get()) {
          LOG(ERROR) << "SPDY Compression failure";
          CloseSessionOnError(net::ERR_SPDY_PROTOCOL_ERROR);
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

void SpdySession::CloseAllStreams(net::Error code) {
  LOG(INFO) << "Closing all SPDY Streams for " << host_port_pair().ToString();

  static StatsCounter abandoned_streams("spdy.abandoned_streams");
  static StatsCounter abandoned_push_streams("spdy.abandoned_push_streams");

  if (active_streams_.size()) {
    abandoned_streams.Add(active_streams_.size());

    // Create a copy of the list, since aborting streams can invalidate
    // our list.
    SpdyStream** list = new SpdyStream*[active_streams_.size()];
    ActiveStreamMap::const_iterator it;
    int index = 0;
    for (it = active_streams_.begin(); it != active_streams_.end(); ++it)
      list[index++] = it->second;

    // Issue the aborts.
    for (--index; index >= 0; index--) {
      LOG(ERROR) << "ABANDONED (stream_id=" << list[index]->stream_id()
                 << "): " << list[index]->path();
      list[index]->OnClose(code);
    }

    // Clear out anything pending.
    active_streams_.clear();

    delete[] list;
  }

  if (pushed_streams_.size()) {
    streams_abandoned_count_ += pushed_streams_.size();
    abandoned_push_streams.Add(pushed_streams_.size());
    pushed_streams_.clear();
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

void SpdySession::CloseSessionOnError(net::Error err) {
  // Closing all streams can have a side-effect of dropping the last reference
  // to |this|.  Hold a reference through this function.
  scoped_refptr<SpdySession> self(this);

  DCHECK_LT(err, OK);
  LOG(INFO) << "spdy::CloseSessionOnError(" << err << ") for " <<
      host_port_pair().ToString();

  // Don't close twice.  This can occur because we can have both
  // a read and a write outstanding, and each can complete with
  // an error.
  if (state_ != CLOSED) {
    state_ = CLOSED;
    error_ = err;
    CloseAllStreams(err);
    RemoveFromPool();
  }
}

void SpdySession::ActivateStream(SpdyStream* stream) {
  const spdy::SpdyStreamId id = stream->stream_id();
  DCHECK(!IsStreamActive(id));

  active_streams_[id] = stream;
}

void SpdySession::DeactivateStream(spdy::SpdyStreamId id) {
  DCHECK(IsStreamActive(id));

  // Verify it is not on the pushed_streams_ list.
  ActiveStreamList::iterator it;
  for (it = pushed_streams_.begin(); it != pushed_streams_.end(); ++it) {
    scoped_refptr<SpdyStream> curr = *it;
    if (id == curr->stream_id()) {
      pushed_streams_.erase(it);
      break;
    }
  }

  active_streams_.erase(id);
}

void SpdySession::RemoveFromPool() {
  if (in_session_pool_) {
    session_->spdy_session_pool()->Remove(this);
    in_session_pool_ = false;
  }
}

scoped_refptr<SpdyStream> SpdySession::GetPushStream(const std::string& path) {
  static StatsCounter used_push_streams("spdy.claimed_push_streams");

  LOG(INFO) << "Looking for push stream: " << path;

  scoped_refptr<SpdyStream> stream;

  // We just walk a linear list here.
  ActiveStreamList::iterator it;
  for (it = pushed_streams_.begin(); it != pushed_streams_.end(); ++it) {
    stream = *it;
    if (path == stream->path()) {
      CHECK(stream->pushed());
      pushed_streams_.erase(it);
      used_push_streams.Increment();
      LOG(INFO) << "Push Stream Claim for: " << path;
      return stream;
    }
  }

  return NULL;
}

void SpdySession::GetSSLInfo(SSLInfo* ssl_info) {
  if (is_secure_) {
    SSLClientSocket* ssl_socket =
        reinterpret_cast<SSLClientSocket*>(connection_->socket());
    ssl_socket->GetSSLInfo(ssl_info);
  }
}

void SpdySession::OnError(spdy::SpdyFramer* framer) {
  LOG(ERROR) << "SpdySession error: " << framer->error_code();
  CloseSessionOnError(net::ERR_SPDY_PROTOCOL_ERROR);
}

void SpdySession::OnStreamFrameData(spdy::SpdyStreamId stream_id,
                                    const char* data,
                                    size_t len) {
  LOG(INFO) << "Spdy data for stream " << stream_id << ", " << len << " bytes";
  bool valid_stream = IsStreamActive(stream_id);
  if (!valid_stream) {
    // NOTE:  it may just be that the stream was cancelled.
    LOG(WARNING) << "Received data frame for invalid stream " << stream_id;
    return;
  }

  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  bool success = stream->OnDataReceived(data, len);
  // |len| == 0 implies a closed stream.
  if (!success || !len)
    DeactivateStream(stream_id);
}

bool SpdySession::Respond(const spdy::SpdyHeaderBlock& headers,
                          const scoped_refptr<SpdyStream> stream) {
  // TODO(mbelshe): For now we convert from our nice hash map back
  // to a string of headers; this is because the HttpResponseInfo
  // is a bit rigid for its http (non-spdy) design.
  HttpResponseInfo response;
  // TODO(ahendrickson): This is recorded after the entire SYN_STREAM control
  // frame has been received and processed.  Move to framer?
  response.response_time = base::Time::Now();
  int rv = OK;

  if (SpdyHeadersToHttpResponse(headers, &response)) {
    GetSSLInfo(&response.ssl_info);
    response.request_time = stream->GetRequestTime();
    response.vary_data.Init(*stream->GetRequestInfo(), *response.headers);
    rv = stream->OnResponseReceived(response);
  } else {
    rv = ERR_INVALID_RESPONSE;
  }

  if (rv < 0) {
    DCHECK_NE(rv, ERR_IO_PENDING);
    const spdy::SpdyStreamId stream_id = stream->stream_id();
    stream->OnClose(rv);
    DeactivateStream(stream_id);
    return false;
  }
  return true;
}

void SpdySession::OnSyn(const spdy::SpdySynStreamControlFrame& frame,
                        const spdy::SpdyHeaderBlock& headers) {
  spdy::SpdyStreamId stream_id = frame.stream_id();

  LOG(INFO) << "Spdy SynStream for stream " << stream_id;

  // Server-initiated streams should have even sequence numbers.
  if ((stream_id & 0x1) != 0) {
    LOG(ERROR) << "Received invalid OnSyn stream id " << stream_id;
    return;
  }

  if (IsStreamActive(stream_id)) {
    LOG(ERROR) << "Received OnSyn for active stream " << stream_id;
    return;
  }

  streams_pushed_count_++;

  LOG(INFO) << "SpdySession: Syn received for stream: " << stream_id;

  LOG(INFO) << "SPDY SYN RESPONSE HEADERS -----------------------";
  DumpSpdyHeaders(headers);

  // TODO(mbelshe): DCHECK that this is a GET method?

  const std::string& path = ContainsKey(headers, "path") ?
      headers.find("path")->second : "";

  // Verify that the response had a URL for us.
  DCHECK(!path.empty());
  if (path.empty()) {
    LOG(WARNING) << "Pushed stream did not contain a path.";
    return;
  }

  scoped_refptr<SpdyStream> stream;

  // Check if we already have a delegate awaiting this stream.
  PendingStreamMap::iterator it;
  it = pending_streams_.find(path);
  if (it != pending_streams_.end()) {
    stream = it->second;
    pending_streams_.erase(it);
  }

  if (stream) {
    CHECK(stream->pushed());
    CHECK_EQ(0u, stream->stream_id());
    stream->set_stream_id(stream_id);
  } else {
    // TODO(mbelshe): can we figure out how to use a NetLog here?
    stream = new SpdyStream(this, stream_id, true, BoundNetLog());

    // A new HttpResponseInfo object needs to be generated so the call to
    // OnResponseReceived below has something to fill in.
    // When a SpdyNetworkTransaction is created for this resource, the
    // response_info is copied over and this version is destroyed.
    //
    // TODO(cbentzel): Minimize allocations and copies of HttpResponseInfo
    // object. Should it just be part of SpdyStream?
    HttpResponseInfo* response_info = new HttpResponseInfo();
    stream->set_response_info_pointer(response_info);
  }

  pushed_streams_.push_back(stream);

  // Activate a stream and parse the headers.
  ActivateStream(stream);

  stream->set_path(path);

  if (!Respond(headers, stream))
    return;

  LOG(INFO) << "Got pushed stream for " << stream->path();

  static StatsCounter push_requests("spdy.pushed_streams");
  push_requests.Increment();
}

void SpdySession::OnSynReply(const spdy::SpdySynReplyControlFrame& frame,
                             const spdy::SpdyHeaderBlock& headers) {
  spdy::SpdyStreamId stream_id = frame.stream_id();
  LOG(INFO) << "Spdy SynReply for stream " << stream_id;

  bool valid_stream = IsStreamActive(stream_id);
  if (!valid_stream) {
    // NOTE:  it may just be that the stream was cancelled.
    LOG(WARNING) << "Received SYN_REPLY for invalid stream " << stream_id;
    return;
  }

  LOG(INFO) << "SPDY SYN_REPLY RESPONSE HEADERS for stream: " << stream_id;
  DumpSpdyHeaders(headers);

  // We record content declared as being pushed so that we don't
  // request a duplicate stream which is already scheduled to be
  // sent to us.
  spdy::SpdyHeaderBlock::const_iterator it;
  it = headers.find("x-associated-content");
  if (it != headers.end()) {
    const std::string& content = it->second;
    std::string::size_type start = 0;
    std::string::size_type end = 0;
    do {
      end = content.find("||", start);
      if (end == std::string::npos)
        end = content.length();
      std::string url = content.substr(start, end - start);
      std::string::size_type pos = url.find("??");
      if (pos == std::string::npos)
        break;
      url = url.substr(pos + 2);
      GURL gurl(url);
      std::string path = gurl.PathForRequest();
      if (path.length())
        pending_streams_[path] = NULL;
      else
        LOG(INFO) << "Invalid X-Associated-Content path: " << url;
      start = end + 2;
    } while (start < content.length());
  }

  scoped_refptr<SpdyStream> stream = active_streams_[stream_id];
  CHECK_EQ(stream->stream_id(), stream_id);
  CHECK(!stream->cancelled());

  Respond(headers, stream);
}

void SpdySession::OnControl(const spdy::SpdyControlFrame* frame) {
  spdy::SpdyHeaderBlock headers;
  uint32 type = frame->type();
  if (type == spdy::SYN_STREAM || type == spdy::SYN_REPLY) {
    if (!spdy_framer_.ParseHeaderBlock(frame, &headers)) {
      LOG(WARNING) << "Could not parse Spdy Control Frame Header";
      // TODO(mbelshe):  Error the session?
      return;
    }
  }

  switch (type) {
    case spdy::GOAWAY:
      OnGoAway(*reinterpret_cast<const spdy::SpdyGoAwayControlFrame*>(frame));
      break;
    case spdy::SETTINGS:
      OnSettings(
          *reinterpret_cast<const spdy::SpdySettingsControlFrame*>(frame));
      break;
    case spdy::RST_STREAM:
      OnFin(*reinterpret_cast<const spdy::SpdyRstStreamControlFrame*>(frame));
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
    default:
      DCHECK(false);  // Error!
  }
}

void SpdySession::OnFin(const spdy::SpdyRstStreamControlFrame& frame) {
  spdy::SpdyStreamId stream_id = frame.stream_id();
  LOG(INFO) << "Spdy Fin for stream " << stream_id;

  bool valid_stream = IsStreamActive(stream_id);
  if (!valid_stream) {
    // NOTE:  it may just be that the stream was cancelled.
    LOG(WARNING) << "Received FIN for invalid stream" << stream_id;
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
    stream->OnClose(ERR_FAILED);
  }

  DeactivateStream(stream_id);
}

void SpdySession::OnGoAway(const spdy::SpdyGoAwayControlFrame& frame) {
  LOG(INFO) << "Spdy GOAWAY for session[" << this << "] for " <<
      host_port_pair().ToString();
  RemoveFromPool();

  // TODO(willchan): Cancel any streams that are past the GoAway frame's
  // |last_accepted_stream_id|.

  // Don't bother killing any streams that are still reading.  They'll either
  // complete successfully or get an ERR_CONNECTION_CLOSED when the socket is
  // closed.
}

void SpdySession::OnSettings(const spdy::SpdySettingsControlFrame& frame) {
  spdy::SpdySettings settings;
  if (spdy_framer_.ParseSettings(&frame, &settings)) {
    SpdySettingsStorage* settings_storage = session_->mutable_spdy_settings();
    settings_storage->Set(host_port_pair_, settings);
  }
}

void SpdySession::SendSettings() {
  const SpdySettingsStorage& settings_storage = session_->spdy_settings();
  const spdy::SpdySettings& settings = settings_storage.Get(host_port_pair_);
  if (settings.empty())
    return;

  // Create the SETTINGS frame and send it.
  scoped_ptr<spdy::SpdySettingsControlFrame> settings_frame(
      spdy_framer_.CreateSettings(settings));
  QueueFrame(settings_frame.get(), 0, NULL);
}

}  // namespace net
