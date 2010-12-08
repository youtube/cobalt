// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_BASIC_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_BASIC_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"

namespace net {

// Code for handling http basic authentication.
class HttpAuthHandlerBasic : public HttpAuthHandler {
 public:
  class Factory : public HttpAuthHandlerFactory {
   public:
    Factory();
    virtual ~Factory();

    virtual int CreateAuthHandler(HttpAuth::ChallengeTokenizer* challenge,
                                  HttpAuth::Target target,
                                  const GURL& origin,
                                  CreateReason reason,
                                  int digest_nonce_count,
                                  const BoundNetLog& net_log,
                                  scoped_ptr<HttpAuthHandler>* handler);
  };

  virtual HttpAuth::AuthorizationResult HandleAnotherChallenge(
      HttpAuth::ChallengeTokenizer* challenge);

 protected:
  virtual bool Init(HttpAuth::ChallengeTokenizer* challenge);

  virtual int GenerateAuthTokenImpl(const string16* username,
                                    const string16* password,
                                    const HttpRequestInfo* request,
                                    CompletionCallback* callback,
                                    std::string* auth_token);

 private:
  ~HttpAuthHandlerBasic() {}

  bool ParseChallenge(HttpAuth::ChallengeTokenizer* challenge);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_BASIC_H_
