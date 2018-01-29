// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_SHELL_HTTP_TRANSACTION_SHELL_UNITTEST_H_
#define NET_HTTP_SHELL_HTTP_TRANSACTION_SHELL_UNITTEST_H_

#include <algorithm>
#include <string>
#include <list>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/test/test_file_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/capturing_net_log.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/http/shell/http_transaction_factory_shell.h"
#include "net/http/shell/http_transaction_shell.h"
#include "net/http/shell/http_stream_shell_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

class MockStreamShellLoader : public HttpStreamShellLoader {
 public:
  // HttpStreamShellLoader methods:
  MockStreamShellLoader()
      : response_(NULL), current_pos_(0), callback_(),
        reading_headers_(false) {}

  virtual int Open(const HttpRequestInfo* info,
                   const BoundNetLog& net_log) override {
    net_log_ = net_log;
    return OK;
  }

  virtual int SendRequest(const std::string& request_line,
                          const HttpRequestHeaders& headers,
                          HttpResponseInfo* response,
                          const CompletionCallback& callback) override {
    DCHECK(callback_.is_null());
    response_ = response;
    response_->socket_address = HostPortPair::FromString(request_line);
    callback_ = callback;

    net_log_.AddEvent(
        NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST_HEADERS,
        base::Bind(&HttpRequestHeaders::NetLogCallback,
                   base::Unretained(&headers),
                   &request_line));

    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&MockStreamShellLoader::IOComplete, this, OK));
    return ERR_IO_PENDING;
  }

  virtual int ReadResponseHeaders(const CompletionCallback& callback) override {
    DCHECK(callback_.is_null());
    // Set headers
    callback_ = callback;

    net_log_.BeginEvent(NetLog::TYPE_HTTP_STREAM_PARSER_READ_HEADERS);
    reading_headers_ = true;

    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&MockStreamShellLoader::IOComplete, this, OK));
    return ERR_IO_PENDING;
  }

  virtual int ReadResponseBody(IOBuffer* buf, int buf_len,
                               const CompletionCallback& callback) override {
    DCHECK(callback_.is_null());
    // Copy data
    int to_read = std::min(buf_len,
                           static_cast<int>(data_.size()) - current_pos_);
    memcpy(buf->data(), data_.c_str() + current_pos_, to_read);
    current_pos_ += to_read;
    callback_ = callback;
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&MockStreamShellLoader::IOComplete, this, to_read));
    return ERR_IO_PENDING;
  }

  virtual void SetProxy(const ProxyInfo* info) override {}

  virtual bool IsResponseBodyComplete() const override {
    return current_pos_ == data_.size();
  }

  virtual void Close(bool not_reusable) override {
  }

  void IOComplete(int result) {
    if (reading_headers_) {
      response_->headers = new HttpResponseHeaders(
          HttpUtil::AssembleRawHeaders(headers_.c_str(),
                                       headers_.size()));
      net_log_.EndEventWithNetErrorCode(
          NetLog::TYPE_HTTP_STREAM_PARSER_READ_HEADERS, OK);
    }

    DCHECK(!callback_.is_null());
    CompletionCallback c = callback_;
    callback_.Reset();
    c.Run(result);
  }

  void SetReturnData(const std::string& headers, const std::string& data) {
    headers_ = headers;
    data_ = data;
    current_pos_ = 0;
  }

 private:
  // data provider
  HttpResponseInfo* response_;
  // Fake data
  std::string headers_;
  std::string data_;
  int current_pos_;
  bool reading_headers_;
  // Caller's callback functions
  CompletionCallback callback_;
  // Net log
  BoundNetLog net_log_;
};

// Mock transaction class. Just to add accessor for internal stream loader
class MockTransactionShell : public HttpTransactionShell {
 public:
  explicit MockTransactionShell(const net::HttpNetworkSession::Params* params,
      HttpStreamShell* stream) : HttpTransactionShell(params, stream) {}

  MockStreamShellLoader* GetStreamLoader() {
    if (stream_)
      return static_cast<MockStreamShellLoader*>(stream_->GetStreamLoader());
    return NULL;
  }
};

class MockTransactionFactoryShell : public HttpTransactionFactoryShell {
 public:
  MockTransactionFactoryShell(const net::HttpNetworkSession::Params& params)
      : HttpTransactionFactoryShell(params) {}

  // HttpTransactionFactory methods:
  virtual int CreateTransaction(scoped_ptr<HttpTransaction>* trans,
                                HttpTransactionDelegate* delegate) override {
    HttpStreamShell* stream = new HttpStreamShell(new MockStreamShellLoader());
    trans->reset(new MockTransactionShell(&params_, stream));
    return OK;
  }
};

}  // namespace net
#endif  // NET_HTTP_SHELL_HTTP_TRANSACTION_SHELL_UNITTEST_H_
