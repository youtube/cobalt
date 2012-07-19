// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream_test_util.h"

#include "net/base/completion_callback.h"
#include "net/spdy/spdy_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace test {

TestSpdyStreamDelegate::TestSpdyStreamDelegate(
    SpdyStream* stream,
    IOBufferWithSize* buf,
    const CompletionCallback& callback)
    : stream_(stream),
      buf_(buf),
      callback_(callback),
      send_headers_completed_(false),
      response_(new SpdyHeaderBlock),
      data_sent_(0),
      closed_(false) {
}

TestSpdyStreamDelegate::~TestSpdyStreamDelegate() {
}

bool TestSpdyStreamDelegate::OnSendHeadersComplete(int status) {
  send_headers_completed_ = true;
  return true;
}

int TestSpdyStreamDelegate::OnSendBody() {
  ADD_FAILURE() << "OnSendBody should not be called";
  return ERR_UNEXPECTED;
}
int TestSpdyStreamDelegate::OnSendBodyComplete(int /*status*/, bool* /*eof*/) {
  ADD_FAILURE() << "OnSendBodyComplete should not be called";
  return ERR_UNEXPECTED;
}

int TestSpdyStreamDelegate::OnResponseReceived(const SpdyHeaderBlock& response,
                                               base::Time response_time,
                                               int status) {
  EXPECT_TRUE(send_headers_completed_);
  *response_ = response;
  if (buf_) {
    EXPECT_EQ(ERR_IO_PENDING,
              stream_->WriteStreamData(buf_.get(), buf_->size(),
                                       DATA_FLAG_NONE));
  }
  return status;
}

void TestSpdyStreamDelegate::OnDataReceived(const char* buffer, int bytes) {
  received_data_ += std::string(buffer, bytes);
}

void TestSpdyStreamDelegate::OnDataSent(int length) {
  data_sent_ += length;
}

void TestSpdyStreamDelegate::OnClose(int status) {
  closed_ = true;
  CompletionCallback callback = callback_;
  callback_.Reset();
  callback.Run(OK);
}

} // namespace test

} // namespace net
