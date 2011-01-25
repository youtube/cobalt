// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_URL_SECURITY_MANAGER_H_
#define NET_HTTP_URL_SECURITY_MANAGER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/basictypes.h"

class GURL;

namespace net {

class HttpAuthFilter;

// The URL security manager controls the policies (allow, deny, prompt user)
// regarding URL actions (e.g., sending the default credentials to a server).
class URLSecurityManager {
 public:
  URLSecurityManager() {}
  virtual ~URLSecurityManager() {}

  // Creates a platform-dependent instance of URLSecurityManager.
  //
  // |whitelist_default| is the whitelist of servers that default credentials
  // can be used with during NTLM or Negotiate authentication. If
  // |whitelist_default| is NULL and the platform is Windows, it indicates
  // that security zone mapping should be used to determine whether default
  // credentials sxhould be used. If |whitelist_default| is NULL and the
  // platform is non-Windows, it indicates that no servers should be
  // whitelisted.
  //
  // |whitelist_delegate| is the whitelist of servers that are allowed
  // to have Delegated Kerberos tickets. If |whitelist_delegate| is NULL,
  // no servers can have delegated Kerberos tickets.
  //
  // Both |whitelist_default| and |whitelist_delegate| will be owned by
  // the created URLSecurityManager.
  //
  // TODO(cbentzel): Perhaps it's better to make a non-abstract HttpAuthFilter
  //                 and just copy into the URLSecurityManager?
  static URLSecurityManager* Create(const HttpAuthFilter* whitelist_default,
                                    const HttpAuthFilter* whitelist_delegate);

  // Returns true if we can send the default credentials to the server at
  // |auth_origin| for HTTP NTLM or Negotiate authentication.
  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) const = 0;

  // Returns true if Kerberos delegation is allowed for the server at
  // |auth_origin| for HTTP Negotiate authentication.
  virtual bool CanDelegate(const GURL& auth_origin) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLSecurityManager);
};

class URLSecurityManagerWhitelist : public URLSecurityManager {
 public:
  // The URLSecurityManagerWhitelist takes ownership of the whitelists.
  URLSecurityManagerWhitelist(const HttpAuthFilter* whitelist_default,
                              const HttpAuthFilter* whitelist_delegation);
  virtual ~URLSecurityManagerWhitelist();

  // URLSecurityManager methods.
  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) const;
  virtual bool CanDelegate(const GURL& auth_origin) const;

 private:
  scoped_ptr<const HttpAuthFilter> whitelist_default_;
  scoped_ptr<const HttpAuthFilter> whitelist_delegate_;

  DISALLOW_COPY_AND_ASSIGN(URLSecurityManagerWhitelist);
};

#if defined(UNIT_TEST)
// An URLSecurityManager which is very permissive.
class URLSecurityManagerAllow : public URLSecurityManager {
 public:
  URLSecurityManagerAllow() {}
  virtual ~URLSecurityManagerAllow() {}

  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) const {
    return true;
  }
  virtual bool CanDelegate(const GURL& auth_origin) const {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(URLSecurityManagerAllow);
};
#endif  // defined(UNIT_TEST)

}  // namespace net

#endif  // NET_HTTP_URL_SECURITY_MANAGER_H_
