// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/url_security_manager.h"

#include "net/http/http_auth_filter.h"

namespace net {

URLSecurityManagerWhitelist::URLSecurityManagerWhitelist(
    HttpAuthFilter* whitelist) : whitelist_(whitelist) {
}

bool URLSecurityManagerWhitelist::CanUseDefaultCredentials(
    const GURL& auth_origin) {
  if (whitelist_.get())
    return whitelist_->IsValid(auth_origin, HttpAuth::AUTH_SERVER);
  return false;
}

}  //  namespace net
