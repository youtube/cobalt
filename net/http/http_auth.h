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

  // Helper structure used by HttpNetworkTransaction to track
  // the current identity being used for authorization.
  struct Identity {
    Identity() : source(IDENT_SRC_NONE), invalid(true) { }

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
      const std::set<std::string>& disabled_schemes,
      const BoundNetLog& net_log,
      scoped_ptr<HttpAuthHandler>* handler);

  // Handle a response to a previous authentication attempt.
  static AuthorizationResult HandleChallengeResponse(
      HttpAuthHandler* handler,
      const HttpResponseHeaders* headers,
      Target target,
      const std::set<std::string>& disabled_schemes);

  // ChallengeTokenizer breaks up a challenge string into the the auth scheme
  // and parameter list, according to RFC 2617 Sec 1.2:
  //    challenge = auth-scheme 1*SP 1#auth-param
  //
  // Check valid() after each iteration step in case it was malformed.
  // Also note that value() will give whatever is to the right of the equals
  // sign, quotemarks and all. Use unquoted_value() to get the logical value.
  class ChallengeTokenizer {
   public:
    ChallengeTokenizer(std::string::const_iterator begin,
                       std::string::const_iterator end)
        : props_(begin, end, ','),
          valid_(true),
          begin_(begin),
          end_(end),
          value_is_quoted_(false),
          expect_base64_token_(false) {
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

    // Returns false if there was a parse error.
    bool valid() const {
      return valid_;
    }

    // Advances the iterator to the next name-value pair, if any.
    // Returns true if there is none to consume.
    bool GetNext();

    // Inform the tokenizer whether the next token should be treated as a base64
    // encoded value. If |expect_base64_token| is true, |GetNext| will treat the
    // next token as a base64 encoded value, and will include the trailing '='
    // padding rather than attempt to split the token into a name/value pair.
    // In this case, |name| will be empty, and |value| will contain the token.
    // Subsequent calls to |GetNext()| will not treat the token like a base64
    // encoded token unless the caller again calls |set_expect_base64_token|.
    void set_expect_base64_token(bool expect_base64_token) {
      expect_base64_token_ = expect_base64_token;
    }

    // The name of the current name-value pair.
    std::string::const_iterator name_begin() const { return name_begin_; }
    std::string::const_iterator name_end() const { return name_end_; }
    std::string name() const {
      return std::string(name_begin_, name_end_);
    }

    // The value of the current name-value pair.
    std::string::const_iterator value_begin() const { return value_begin_; }
    std::string::const_iterator value_end() const { return value_end_; }
    std::string value() const {
      return std::string(value_begin_, value_end_);
    }

    // If value() has quotemarks, unquote it.
    std::string unquoted_value() const;

    // True if the name-value pair's value has quote marks.
    bool value_is_quoted() const { return value_is_quoted_; }

   private:
    void Init(std::string::const_iterator begin,
              std::string::const_iterator end);

    HttpUtil::ValuesIterator props_;
    bool valid_;

    std::string::const_iterator begin_;
    std::string::const_iterator end_;

    std::string::const_iterator scheme_begin_;
    std::string::const_iterator scheme_end_;

    std::string::const_iterator name_begin_;
    std::string::const_iterator name_end_;

    std::string::const_iterator value_begin_;
    std::string::const_iterator value_end_;

    bool value_is_quoted_;
    bool expect_base64_token_;
  };
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_H_
