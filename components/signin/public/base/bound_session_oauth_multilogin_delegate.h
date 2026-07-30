// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_BOUND_SESSION_OAUTH_MULTILOGIN_DELEGATE_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_BOUND_SESSION_OAUTH_MULTILOGIN_DELEGATE_H_

#include <string>
#include <utility>
#include <vector>

#include "url/gurl.h"

class OAuthMultiloginResult;

namespace signin {

// Handles cookie binding during OAuthMultiLogin request.
// OAuthMultiLogin can start a bound session or override an existing one.
// If cookies from OAuthMultiLogin are bound, it would be to the refresh token's
// binding key.
// There should be one delegate instance per `OAuthMultiloginHelper`.
class BoundSessionOAuthMultiLoginDelegate {
 public:
  virtual ~BoundSessionOAuthMultiLoginDelegate() = default;

  // Returns a list of (site_url, session_id) pairs for all sessions.
  virtual std::vector<std::pair<GURL, std::string>> GetAllSessions() const = 0;

  // Processes `DeviceBoundSession`(s) if available in `result`.
  // Called before setting cookies to avoid any possible race condition where
  // bound cookies are overridden by in-flight cookie rotation request.
  // Bound sessions impacted by OAuthMultiLogin have cookie rotation
  // paused till `OnCookiesSet()` is called.
  virtual void BeforeSetCookies(const OAuthMultiloginResult& result) = 0;

  // Resumes cookie rotation and overrides the existing sessions if needed.
  //
  // The caller MUST ensure that `BeforeSetCookies()` is called before
  // `OnCookiesSet()`.
  virtual void OnCookiesSet() = 0;
};
}  // namespace signin
#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_BOUND_SESSION_OAUTH_MULTILOGIN_DELEGATE_H_
