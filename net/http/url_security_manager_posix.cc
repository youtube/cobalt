// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth.h"
#include "net/http/http_auth_filter.h"
#include "net/http/url_security_manager.h"

#include "googleurl/src/gurl.h"

namespace net {

class URLSecurityManagerPosix : public URLSecurityManager {
 public:
  explicit URLSecurityManagerPosix(const HttpAuthFilter* whitelist)
      : URLSecurityManager(whitelist) {}

  // URLSecurityManager methods:
  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) const;
};

bool URLSecurityManagerPosix::CanUseDefaultCredentials(
    const GURL& auth_origin) const {
  if (whitelist_)
    return whitelist_->IsValid(auth_origin, HttpAuth::AUTH_SERVER);
  return false;
}

// static
URLSecurityManager* URLSecurityManager::Create(
    const HttpAuthFilter* whitelist) {
  return new URLSecurityManagerPosix(whitelist);
}

}  //  namespace net
