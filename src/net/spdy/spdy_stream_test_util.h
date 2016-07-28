// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_STREAM_TEST_UTIL_H_
#define NET_SPDY_SPDY_STREAM_TEST_UTIL_H_

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/spdy/spdy_stream.h"

namespace net {

namespace test {

class TestSpdyStreamDelegate : public SpdyStream::Delegate {
 public:
  TestSpdyStreamDelegate(SpdyStream* stream,
                         SpdyHeaderBlock* headers,
                         IOBufferWithSize* buf,
                         const CompletionCallback& callback);
  virtual ~TestSpdyStreamDelegate();

  virtual bool OnSendHeadersComplete(int status) OVERRIDE;
  virtual int OnSendBody() OVERRIDE;
  virtual int OnSendBodyComplete(int status, bool* eof) OVERRIDE;
  virtual int OnResponseReceived(const SpdyHeaderBlock& response,
                                 base::Time response_time,
                                 int status) OVERRIDE;
  virtual void OnHeadersSent() OVERRIDE;
  virtual int OnDataReceived(const char* buffer, int bytes) OVERRIDE;
  virtual void OnDataSent(int length) OVERRIDE;
  virtual void OnClose(int status) OVERRIDE;

  bool send_headers_completed() const { return send_headers_completed_; }
  const linked_ptr<SpdyHeaderBlock>& response() const {
    return response_;
  }
  const std::string& received_data() const { return received_data_; }
  int headers_sent() const { return headers_sent_; }
  int data_sent() const { return data_sent_; }
  bool closed() const {  return closed_; }

 private:
  SpdyStream* stream_;
  scoped_ptr<SpdyHeaderBlock> headers_;
  scoped_refptr<IOBufferWithSize> buf_;
  CompletionCallback callback_;
  bool send_headers_completed_;
  linked_ptr<SpdyHeaderBlock> response_;
  std::string received_data_;
  int headers_sent_;
  int data_sent_;
  bool closed_;

};

} // namespace test

} // namespace net

#endif // NET_SPDY_SPDY_STREAM_TEST_UTIL_H_
