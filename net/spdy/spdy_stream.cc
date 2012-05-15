// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"

namespace net {

NetLogSpdyStreamErrorParameter::NetLogSpdyStreamErrorParameter(
    SpdyStreamId stream_id,
    int status,
    const std::string& description)
    : stream_id_(stream_id),
      status_(status),
      description_(description) {
}

NetLogSpdyStreamErrorParameter::~NetLogSpdyStreamErrorParameter() {
}

Value* NetLogSpdyStreamErrorParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("stream_id", static_cast<int>(stream_id_));
  dict->SetInteger("status", status_);
  dict->SetString("description", description_);
  return dict;
}

namespace {

class NetLogSpdyStreamWindowUpdateParameter : public NetLog::EventParameters {
 public:
  NetLogSpdyStreamWindowUpdateParameter(SpdyStreamId stream_id,
                                        int32 delta,
                                        int32 window_size)
      : stream_id_(stream_id), delta_(delta), window_size_(window_size) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("stream_id", stream_id_);
    dict->SetInteger("delta", delta_);
    dict->SetInteger("window_size", window_size_);
    return dict;
  }

 private:
  virtual ~NetLogSpdyStreamWindowUpdateParameter() {}

  const SpdyStreamId stream_id_;
  const int32 delta_;
  const int32 window_size_;
  DISALLOW_COPY_AND_ASSIGN(NetLogSpdyStreamWindowUpdateParameter);
};

bool ContainsUpperAscii(const std::string& str) {
  for (std::string::const_iterator i(str.begin()); i != str.end(); ++i) {
    if (*i >= 'A' && *i <= 'Z') {
      return true;
    }
  }
  return false;
}

}  // namespace

SpdyStream::SpdyStream(SpdySession* session,
                       SpdyStreamId stream_id,
                       bool pushed,
                       const BoundNetLog& net_log)
    : continue_buffering_data_(true),
      stream_id_(stream_id),
      priority_(HIGHEST),
      slot_(0),
      stalled_by_flow_control_(false),
      send_window_size_(kSpdyStreamInitialWindowSize),
      recv_window_size_(kSpdyStreamInitialWindowSize),
      unacked_recv_window_bytes_(0),
      pushed_(pushed),
      response_received_(false),
      session_(session),
      delegate_(NULL),
      request_time_(base::Time::Now()),
      response_(new SpdyHeaderBlock),
      io_state_(STATE_NONE),
      response_status_(OK),
      cancelled_(false),
      has_upload_data_(false),
      net_log_(net_log),
      send_bytes_(0),
      recv_bytes_(0),
      domain_bound_cert_type_(CLIENT_CERT_INVALID_TYPE) {
}

SpdyStream::~SpdyStream() {
  UpdateHistograms();
}

void SpdyStream::SetDelegate(Delegate* delegate) {
  CHECK(delegate);
  delegate_ = delegate;

  if (pushed_) {
    CHECK(response_received());
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&SpdyStream::PushedStreamReplayData, this));
  } else {
    continue_buffering_data_ = false;
  }
}

void SpdyStream::PushedStreamReplayData() {
  if (cancelled_ || !delegate_)
    return;

  continue_buffering_data_ = false;

  int rv = delegate_->OnResponseReceived(*response_, response_time_, OK);
  if (rv == ERR_INCOMPLETE_SPDY_HEADERS) {
    // We don't have complete headers.  Assume we're waiting for another
    // HEADERS frame.  Since we don't have headers, we had better not have
    // any pending data frames.
    DCHECK_EQ(0U, pending_buffers_.size());
    return;
  }

  std::vector<scoped_refptr<IOBufferWithSize> > buffers;
  buffers.swap(pending_buffers_);
  for (size_t i = 0; i < buffers.size(); ++i) {
    // It is always possible that a callback to the delegate results in
    // the delegate no longer being available.
    if (!delegate_)
      break;
    if (buffers[i]) {
      delegate_->OnDataReceived(buffers[i]->data(), buffers[i]->size());
    } else {
      delegate_->OnDataReceived(NULL, 0);
      session_->CloseStream(stream_id_, net::OK);
      // Note: |this| may be deleted after calling CloseStream.
      DCHECK_EQ(buffers.size() - 1, i);
    }
  }
}

void SpdyStream::DetachDelegate() {
  if (delegate_)
    delegate_->set_chunk_callback(NULL);
  delegate_ = NULL;
  if (!closed())
    Cancel();
}

const linked_ptr<SpdyHeaderBlock>& SpdyStream::spdy_headers() const {
  return request_;
}

void SpdyStream::set_spdy_headers(
    const linked_ptr<SpdyHeaderBlock>& headers) {
  request_ = headers;
}

void SpdyStream::set_initial_recv_window_size(int32 window_size) {
  session_->set_initial_recv_window_size(window_size);
}

void SpdyStream::PossiblyResumeIfStalled() {
  if (send_window_size_ > 0 && stalled_by_flow_control_) {
    stalled_by_flow_control_ = false;
    io_state_ = STATE_SEND_BODY;
    DoLoop(OK);
  }
}

void SpdyStream::AdjustSendWindowSize(int32 delta_window_size) {
  send_window_size_ += delta_window_size;
  PossiblyResumeIfStalled();
}

void SpdyStream::IncreaseSendWindowSize(int32 delta_window_size) {
  DCHECK(session_->is_flow_control_enabled());
  DCHECK_GE(delta_window_size, 1);

  int32 new_window_size = send_window_size_ + delta_window_size;

  // We should ignore WINDOW_UPDATEs received before or after this state,
  // since before means we've not written SYN_STREAM yet (i.e. it's too
  // early) and after means we've written a DATA frame with FIN bit.
  if (io_state_ != STATE_SEND_BODY_COMPLETE)
    return;

  // it's valid for send_window_size_ to become negative (via an incoming
  // SETTINGS), in which case incoming WINDOW_UPDATEs will eventually make
  // it positive; however, if send_window_size_ is positive and incoming
  // WINDOW_UPDATE makes it negative, we have an overflow.
  if (send_window_size_ > 0 && new_window_size < 0) {
    std::string desc = base::StringPrintf(
        "Received WINDOW_UPDATE [delta: %d] for stream %d overflows "
        "send_window_size_ [current: %d]", delta_window_size, stream_id_,
        send_window_size_);
    session_->ResetStream(stream_id_, FLOW_CONTROL_ERROR, desc);
    return;
  }

  send_window_size_ = new_window_size;

  net_log_.AddEvent(
      NetLog::TYPE_SPDY_STREAM_UPDATE_SEND_WINDOW,
      make_scoped_refptr(new NetLogSpdyStreamWindowUpdateParameter(
          stream_id_, delta_window_size, send_window_size_)));
  PossiblyResumeIfStalled();
}

void SpdyStream::DecreaseSendWindowSize(int32 delta_window_size) {
  // we only call this method when sending a frame, therefore
  // |delta_window_size| should be within the valid frame size range.
  DCHECK(session_->is_flow_control_enabled());
  DCHECK_GE(delta_window_size, 1);
  DCHECK_LE(delta_window_size, kMaxSpdyFrameChunkSize);

  // |send_window_size_| should have been at least |delta_window_size| for
  // this call to happen.
  DCHECK_GE(send_window_size_, delta_window_size);

  send_window_size_ -= delta_window_size;

  net_log_.AddEvent(
      NetLog::TYPE_SPDY_STREAM_UPDATE_SEND_WINDOW,
      make_scoped_refptr(new NetLogSpdyStreamWindowUpdateParameter(
          stream_id_, -delta_window_size, send_window_size_)));
}

void SpdyStream::IncreaseRecvWindowSize(int32 delta_window_size) {
  DCHECK_GE(delta_window_size, 1);
  // By the time a read is isued, stream may become inactive.
  if (!session_->IsStreamActive(stream_id_))
    return;

  if (!session_->is_flow_control_enabled())
    return;

  int32 new_window_size = recv_window_size_ + delta_window_size;
  if (recv_window_size_ > 0)
    DCHECK(new_window_size > 0);

  recv_window_size_ = new_window_size;
  net_log_.AddEvent(
      NetLog::TYPE_SPDY_STREAM_UPDATE_RECV_WINDOW,
      make_scoped_refptr(new NetLogSpdyStreamWindowUpdateParameter(
          stream_id_, delta_window_size, recv_window_size_)));

  unacked_recv_window_bytes_ += delta_window_size;
  if (unacked_recv_window_bytes_ > session_->initial_recv_window_size() / 2) {
    session_->SendWindowUpdate(stream_id_, unacked_recv_window_bytes_);
    unacked_recv_window_bytes_ = 0;
  }
}

void SpdyStream::DecreaseRecvWindowSize(int32 delta_window_size) {
  DCHECK_GE(delta_window_size, 1);

  if (!session_->is_flow_control_enabled())
    return;

  recv_window_size_ -= delta_window_size;
  net_log_.AddEvent(
      NetLog::TYPE_SPDY_STREAM_UPDATE_RECV_WINDOW,
      make_scoped_refptr(new NetLogSpdyStreamWindowUpdateParameter(
          stream_id_, -delta_window_size, recv_window_size_)));

  // Since we never decrease the initial window size, we should never hit
  // a negative |recv_window_size_|, if we do, it's a client side bug, so we use
  // PROTOCOL_ERROR for lack of a better error code.
  if (recv_window_size_ < 0) {
    session_->ResetStream(stream_id_, PROTOCOL_ERROR,
                          "Negative recv window size");
    NOTREACHED();
  }
}

int SpdyStream::GetPeerAddress(AddressList* address) const {
  return session_->GetPeerAddress(address);
}

int SpdyStream::GetLocalAddress(IPEndPoint* address) const {
  return session_->GetLocalAddress(address);
}

bool SpdyStream::WasEverUsed() const {
  return session_->WasEverUsed();
}

base::Time SpdyStream::GetRequestTime() const {
  return request_time_;
}

void SpdyStream::SetRequestTime(base::Time t) {
  request_time_ = t;
}

int SpdyStream::OnResponseReceived(const SpdyHeaderBlock& response) {
  int rv = OK;

  metrics_.StartStream();

  DCHECK(response_->empty());
  *response_ = response;  // TODO(ukai): avoid copy.

  recv_first_byte_time_ = base::TimeTicks::Now();
  response_time_ = base::Time::Now();

  // If we receive a response before we are in STATE_WAITING_FOR_RESPONSE, then
  // the server has sent the SYN_REPLY too early.
  if (!pushed_ && io_state_ != STATE_WAITING_FOR_RESPONSE)
    return ERR_SPDY_PROTOCOL_ERROR;
  if (pushed_)
    CHECK(io_state_ == STATE_NONE);
  io_state_ = STATE_OPEN;

  // Append all the headers into the response header block.
  for (SpdyHeaderBlock::const_iterator it = response.begin();
       it != response.end(); ++it) {
    // Disallow uppercase headers.
    if (ContainsUpperAscii(it->first)) {
      session_->ResetStream(stream_id_, PROTOCOL_ERROR,
                            "Upper case characters in header: " + it->first);
      response_status_ = ERR_SPDY_PROTOCOL_ERROR;
      return ERR_SPDY_PROTOCOL_ERROR;
    }
  }

  if ((*response_).find("transfer-encoding") != (*response_).end()) {
    session_->ResetStream(stream_id_, PROTOCOL_ERROR,
                         "Received transfer-encoding header");
    return ERR_SPDY_PROTOCOL_ERROR;
  }

  if (delegate_)
    rv = delegate_->OnResponseReceived(*response_, response_time_, rv);
  // If delegate_ is not yet attached, we'll call OnResponseReceived after the
  // delegate gets attached to the stream.

  return rv;
}

int SpdyStream::OnHeaders(const SpdyHeaderBlock& headers) {
  DCHECK(!response_->empty());

  // Append all the headers into the response header block.
  for (SpdyHeaderBlock::const_iterator it = headers.begin();
      it != headers.end(); ++it) {
    // Disallow duplicate headers.  This is just to be conservative.
    if ((*response_).find(it->first) != (*response_).end()) {
      LogStreamError(ERR_SPDY_PROTOCOL_ERROR, "HEADERS duplicate header");
      response_status_ = ERR_SPDY_PROTOCOL_ERROR;
      return ERR_SPDY_PROTOCOL_ERROR;
    }

    // Disallow uppercase headers.
    if (ContainsUpperAscii(it->first)) {
      session_->ResetStream(stream_id_, PROTOCOL_ERROR,
                            "Upper case characters in header: " + it->first);
      response_status_ = ERR_SPDY_PROTOCOL_ERROR;
      return ERR_SPDY_PROTOCOL_ERROR;
    }

    (*response_)[it->first] = it->second;
  }

  if ((*response_).find("transfer-encoding") != (*response_).end()) {
    session_->ResetStream(stream_id_, PROTOCOL_ERROR,
                         "Received transfer-encoding header");
    return ERR_SPDY_PROTOCOL_ERROR;
  }

  int rv = OK;
  if (delegate_) {
    rv = delegate_->OnResponseReceived(*response_, response_time_, rv);
    // ERR_INCOMPLETE_SPDY_HEADERS means that we are waiting for more
    // headers before the response header block is complete.
    if (rv == ERR_INCOMPLETE_SPDY_HEADERS)
      rv = OK;
  }
  return rv;
}

void SpdyStream::OnDataReceived(const char* data, int length) {
  DCHECK_GE(length, 0);

  // If we don't have a response, then the SYN_REPLY did not come through.
  // We cannot pass data up to the caller unless the reply headers have been
  // received.
  if (!response_received()) {
    LogStreamError(ERR_SYN_REPLY_NOT_RECEIVED, "Didn't receive a response.");
    session_->CloseStream(stream_id_, ERR_SYN_REPLY_NOT_RECEIVED);
    return;
  }

  if (!delegate_ || continue_buffering_data_) {
    // It should be valid for this to happen in the server push case.
    // We'll return received data when delegate gets attached to the stream.
    if (length > 0) {
      IOBufferWithSize* buf = new IOBufferWithSize(length);
      memcpy(buf->data(), data, length);
      pending_buffers_.push_back(make_scoped_refptr(buf));
    } else {
      pending_buffers_.push_back(NULL);
      metrics_.StopStream();
      // Note: we leave the stream open in the session until the stream
      //       is claimed.
    }
    return;
  }

  CHECK(!closed());

  // A zero-length read means that the stream is being closed.
  if (!length) {
    metrics_.StopStream();
    session_->CloseStream(stream_id_, net::OK);
    // Note: |this| may be deleted after calling CloseStream.
    return;
  }

  DecreaseRecvWindowSize(length);

  // Track our bandwidth.
  metrics_.RecordBytes(length);
  recv_bytes_ += length;
  recv_last_byte_time_ = base::TimeTicks::Now();

  if (!delegate_) {
    // It should be valid for this to happen in the server push case.
    // We'll return received data when delegate gets attached to the stream.
    IOBufferWithSize* buf = new IOBufferWithSize(length);
    memcpy(buf->data(), data, length);
    pending_buffers_.push_back(make_scoped_refptr(buf));
    return;
  }

  delegate_->OnDataReceived(data, length);
}

// This function is only called when an entire frame is written.
void SpdyStream::OnWriteComplete(int bytes) {
  DCHECK_LE(0, bytes);
  send_bytes_ += bytes;
  if (cancelled() || closed())
    return;
  DoLoop(bytes);
}

void SpdyStream::OnChunkAvailable() {
  DCHECK(io_state_ == STATE_SEND_HEADERS || io_state_ == STATE_SEND_BODY ||
         io_state_ == STATE_SEND_BODY_COMPLETE);
  if (io_state_ == STATE_SEND_BODY)
    OnWriteComplete(0);
}

int SpdyStream::GetProtocolVersion() const {
  return session_->GetProtocolVersion();
}

void SpdyStream::LogStreamError(int status, const std::string& description) {
  net_log_.AddEvent(
      NetLog::TYPE_SPDY_STREAM_ERROR,
      make_scoped_refptr(
          new NetLogSpdyStreamErrorParameter(stream_id_, status, description)));
}

void SpdyStream::OnClose(int status) {
  io_state_ = STATE_DONE;
  response_status_ = status;
  Delegate* delegate = delegate_;
  delegate_ = NULL;
  if (delegate) {
    delegate->set_chunk_callback(NULL);
    delegate->OnClose(status);
  }
}

void SpdyStream::Cancel() {
  if (cancelled())
    return;

  cancelled_ = true;
  if (session_->IsStreamActive(stream_id_))
    session_->ResetStream(stream_id_, CANCEL, "");
}

void SpdyStream::Close() {
  session_->CloseStream(stream_id_, net::OK);
}

int SpdyStream::SendRequest(bool has_upload_data) {
  if (delegate_)
    delegate_->set_chunk_callback(this);

  // Pushed streams do not send any data, and should always be in STATE_OPEN or
  // STATE_DONE. However, we still want to return IO_PENDING to mimic non-push
  // behavior.
  has_upload_data_ = has_upload_data;
  if (pushed_) {
    send_time_ = base::TimeTicks::Now();
    DCHECK(!has_upload_data_);
    DCHECK(response_received());
    return ERR_IO_PENDING;
  }
  CHECK_EQ(STATE_NONE, io_state_);
  io_state_ = STATE_GET_DOMAIN_BOUND_CERT;
  return DoLoop(OK);
}

int SpdyStream::WriteStreamData(IOBuffer* data, int length,
                                SpdyDataFlags flags) {
  // Until the headers have been completely sent, we can not be sure
  // that our stream_id is correct.
  DCHECK_GT(io_state_, STATE_SEND_HEADERS_COMPLETE);
  return session_->WriteStreamData(stream_id_, data, length, flags);
}

bool SpdyStream::GetSSLInfo(SSLInfo* ssl_info,
                            bool* was_npn_negotiated,
                            NextProto* protocol_negotiated) {
  return session_->GetSSLInfo(
      ssl_info, was_npn_negotiated, protocol_negotiated);
}

bool SpdyStream::GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) {
  return session_->GetSSLCertRequestInfo(cert_request_info);
}

bool SpdyStream::HasUrl() const {
  if (pushed_)
    return response_received();
  return request_.get() != NULL;
}

GURL SpdyStream::GetUrl() const {
  DCHECK(HasUrl());

  const SpdyHeaderBlock& headers = (pushed_) ? *response_ : *request_;
  return GetUrlFromHeaderBlock(headers, GetProtocolVersion(), pushed_);
}

void SpdyStream::OnGetDomainBoundCertComplete(int result) {
  DCHECK_EQ(STATE_GET_DOMAIN_BOUND_CERT_COMPLETE, io_state_);
  DoLoop(result);
}

int SpdyStream::DoLoop(int result) {
  do {
    State state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      // State machine 1: Send headers and body.
      case STATE_GET_DOMAIN_BOUND_CERT:
        CHECK_EQ(OK, result);
        result = DoGetDomainBoundCert();
        break;
      case STATE_GET_DOMAIN_BOUND_CERT_COMPLETE:
        result = DoGetDomainBoundCertComplete(result);
        break;
      case STATE_SEND_DOMAIN_BOUND_CERT:
        CHECK_EQ(OK, result);
        result = DoSendDomainBoundCert();
        break;
      case STATE_SEND_DOMAIN_BOUND_CERT_COMPLETE:
        result = DoSendDomainBoundCertComplete(result);
        break;
      case STATE_SEND_HEADERS:
        CHECK_EQ(OK, result);
        result = DoSendHeaders();
        break;
      case STATE_SEND_HEADERS_COMPLETE:
        result = DoSendHeadersComplete(result);
        break;
      case STATE_SEND_BODY:
        CHECK_EQ(OK, result);
        result = DoSendBody();
        break;
      case STATE_SEND_BODY_COMPLETE:
        result = DoSendBodyComplete(result);
        break;
      // This is an intermediary waiting state. This state is reached when all
      // data has been sent, but no data has been received.
      case STATE_WAITING_FOR_RESPONSE:
        io_state_ = STATE_WAITING_FOR_RESPONSE;
        result = ERR_IO_PENDING;
        break;
      // State machine 2: connection is established.
      // In STATE_OPEN, OnResponseReceived has already been called.
      // OnDataReceived, OnClose and OnWriteCompelte can be called.
      // Only OnWriteComplete calls DoLoop(().
      //
      // For HTTP streams, no data is sent from the client while in the OPEN
      // state, so OnWriteComplete is never called here.  The HTTP body is
      // handled in the OnDataReceived callback, which does not call into
      // DoLoop.
      //
      // For WebSocket streams, which are bi-directional, we'll send and
      // receive data once the connection is established.  Received data is
      // handled in OnDataReceived.  Sent data is handled in OnWriteComplete,
      // which calls DoOpen().
      case STATE_OPEN:
        result = DoOpen(result);
        break;

      case STATE_DONE:
        DCHECK(result != ERR_IO_PENDING);
        break;
      default:
        NOTREACHED() << io_state_;
        break;
    }
  } while (result != ERR_IO_PENDING && io_state_ != STATE_NONE &&
           io_state_ != STATE_OPEN);

  return result;
}

int SpdyStream::DoGetDomainBoundCert() {
  CHECK(request_.get());
  if (!session_->NeedsCredentials()) {
    // Proceed directly to sending headers
    io_state_ = STATE_SEND_HEADERS;
    return OK;
  }

  slot_ = session_->credential_state()->FindCredentialSlot(GetUrl());
  if (slot_ != SpdyCredentialState::kNoEntry) {
    // Proceed directly to sending headers
    io_state_ = STATE_SEND_HEADERS;
    return OK;
  }

  io_state_ = STATE_GET_DOMAIN_BOUND_CERT_COMPLETE;
  ServerBoundCertService* sbc_service = session_->GetServerBoundCertService();
  DCHECK(sbc_service != NULL);
  std::vector<uint8> requested_cert_types;
  requested_cert_types.push_back(session_->GetDomainBoundCertType());
  int rv = sbc_service->GetDomainBoundCert(
      GetUrl().GetOrigin().spec(), requested_cert_types,
      &domain_bound_cert_type_, &domain_bound_private_key_, &domain_bound_cert_,
      base::Bind(&SpdyStream::OnGetDomainBoundCertComplete,
                 base::Unretained(this)),
      &domain_bound_cert_request_handle_);
  return rv;
}

int SpdyStream::DoGetDomainBoundCertComplete(int result) {
  if (result != OK)
    return result;

  io_state_ = STATE_SEND_DOMAIN_BOUND_CERT;
  slot_ =  session_->credential_state()->SetHasCredential(GetUrl());
  return OK;
}

int SpdyStream::DoSendDomainBoundCert() {
  io_state_ = STATE_SEND_DOMAIN_BOUND_CERT_COMPLETE;
  CHECK(request_.get());
  std::string origin = GetUrl().GetOrigin().spec();
  origin.erase(origin.length() - 1);  // trim trailing slash
  int rv =  session_->WriteCredentialFrame(
      origin, domain_bound_cert_type_, domain_bound_private_key_,
      domain_bound_cert_, priority_);
  if (rv != ERR_IO_PENDING)
    return rv;
  return OK;
}

int SpdyStream::DoSendDomainBoundCertComplete(int result) {
  if (result < 0)
    return result;

  io_state_ = STATE_SEND_HEADERS;
  return OK;
}

int SpdyStream::DoSendHeaders() {
  CHECK(!cancelled_);

  SpdyControlFlags flags = CONTROL_FLAG_NONE;
  if (!has_upload_data_)
    flags = CONTROL_FLAG_FIN;

  CHECK(request_.get());
  int result = session_->WriteSynStream(
      stream_id_, priority_, slot_, flags,
      request_);
  if (result != ERR_IO_PENDING)
    return result;

  send_time_ = base::TimeTicks::Now();
  io_state_ = STATE_SEND_HEADERS_COMPLETE;
  return ERR_IO_PENDING;
}

int SpdyStream::DoSendHeadersComplete(int result) {
  if (result < 0)
    return result;

  CHECK_GT(result, 0);

  if (!delegate_)
    return ERR_UNEXPECTED;

  // There is no body, skip that state.
  if (delegate_->OnSendHeadersComplete(result)) {
    io_state_ = STATE_WAITING_FOR_RESPONSE;
    return OK;
  }

  io_state_ = STATE_SEND_BODY;
  return OK;
}

// DoSendBody is called to send the optional body for the request.  This call
// will also be called as each write of a chunk of the body completes.
int SpdyStream::DoSendBody() {
  // If we're already in the STATE_SENDING_BODY state, then we've already
  // sent a portion of the body.  In that case, we need to first consume
  // the bytes written in the body stream.  Note that the bytes written is
  // the number of bytes in the frame that were written, only consume the
  // data portion, of course.
  io_state_ = STATE_SEND_BODY_COMPLETE;
  if (!delegate_)
    return ERR_UNEXPECTED;
  return delegate_->OnSendBody();
}

int SpdyStream::DoSendBodyComplete(int result) {
  if (result < 0)
    return result;

  if (!delegate_)
    return ERR_UNEXPECTED;

  bool eof = false;
  result = delegate_->OnSendBodyComplete(result, &eof);
  if (!eof)
    io_state_ = STATE_SEND_BODY;
  else
    io_state_ = STATE_WAITING_FOR_RESPONSE;

  return result;
}

int SpdyStream::DoOpen(int result) {
  if (delegate_)
    delegate_->OnDataSent(result);
  io_state_ = STATE_OPEN;
  return result;
}

void SpdyStream::UpdateHistograms() {
  // We need all timers to be filled in, otherwise metrics can be bogus.
  if (send_time_.is_null() || recv_first_byte_time_.is_null() ||
      recv_last_byte_time_.is_null())
    return;

  UMA_HISTOGRAM_TIMES("Net.SpdyStreamTimeToFirstByte",
      recv_first_byte_time_ - send_time_);
  UMA_HISTOGRAM_TIMES("Net.SpdyStreamDownloadTime",
      recv_last_byte_time_ - recv_first_byte_time_);
  UMA_HISTOGRAM_TIMES("Net.SpdyStreamTime",
      recv_last_byte_time_ - send_time_);

  UMA_HISTOGRAM_COUNTS("Net.SpdySendBytes", send_bytes_);
  UMA_HISTOGRAM_COUNTS("Net.SpdyRecvBytes", recv_bytes_);
}

}  // namespace net
