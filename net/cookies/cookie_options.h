// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_COOKIES_COOKIE_OPTIONS_H_
#define NET_COOKIES_COOKIE_OPTIONS_H_

namespace net {

class CookieOptions {
 public:
  // Default is to exclude httponly, which means:
  // - reading operations will not return httponly cookies.
  // - writing operations will not write httponly cookies.
  CookieOptions()
      : exclude_httponly_(true) {
  }

  void set_exclude_httponly() { exclude_httponly_ = true; }
  void set_include_httponly() { exclude_httponly_ = false; }
  bool exclude_httponly() const { return exclude_httponly_; }

 private:
  bool exclude_httponly_;
};
}  // namespace net

#endif  // NET_COOKIES_COOKIE_OPTIONS_H_

