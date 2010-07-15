// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_MOCK_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_MOCK_H_

#include <string>

#include "base/task.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"

namespace net {

// MockAuthHandler is used in tests to reliably trigger edge cases.
class HttpAuthHandlerMock : public HttpAuthHandler {
 public:
  enum Resolve {
    RESOLVE_INIT,
    RESOLVE_SKIP,
    RESOLVE_SYNC,
    RESOLVE_ASYNC,
    RESOLVE_TESTED,
  };

  HttpAuthHandlerMock();

  virtual ~HttpAuthHandlerMock();

  void SetResolveExpectation(Resolve resolve);

  virtual bool NeedsCanonicalName();

  virtual int ResolveCanonicalName(HostResolver* host_resolver,
                                   CompletionCallback* callback);

  virtual bool NeedsIdentity() { return first_round_; }
  virtual bool IsFinalRound() { return false; }

  void SetGenerateExpectation(bool async, int rv);

  void set_connection_based(bool connection_based) {
    connection_based_ = connection_based;
  }

  // The Factory class simply returns the same handler each time
  // CreateAuthHandler is called.
  class Factory : public HttpAuthHandlerFactory {
   public:
    Factory() {}
    virtual ~Factory() {}

    void set_mock_handler(HttpAuthHandler* handler, HttpAuth::Target target);

    virtual int CreateAuthHandler(HttpAuth::ChallengeTokenizer* challenge,
                                  HttpAuth::Target target,
                                  const GURL& origin,
                                  CreateReason reason,
                                  int nonce_count,
                                  const BoundNetLog& net_log,
                                  scoped_ptr<HttpAuthHandler>* handler);

   private:
    scoped_ptr<HttpAuthHandler> handlers_[HttpAuth::AUTH_NUM_TARGETS];
  };

 protected:
  virtual bool Init(HttpAuth::ChallengeTokenizer* challenge);

  virtual int GenerateAuthTokenImpl(const std::wstring* username,
                                    const std::wstring* password,
                                    const HttpRequestInfo* request,
                                    CompletionCallback* callback,
                                    std::string* auth_token);

 private:
  void OnResolveCanonicalName();

  void OnGenerateAuthToken();

  Resolve resolve_;
  CompletionCallback* user_callback_;
  ScopedRunnableMethodFactory<HttpAuthHandlerMock> method_factory_;
  bool generate_async_;
  int generate_rv_;
  std::string* auth_token_;
  bool first_round_;
  bool connection_based_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_MOCK_H_
