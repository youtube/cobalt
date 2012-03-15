// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_STORE_TEST_CALLBACKS_H_
#define NET_COOKIES_COOKIE_STORE_TEST_CALLBACKS_H_
#pragma once

#include <string>
#include <vector>

#include "net/cookies/cookie_store.h"

class MessageLoop;

namespace base {
class Thread;
}

namespace net {

// Defines common behaviour for the callbacks from GetCookies, SetCookies, etc.
// Asserts that the current thread is the expected invocation thread, sends a
// quit to the thread in which it was constructed.
class CookieCallback {
 public:
  // Indicates whether the callback has been called.
  bool did_run() { return did_run_; }

 protected:
  // Constructs a callback that expects to be called in the given thread and
  // will, upon execution, send a QUIT to the constructing thread.
  explicit CookieCallback(base::Thread* run_in_thread);

  // Constructs a callback that expects to be called in current thread and will
  // send a QUIT to the constructing thread.
  CookieCallback();

  // Tests whether the current thread was the caller's thread.
  // Sends a QUIT to the constructing thread.
  void CallbackEpilogue();

 private:
  bool did_run_;
  base::Thread* run_in_thread_;
  MessageLoop* run_in_loop_;
  MessageLoop* parent_loop_;
  MessageLoop* loop_to_quit_;
};

// Callback implementations for the asynchronous CookieStore methods.

class SetCookieCallback : public CookieCallback {
 public:
  SetCookieCallback();
  explicit SetCookieCallback(base::Thread* run_in_thread);

  void Run(bool result) {
    result_ = result;
    CallbackEpilogue();
  }

  bool result() { return result_; }

 private:
  bool result_;
};

class GetCookieStringCallback : public CookieCallback {
 public:
  GetCookieStringCallback();
  explicit GetCookieStringCallback(base::Thread* run_in_thread);

  void Run(const std::string& cookie) {
    cookie_ = cookie;
    CallbackEpilogue();
  }

  const std::string& cookie() { return cookie_; }

 private:
  std::string cookie_;
};

class GetCookiesWithInfoCallback : public CookieCallback {
 public:
  GetCookiesWithInfoCallback();
  explicit GetCookiesWithInfoCallback(base::Thread* run_in_thread);
  ~GetCookiesWithInfoCallback();

  void Run(
      const std::string& cookie_line,
      const std::vector<CookieStore::CookieInfo>& cookie_info) {
    cookie_line_ = cookie_line;
    cookie_info_ = cookie_info;
    CallbackEpilogue();
  }

  const std::string& cookie_line() { return cookie_line_; }
  const std::vector<CookieStore::CookieInfo>& cookie_info() {
    return cookie_info_;
  }

 private:
  std::string cookie_line_;
  std::vector<CookieStore::CookieInfo> cookie_info_;
};

class DeleteCallback : public CookieCallback {
 public:
  DeleteCallback();
  explicit DeleteCallback(base::Thread* run_in_thread);

  void Run(int num_deleted) {
    num_deleted_ = num_deleted;
    CallbackEpilogue();
  }

  int num_deleted() { return num_deleted_; }

 private:
  int num_deleted_;
};

class DeleteCookieCallback : public CookieCallback {
 public:
  DeleteCookieCallback();
  explicit DeleteCookieCallback(base::Thread* run_in_thread);

  void Run() {
    CallbackEpilogue();
  }
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_STORE_TEST_CALLBACKS_H_
