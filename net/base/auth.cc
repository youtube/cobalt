// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/auth.h"

namespace net {

AuthChallengeInfo::AuthChallengeInfo() : is_proxy(false) {
}

bool AuthChallengeInfo::Equals(const AuthChallengeInfo& that) const {
  return (this->is_proxy == that.is_proxy &&
          this->challenger.Equals(that.challenger) &&
          this->scheme == that.scheme &&
          this->realm == that.realm);
}

AuthChallengeInfo::~AuthChallengeInfo() {
}

AuthData::AuthData() : state(AUTH_STATE_NEED_AUTH) {
}

AuthData::~AuthData() {
}

AuthCredentials::AuthCredentials() {
}

AuthCredentials::~AuthCredentials() {
}

}  // namespace net
