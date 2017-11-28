// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_STORE_TEST_HELPERS_H_
#define NET_COOKIES_COOKIE_STORE_TEST_HELPERS_H_

#include "net/cookies/cookie_monster.h"

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class DelayedCookieMonster : public CookieStore {
 public:
  DelayedCookieMonster();

  // Call the asynchronous CookieMonster function, expect it to immediately
  // invoke the internal callback.
  // Post a delayed task to invoke the original callback with the results.

  virtual void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const CookieOptions& options,
      const CookieMonster::SetCookiesCallback& callback) override;

  virtual void GetCookiesWithOptionsAsync(
      const GURL& url, const CookieOptions& options,
      const CookieMonster::GetCookiesCallback& callback) override;

  virtual void GetCookiesWithInfoAsync(
      const GURL& url,
      const CookieOptions& options,
      const CookieMonster::GetCookieInfoCallback& callback) override;

  virtual bool SetCookieWithOptions(const GURL& url,
                                    const std::string& cookie_line,
                                    const CookieOptions& options);

  virtual std::string GetCookiesWithOptions(const GURL& url,
                                            const CookieOptions& options);

  virtual void GetCookiesWithInfo(const GURL& url,
                                  const CookieOptions& options,
                                  std::string* cookie_line,
                                  std::vector<CookieInfo>* cookie_infos);

  virtual void DeleteCookie(const GURL& url,
                            const std::string& cookie_name);

  virtual void DeleteCookieAsync(const GURL& url,
                                 const std::string& cookie_name,
                                 const base::Closure& callback) override;

  virtual void DeleteAllCreatedBetweenAsync(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      const DeleteCallback& callback) override;

  virtual void DeleteSessionCookiesAsync(const DeleteCallback&) override;

  virtual CookieMonster* GetCookieMonster() override;

 private:

  // Be called immediately from CookieMonster.

  void GetCookiesInternalCallback(
      const std::string& cookie_line,
      const std::vector<CookieStore::CookieInfo>& cookie_info);

  void SetCookiesInternalCallback(bool result);

  void GetCookiesWithOptionsInternalCallback(const std::string& cookie);

  // Invoke the original callbacks.

  void InvokeGetCookiesCallback(
      const CookieMonster::GetCookieInfoCallback& callback);

  void InvokeSetCookiesCallback(
      const CookieMonster::SetCookiesCallback& callback);

  void InvokeGetCookieStringCallback(
      const CookieMonster::GetCookiesCallback& callback);

  friend class base::RefCountedThreadSafe<DelayedCookieMonster>;
  virtual ~DelayedCookieMonster();

  scoped_refptr<CookieMonster> cookie_monster_;

  bool did_run_;
  bool result_;
  std::string cookie_;
  std::string cookie_line_;
  std::vector<CookieStore::CookieInfo> cookie_info_;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_STORE_TEST_HELPERS_H_
