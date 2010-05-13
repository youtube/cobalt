// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_H_

#include <string>

#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/http/http_auth.h"

namespace net {

class BoundNetLog;
class HostResolver;
class ProxyInfo;
struct HttpRequestInfo;

// HttpAuthHandler is the interface for the authentication schemes
// (basic, digest, NTLM, Negotiate).
// HttpAuthHandler objects are typically created by an HttpAuthHandlerFactory.
class HttpAuthHandler : public base::RefCounted<HttpAuthHandler> {
 public:
  // Initializes the handler using a challenge issued by a server.
  // |challenge| must be non-NULL and have already tokenized the
  // authentication scheme, but none of the tokens occuring after the
  // authentication scheme. |target| and |origin| are both stored
  // for later use, and are not part of the initial challenge.
  bool InitFromChallenge(HttpAuth::ChallengeTokenizer* challenge,
                         HttpAuth::Target target,
                         const GURL& origin);

  // Lowercase name of the auth scheme
  const std::string& scheme() const {
    return scheme_;
  }

  // The realm value that was parsed during Init().
  const std::string& realm() const {
    return realm_;
  }

  // Numeric rank based on the challenge's security level. Higher
  // numbers are better. Used by HttpAuth::ChooseBestChallenge().
  int score() const {
    return score_;
  }

  HttpAuth::Target target() const {
    return target_;
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
  virtual bool NeedsIdentity() { return true; }

  // Returns true if this is the final round of the authentication sequence.
  // For Basic and Digest, the method always returns true because they are
  // single-round schemes.
  virtual bool IsFinalRound() { return true; }

  // Returns whether the default credentials may be used for the |origin| passed
  // into |InitFromChallenge|. If true, the user does not need to be prompted
  // for username and password to establish credentials.
  // NOTE: SSO is a potential security risk.
  // TODO(cbentzel): Add a pointer to Firefox documentation about risk.
  virtual bool AllowsDefaultCredentials() { return false; }

  // Returns whether the canonical DNS name for the origin host needs to be
  // resolved. The Negotiate auth scheme typically uses the canonical DNS
  // name when constructing the Kerberos SPN.
  virtual bool NeedsCanonicalName() { return false; }

  // TODO(cbentzel): Separate providing credentials from generating the
  // authentication token in the API.

  // Generates an authentication token.
  // The return value is an error code. If the code is not |OK|, the value of
  // |*auth_token| is unspecified.
  // |auth_token| is a return value and must be non-NULL.
  virtual int GenerateAuthToken(const std::wstring& username,
                                const std::wstring& password,
                                const HttpRequestInfo* request,
                                const ProxyInfo* proxy,
                                std::string* auth_token) = 0;

  // Generates an authentication token using default credentials.
  // The return value is an error code. If the code is not |OK|, the value of
  // |*auth_token| is unspecified.
  // |auth_token| is a return value and must be non-NULL.
  // This should only be called if |SupportsDefaultCredentials| returns true.
  virtual int GenerateDefaultAuthToken(const HttpRequestInfo* request,
                                       const ProxyInfo* proxy,
                                       std::string* auth_token) = 0;

  // Resolves the canonical name for the |origin_| host. The canonical
  // name is used by the Negotiate scheme to generate a valid Kerberos
  // SPN.
  // The return value is a net error code.
  virtual int ResolveCanonicalName(HostResolver* host_resolver,
                                   CompletionCallback* callback,
                                   const BoundNetLog& net_log);

 protected:
  enum Property {
    ENCRYPTS_IDENTITY = 1 << 0,
    IS_CONNECTION_BASED = 1 << 1,
  };

  friend class base::RefCounted<HttpAuthHandler>;

  virtual ~HttpAuthHandler() { }

  // Initializes the handler using a challenge issued by a server.
  // |challenge| must be non-NULL and have already tokenized the
  // authentication scheme, but none of the tokens occuring after the
  // authentication scheme.
  // Implementations are expcted to initialize the following members:
  // scheme_, realm_, score_, properties_
  virtual bool Init(HttpAuth::ChallengeTokenizer* challenge) = 0;

  // The lowercase auth-scheme {"basic", "digest", "ntlm", "negotiate"}
  std::string scheme_;

  // The realm.  Used by "basic" and "digest".
  std::string realm_;

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
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_H_
