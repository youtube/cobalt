// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/url_security_manager.h"

namespace net {

URLSecurityManager::URLSecurityManager(const HttpAuthFilter* whitelist)
    : whitelist_(whitelist) {
}

}  //  namespace net
