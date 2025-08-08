// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PASSTHROUGH_OAUTH_TOKEN_GETTER_H_
#define REMOTING_BASE_PASSTHROUGH_OAUTH_TOKEN_GETTER_H_

#include "remoting/base/oauth_token_getter.h"

namespace remoting {

// An OAuthTokenGetter implementation that simply passes |username| and
// |access_token| when CallWithToken() is called.
class PassthroughOAuthTokenGetter : public OAuthTokenGetter {
 public:
  // Creates a PassthroughOAuthTokenGetter with empty username and access token.
  // Caller needs to set them with set_username() and set_access_token().
  PassthroughOAuthTokenGetter();
  PassthroughOAuthTokenGetter(const std::string& username,
                              const std::string& access_token);
  ~PassthroughOAuthTokenGetter() override;

  // OAuthTokenGetter overrides.
  void CallWithToken(OAuthTokenGetter::TokenCallback on_access_token) override;
  void InvalidateCache() override;

  void set_username(const std::string& username) { username_ = username; }

  void set_access_token(const std::string& access_token) {
    access_token_ = access_token;
  }

 private:
  std::string username_;
  std::string access_token_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PASSTHROUGH_OAUTH_TOKEN_GETTER_H_
