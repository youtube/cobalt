// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_URL_SECURITY_MANAGER_H_
#define NET_HTTP_URL_SECURITY_MANAGER_H_

class GURL;

namespace net {

class HttpAuthFilter;

// The URL security manager controls the policies (allow, deny, prompt user)
// regarding URL actions (e.g., sending the default credentials to a server).
//
// On Windows, we honor the WinINet/IE settings and group policy related to
// URL Security Zones.  See the Microsoft Knowledge Base article 182569
// "Internet Explorer security zones registry entries for advanced users"
// (http://support.microsoft.com/kb/182569) for more info on these registry
// keys.
class URLSecurityManager {
 public:
  // The UrlSecurityManager does not take ownership of the HttpAuthFilter.
  explicit URLSecurityManager(const HttpAuthFilter* whitelist);
  virtual ~URLSecurityManager() {}

  // Creates a platform-dependent instance of URLSecurityManager.
  static URLSecurityManager* Create(const HttpAuthFilter* whitelist);

  // Returns true if we can send the default credentials to the server at
  // |auth_origin| for HTTP NTLM or Negotiate authentication.
  virtual bool CanUseDefaultCredentials(const GURL& auth_origin) const = 0;

 protected:
  const HttpAuthFilter* whitelist_;
};

}  // namespace net

#endif  // NET_HTTP_URL_SECURITY_MANAGER_H_
