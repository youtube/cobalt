// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_body_drainer.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream.h"

namespace net {

HttpResponseBodyDrainer::HttpResponseBodyDrainer(HttpStream* stream)
    : stream_(stream),
      next_state_(STATE_NONE),
      total_read_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &HttpResponseBodyDrainer::OnIOComplete)),
      user_callback_(NULL),
      session_(NULL) {}

HttpResponseBodyDrainer::~HttpResponseBodyDrainer() {}

void HttpResponseBodyDrainer::Start(HttpNetworkSession* session) {
  StartWithSize(session, kDrainBodyBufferSize);
}

void HttpResponseBodyDrainer::StartWithSize(HttpNetworkSession* session,
                                            int num_bytes_to_drain) {
  DCHECK_LE(0, num_bytes_to_drain);
  // TODO(simonjam): Consider raising this limit if we're pipelining. If we have
  // a bunch of responses in the pipeline, we should be less willing to give up
  // while draining.
  if (num_bytes_to_drain > kDrainBodyBufferSize) {
    Finish(ERR_RESPONSE_BODY_TOO_BIG_TO_DRAIN);
    return;
  } else if (num_bytes_to_drain == 0) {
    Finish(OK);
    return;
  }

  read_size_ = num_bytes_to_drain;
  read_buf_ = new IOBuffer(read_size_);
  next_state_ = STATE_DRAIN_RESPONSE_BODY;
  int rv = DoLoop(OK);

  if (rv == ERR_IO_PENDING) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(kTimeoutInSeconds),
                 this,
                 &HttpResponseBodyDrainer::OnTimerFired);
    session_ = session;
    session->AddResponseDrainer(this);
    return;
  }

  Finish(rv);
}

int HttpResponseBodyDrainer::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_DRAIN_RESPONSE_BODY:
        DCHECK_EQ(OK, rv);
        rv = DoDrainResponseBody();
        break;
      case STATE_DRAIN_RESPONSE_BODY_COMPLETE:
        rv = DoDrainResponseBodyComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int HttpResponseBodyDrainer::DoDrainResponseBody() {
  next_state_ = STATE_DRAIN_RESPONSE_BODY_COMPLETE;

  return stream_->ReadResponseBody(
      read_buf_, read_size_ - total_read_,
      &io_callback_);
}

int HttpResponseBodyDrainer::DoDrainResponseBodyComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  if (result < 0)
    return result;

  if (result == 0)
    return ERR_CONNECTION_CLOSED;

  total_read_ += result;
  if (stream_->IsResponseBodyComplete())
    return OK;

  DCHECK_LE(total_read_, kDrainBodyBufferSize);
  if (total_read_ >= kDrainBodyBufferSize)
    return ERR_RESPONSE_BODY_TOO_BIG_TO_DRAIN;

  next_state_ = STATE_DRAIN_RESPONSE_BODY;
  return OK;
}

void HttpResponseBodyDrainer::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    timer_.Stop();
    Finish(rv);
  }
}

void HttpResponseBodyDrainer::OnTimerFired() {
  Finish(ERR_TIMED_OUT);
}

void HttpResponseBodyDrainer::Finish(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  if (session_)
    session_->RemoveResponseDrainer(this);

  if (result < 0) {
    stream_->Close(true /* no keep-alive */);
  } else {
    DCHECK_EQ(OK, result);
    stream_->Close(false /* keep-alive */);
  }

  delete this;
}

}  // namespace net
