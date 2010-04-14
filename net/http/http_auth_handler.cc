// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

bool HttpAuthHandler::InitFromChallenge(
    HttpAuth::ChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const GURL& origin) {
  origin_ = origin;
  target_ = target;
  score_ = -1;
  properties_ = -1;

  bool ok = Init(challenge);

  // Init() is expected to set the scheme, realm, score, and properties.  The
  // realm may be empty.
  DCHECK(!ok || !scheme().empty());
  DCHECK(!ok || score_ != -1);
  DCHECK(!ok || properties_ != -1);

  return ok;
}

int HttpAuthHandler::ResolveCanonicalName(net::HostResolver* host_resolver,
                                          CompletionCallback* callback,
                                          const BoundNetLog& net_log) {
  NOTREACHED();
  LOG(ERROR) << ErrorToString(ERR_NOT_IMPLEMENTED);
  return ERR_NOT_IMPLEMENTED;
}

}  // namespace net
