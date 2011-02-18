// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_H_
#define NET_HTTP_HTTP_AUTH_H_
#pragma once

#include <set>
#include <string>

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "net/http/http_util.h"

template <class T> class scoped_refptr;

namespace net {

class BoundNetLog;
class HttpAuthHandler;
class HttpAuthHandlerFactory;
class HttpResponseHeaders;

// Utility class for http authentication.
class HttpAuth {
 public:
  // Http authentication can be done the the proxy server, origin server,
  // or both. This enum tracks who the target is.
  enum Target {
    AUTH_NONE = -1,
    // We depend on the valid targets (!= AUTH_NONE) being usable as indexes
    // in an array, so start from 0.
    AUTH_PROXY = 0,
    AUTH_SERVER = 1,
    AUTH_NUM_TARGETS = 2,
  };

  // What the HTTP WWW-Authenticate/Proxy-Authenticate headers indicate about
  // the previous authorization attempt.
  enum AuthorizationResult {
    AUTHORIZATION_RESULT_ACCEPT,   // The authorization attempt was accepted,
                                   // although there still may be additional
                                   // rounds of challenges.

    AUTHORIZATION_RESULT_REJECT,   // The authorization attempt was rejected.

    AUTHORIZATION_RESULT_STALE,    // (Digest) The nonce used in the
                                   // authorization attempt is stale, but
                                   // otherwise the attempt was valid.

    AUTHORIZATION_RESULT_INVALID,  // The authentication challenge headers are
                                   // poorly formed (the authorization attempt
                                   // itself may have been fine).

    AUTHORIZATION_RESULT_DIFFERENT_REALM,  // The authorization
                                           // attempt was rejected,
                                           // but the realm associated
                                           // with the new challenge
                                           // is different from the
                                           // previous attempt.
  };

  // Describes where the identity used for authentication came from.
  enum IdentitySource {
    // Came from nowhere -- the identity is not initialized.
    IDENT_SRC_NONE,

    // The identity came from the auth cache, by doing a path-based
    // lookup (premptive authorization).
    IDENT_SRC_PATH_LOOKUP,

    // The identity was extracted from a URL of the form:
    // http://<username>:<password>@host:port
    IDENT_SRC_URL,

    // The identity was retrieved from the auth cache, by doing a
    // realm lookup.
    IDENT_SRC_REALM_LOOKUP,

    // The identity was provided by RestartWithAuth -- it likely
    // came from a prompt (or maybe the password manager).
    IDENT_SRC_EXTERNAL,

    // The identity used the default credentials for the computer,
    // on schemes that support single sign-on.
    IDENT_SRC_DEFAULT_CREDENTIALS,
  };

  enum Scheme {
    AUTH_SCHEME_BASIC = 0,
    AUTH_SCHEME_DIGEST,
    AUTH_SCHEME_NTLM,
    AUTH_SCHEME_NEGOTIATE,
    AUTH_SCHEME_MOCK,
    AUTH_SCHEME_MAX,
  };

  // Helper structure used by HttpNetworkTransaction to track
  // the current identity being used for authorization.
  struct Identity {
    Identity();

    IdentitySource source;
    bool invalid;
    string16 username;
    string16 password;
  };

  // Get the name of the header containing the auth challenge
  // (either WWW-Authenticate or Proxy-Authenticate).
  static std::string GetChallengeHeaderName(Target target);

  // Get the name of the header where the credentials go
  // (either Authorization or Proxy-Authorization).
  static std::string GetAuthorizationHeaderName(Target target);

  // Returns a string representation of a Target value that can be used in log
  // messages.
  static std::string GetAuthTargetString(Target target);

  // Returns a string representation of an authentication Scheme.
  static const char* SchemeToString(Scheme scheme);

  // Iterate through the challenge headers, and pick the best one that
  // we support. Obtains the implementation class for handling the challenge,
  // and passes it back in |*handler|. If no supported challenge was found,
  // |*handler| is set to NULL.
  //
  // |disabled_schemes| is the set of schemes that we should not use.
  //
  // |origin| is used by the NTLM and Negotiation authentication scheme to
  // construct the service principal name.  It is ignored by other schemes.
  static void ChooseBestChallenge(
      HttpAuthHandlerFactory* http_auth_handler_factory,
      const HttpResponseHeaders* headers,
      Target target,
      const GURL& origin,
      const std::set<Scheme>& disabled_schemes,
      const BoundNetLog& net_log,
      scoped_ptr<HttpAuthHandler>* handler);

  // Handle a 401/407 response from a server/proxy after a previous
  // authentication attempt. For connection-based authentication schemes, the
  // new response may be another round in a multi-round authentication sequence.
  // For request-based schemes, a 401/407 response is typically treated like a
  // rejection of the previous challenge, except in the Digest case when a
  // "stale" attribute is present.
  //
  // |handler| must be non-NULL, and is the HttpAuthHandler from the previous
  // authentication round.
  //
  // |headers| must be non-NULL and contain the new HTTP response.
  //
  // |target| specifies whether the authentication challenge response came
  // from a server or a proxy.
  //
  // |disabled_schemes| are the authentication schemes to ignore.
  //
  // |challenge_used| is the text of the authentication challenge used in
  // support of the returned AuthorizationResult. If no headers were used for
  // the result (for example, all headers have unknown authentication schemes),
  // the value is cleared.
  static AuthorizationResult HandleChallengeResponse(
      HttpAuthHandler* handler,
      const HttpResponseHeaders* headers,
      Target target,
      const std::set<Scheme>& disabled_schemes,
      std::string* challenge_used);

  // Breaks up a challenge string into the the auth scheme and parameter list,
  // according to RFC 2617 Sec 1.2:
  //    challenge = auth-scheme 1*SP 1#auth-param
  //
  // Depending on the challenge scheme, it may be appropriate to interpret the
  // parameters as either a base-64 encoded string or a comma-delimited list
  // of name-value pairs. param_pairs() and base64_param() methods are provided
  // to support either usage.
  class ChallengeTokenizer {
   public:
    ChallengeTokenizer(std::string::const_iterator begin,
                       std::string::const_iterator end)
        : begin_(begin),
          end_(end),
          scheme_begin_(begin),
          scheme_end_(begin),
          params_begin_(end),
          params_end_(end) {
      Init(begin, end);
    }

    // Get the original text.
    std::string challenge_text() const {
      return std::string(begin_, end_);
    }

    // Get the auth scheme of the challenge.
    std::string::const_iterator scheme_begin() const { return scheme_begin_; }
    std::string::const_iterator scheme_end() const { return scheme_end_; }
    std::string scheme() const {
      return std::string(scheme_begin_, scheme_end_);
    }

    HttpUtil::NameValuePairsIterator param_pairs() const;
    std::string base64_param() const;

   private:
    void Init(std::string::const_iterator begin,
              std::string::const_iterator end);

    std::string::const_iterator begin_;
    std::string::const_iterator end_;

    std::string::const_iterator scheme_begin_;
    std::string::const_iterator scheme_end_;

    std::string::const_iterator params_begin_;
    std::string::const_iterator params_end_;
  };
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_H_
