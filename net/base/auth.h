// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_AUTH_H__
#define NET_BASE_AUTH_H__
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"

namespace net {

// Holds info about an authentication challenge that we may want to display
// to the user.
class NET_EXPORT AuthChallengeInfo :
    public base::RefCountedThreadSafe<AuthChallengeInfo> {
 public:
  AuthChallengeInfo();

  // Determines whether two AuthChallengeInfo's are equivalent.
  bool Equals(const AuthChallengeInfo& other) const;

  // Whether this came from a server or a proxy.
  bool is_proxy;

  // The service issuing the challenge.
  HostPortPair challenger;

  // The authentication scheme used, such as "basic" or "digest". If the
  // |source| is FTP_SERVER, this is an empty string. The encoding is ASCII.
  std::string scheme;

  // The realm of the challenge. May be empty. The encoding is UTF-8.
  std::string realm;

 private:
  friend class base::RefCountedThreadSafe<AuthChallengeInfo>;
  ~AuthChallengeInfo();
};

// Authentication structures
enum AuthState {
  AUTH_STATE_DONT_NEED_AUTH,
  AUTH_STATE_NEED_AUTH,
  AUTH_STATE_HAVE_AUTH,
  AUTH_STATE_CANCELED
};

class AuthData : public base::RefCountedThreadSafe<AuthData> {
 public:
  AuthState state;  // whether we need, have, or gave up on authentication.
  string16 username;  // the username supplied to us for auth.
  string16 password;  // the password supplied to us for auth.

  // We wouldn't instantiate this class if we didn't need authentication.
  AuthData();

 private:
  friend class base::RefCountedThreadSafe<AuthData>;
  ~AuthData();
};

}  // namespace net

#endif  // NET_BASE_AUTH_H__
