// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/url_security_manager.h"

#include "googleurl/src/gurl.h"

namespace net {

class URLSecurityManagerDefault : public URLSecurityManager {
 public:
  // URLSecurityManager methods:
  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) const {
    // TODO(wtc): use command-line whitelist.
    return false;
  }
};

// static
URLSecurityManager* URLSecurityManager::Create() {
  return new URLSecurityManagerDefault;
}

}  //  namespace net
