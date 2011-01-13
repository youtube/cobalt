// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/http/http_auth.h"

namespace net {

struct HttpRequestInfo;

// HttpAuthHandler is the interface for the authentication schemes
// (basic, digest, NTLM, Negotiate).
// HttpAuthHandler objects are typically created by an HttpAuthHandlerFactory.
class HttpAuthHandler {
 public:
  HttpAuthHandler();
  virtual ~HttpAuthHandler();

  // Initializes the handler using a challenge issued by a server.
  // |challenge| must be non-NULL and have already tokenized the
  // authentication scheme, but none of the tokens occuring after the
  // authentication scheme. |target| and |origin| are both stored
  // for later use, and are not part of the initial challenge.
  bool InitFromChallenge(HttpAuth::ChallengeTokenizer* challenge,
                         HttpAuth::Target target,
                         const GURL& origin,
                         const BoundNetLog& net_log);

  // Determines how the previous authorization attempt was received.
  //
  // This is called when the server/proxy responds with a 401/407 after an
  // earlier authorization attempt. Although this normally means that the
  // previous attempt was rejected, in multi-round schemes such as
  // NTLM+Negotiate it may indicate that another round of challenge+response
  // is required. For Digest authentication it may also mean that the previous
  // attempt used a stale nonce (and nonce-count) and that a new attempt should
  // be made with a different nonce provided in the challenge.
  //
  // |challenge| must be non-NULL and have already tokenized the
  // authentication scheme, but none of the tokens occuring after the
  // authentication scheme.
  virtual HttpAuth::AuthorizationResult HandleAnotherChallenge(
      HttpAuth::ChallengeTokenizer* challenge) = 0;

  // Generates an authentication token, potentially asynchronously.
  //
  // When |username| and |password| are NULL, the default credentials for
  // the currently logged in user are used. |AllowsDefaultCredentials()| MUST be
  // true in this case.
  //
  // |request|, |callback|, and |auth_token| must be non-NULL.
  //
  // The return value is a net error code.
  // If |OK| is returned, |*auth_token| is filled in with an authentication
  // token which can be inserted in the HTTP request.
  // If |ERR_IO_PENDING| is returned, |*auth_token| will be filled in
  // asynchronously and |callback| will be invoked. The lifetime of
  // |request|, |callback|, and |auth_token| must last until |callback| is
  // invoked, but |username| and |password| are only used during the initial
  // call.
  // Otherwise, there was a problem generating a token synchronously, and the
  // value of |*auth_token| is unspecified.
  int GenerateAuthToken(const string16* username,
                        const string16* password,
                        const HttpRequestInfo* request,
                        CompletionCallback* callback,
                        std::string* auth_token);

  // The authentication scheme as an enumerated value.
  HttpAuth::Scheme auth_scheme() const {
    return auth_scheme_;
  }

  // The realm value that was parsed during Init().
  const std::string& realm() const {
    return realm_;
  }

  // The challenge which was issued when creating the handler.
  const std::string challenge() const {
    return auth_challenge_;
  }

  // Numeric rank based on the challenge's security level. Higher
  // numbers are better. Used by HttpAuth::ChooseBestChallenge().
  int score() const {
    return score_;
  }

  HttpAuth::Target target() const {
    return target_;
  }

  // Returns the proxy or server which issued the authentication challenge
  // that this HttpAuthHandler is handling. The URL includes scheme, host, and
  // port, but does not include path.
  const GURL& origin() const {
    return origin_;
  }

  // Returns true if the authentication scheme does not send the username and
  // password in the clear.
  bool encrypts_identity() const {
    return (properties_ & ENCRYPTS_IDENTITY) != 0;
  }

  // Returns true if the authentication scheme is connection-based, for
  // example, NTLM.  A connection-based authentication scheme does not support
  // preemptive authentication, and must use the same handler object
  // throughout the life of an HTTP transaction.
  bool is_connection_based() const {
    return (properties_ & IS_CONNECTION_BASED) != 0;
  }

  // Returns true if the response to the current authentication challenge
  // requires an identity.
  // TODO(wtc): Find a better way to handle a multi-round challenge-response
  // sequence used by a connection-based authentication scheme.
  virtual bool NeedsIdentity();

  // Returns whether the default credentials may be used for the |origin| passed
  // into |InitFromChallenge|. If true, the user does not need to be prompted
  // for username and password to establish credentials.
  // NOTE: SSO is a potential security risk.
  // TODO(cbentzel): Add a pointer to Firefox documentation about risk.
  virtual bool AllowsDefaultCredentials();

 protected:
  enum Property {
    ENCRYPTS_IDENTITY = 1 << 0,
    IS_CONNECTION_BASED = 1 << 1,
  };

  // Initializes the handler using a challenge issued by a server.
  // |challenge| must be non-NULL and have already tokenized the
  // authentication scheme, but none of the tokens occuring after the
  // authentication scheme.
  // Implementations are expcted to initialize the following members:
  // scheme_, realm_, score_, properties_
  virtual bool Init(HttpAuth::ChallengeTokenizer* challenge) = 0;

  // |GenerateAuthTokenImpl()} is the auth-scheme specific implementation
  // of generating the next auth token. Callers sohuld use |GenerateAuthToken()|
  // which will in turn call |GenerateAuthTokenImpl()|
  virtual int GenerateAuthTokenImpl(const string16* username,
                                    const string16* password,
                                    const HttpRequestInfo* request,
                                    CompletionCallback* callback,
                                    std::string* auth_token) = 0;

  // The auth-scheme as an enumerated value.
  HttpAuth::Scheme auth_scheme_;

  // The realm.  Used by "basic" and "digest".
  std::string realm_;

  // The auth challenge.
  std::string auth_challenge_;

  // The {scheme, host, port} for the authentication target.  Used by "ntlm"
  // and "negotiate" to construct the service principal name.
  GURL origin_;

  // The score for this challenge. Higher numbers are better.
  int score_;

  // Whether this authentication request is for a proxy server, or an
  // origin server.
  HttpAuth::Target target_;

  // A bitmask of the properties of the authentication scheme.
  int properties_;

  BoundNetLog net_log_;

 private:
  void OnGenerateAuthTokenComplete(int rv);
  void FinishGenerateAuthToken();

  CompletionCallback* original_callback_;
  CompletionCallbackImpl<HttpAuthHandler> wrapper_callback_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_H_
