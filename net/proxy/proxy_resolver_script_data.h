// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_SCRIPT_DATA_H_
#define NET_PROXY_PROXY_RESOLVER_SCRIPT_DATA_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"

namespace net {

// Reference-counted wrapper for passing around a PAC script specification.
// The PAC script can be either specified via a URL, a deferred URL for
// auto-detect, or the actual javascript program text.
//
// This is thread-safe so it can be used by multi-threaded implementations of
// ProxyResolver to share the data between threads.
class ProxyResolverScriptData
    : public base::RefCountedThreadSafe<ProxyResolverScriptData> {
 public:
  enum Type {
    TYPE_SCRIPT_CONTENTS,
    TYPE_SCRIPT_URL,
    TYPE_AUTO_DETECT,
  };

  // Creates a script data given the UTF8 bytes of the content.
  static scoped_refptr<ProxyResolverScriptData> FromUTF8(
      const std::string& utf8);

  // Creates a script data given the UTF16 bytes of the content.
  static scoped_refptr<ProxyResolverScriptData> FromUTF16(
      const string16& utf16);

  // Creates a script data given a URL to the PAC script.
  static scoped_refptr<ProxyResolverScriptData> FromURL(const GURL& url);

  // Creates a script data for using an automatically detected PAC URL.
  static scoped_refptr<ProxyResolverScriptData> ForAutoDetect();

  Type type() const {
    return type_;
  }

  // Returns the contents of the script as UTF16.
  // (only valid for type() == TYPE_SCRIPT_CONTENTS).
  const string16& utf16() const;

  // Returns the URL of the script.
  // (only valid for type() == TYPE_SCRIPT_URL).
  const GURL& url() const;

 private:
  friend class base::RefCountedThreadSafe<ProxyResolverScriptData>;
  ProxyResolverScriptData(Type type,
                          const GURL& url,
                          const string16& utf16);
  virtual ~ProxyResolverScriptData();


  const Type type_;
  const GURL url_;
  const string16 utf16_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_SCRIPT_DATA_H_
