// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_client_auth_cache.h"

#include "base/logging.h"
#include "net/base/x509_certificate.h"

namespace net {

SSLClientAuthCache::SSLClientAuthCache() {}

SSLClientAuthCache::~SSLClientAuthCache() {}

bool SSLClientAuthCache::Lookup(
    const std::string& server,
    scoped_refptr<X509Certificate>* certificate) {
  DCHECK(certificate);

  AuthCacheMap::iterator iter = cache_.find(server);
  if (iter == cache_.end())
    return false;

  *certificate = iter->second;
  return true;
}

void SSLClientAuthCache::Add(const std::string& server,
                             X509Certificate* value) {
  cache_[server] = value;

  // TODO(wtc): enforce a maximum number of entries.
}

void SSLClientAuthCache::Remove(const std::string& server) {
  cache_.erase(server);
}

void SSLClientAuthCache::Clear() {
  cache_.clear();
}

}  // namespace net
