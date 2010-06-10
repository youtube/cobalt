// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_DIGEST_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_DIGEST_H_

#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"

// This is needed for the FRIEND_TEST() macro.
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

// Code for handling http digest authentication.
class HttpAuthHandlerDigest : public HttpAuthHandler {
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

 protected:
  virtual bool Init(HttpAuth::ChallengeTokenizer* challenge) {
    return ParseChallenge(challenge);
  }

  virtual int GenerateAuthTokenImpl(const std::wstring* username,
                                    const std::wstring* password,
                                    const HttpRequestInfo* request,
                                    CompletionCallback* callback,
                                    std::string* auth_token);

 private:
  FRIEND_TEST(HttpAuthHandlerDigestTest, ParseChallenge);
  FRIEND_TEST(HttpAuthHandlerDigestTest, AssembleCredentials);
  FRIEND_TEST(HttpNetworkTransactionTest, DigestPreAuthNonceCount);

  // Possible values for the "algorithm" property.
  enum DigestAlgorithm {
    // No algorithm was specified. According to RFC 2617 this means
    // we should default to ALGORITHM_MD5.
    ALGORITHM_UNSPECIFIED,

    // Hashes are run for every request.
    ALGORITHM_MD5,

    // Hash is run only once during the first WWW-Authenticate handshake.
    // (SESS means session).
    ALGORITHM_MD5_SESS,
  };

  // Possible values for "qop" -- may be or-ed together if there were
  // multiple comma separated values.
  enum QualityOfProtection {
    QOP_UNSPECIFIED = 0,
    QOP_AUTH = 1 << 0,
    QOP_AUTH_INT = 1 << 1,
  };

  explicit HttpAuthHandlerDigest(int nonce_count) : nonce_count_(nonce_count) {}

  ~HttpAuthHandlerDigest() {}

  // Parse the challenge, saving the results into this instance.
  // Returns true on success.
  bool ParseChallenge(HttpAuth::ChallengeTokenizer* challenge);

  // Parse an individual property. Returns true on success.
  bool ParseChallengeProperty(const std::string& name,
                              const std::string& value);

  // Generates a random string, to be used for client-nonce.
  static std::string GenerateNonce();

  // Convert enum value back to string.
  static std::string QopToString(int qop);
  static std::string AlgorithmToString(int algorithm);

  // Extract the method and path of the request, as needed by
  // the 'A2' production. (path may be a hostname for proxy).
  void GetRequestMethodAndPath(const HttpRequestInfo* request,
                               std::string* method,
                               std::string* path) const;

  // Build up  the 'response' production.
  std::string AssembleResponseDigest(const std::string& method,
                                     const std::string& path,
                                     const std::string& username,
                                     const std::string& password,
                                     const std::string& cnonce,
                                     const std::string& nc) const;

  // Build up  the value for (Authorization/Proxy-Authorization).
  std::string AssembleCredentials(const std::string& method,
                                  const std::string& path,
                                  const std::string& username,
                                  const std::string& password,
                                  const std::string& cnonce,
                                  int nonce_count) const;

  // Forces cnonce to be the same each time. This is used for unit tests.
  static void SetFixedCnonce(bool fixed_cnonce) {
    fixed_cnonce_ = fixed_cnonce;
  }

  // Information parsed from the challenge.
  std::string nonce_;
  std::string domain_;
  std::string opaque_;
  bool stale_;
  DigestAlgorithm algorithm_;
  int qop_; // Bitfield of QualityOfProtection

  int nonce_count_;

  // Forces the cnonce to be the same each time, for unit tests.
  static bool fixed_cnonce_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_DIGEST_H_
