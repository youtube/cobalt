// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_KEYED_SERVICE_H_
#define CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_KEYED_SERVICE_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/password_store_sites.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class GURL;

namespace login_detection {

// Comparator that converts string to url::Origin and performs the comparison.
// Comparing origins avoid inconsistencies with string comparison. For
// example, https://foo.com https://foo.com/ https://foo.com:443 are all the
// same.
struct OriginComparator {
  bool operator()(const std::string& a, const std::string& b) const;
};

// Keyed service than can be used to get the type of login detected on a
// navigation.
class LoginDetectionKeyedService : public KeyedService {
 public:
  explicit LoginDetectionKeyedService(Profile* profile);
  ~LoginDetectionKeyedService() override;

  // Returns the login type detected for |url|. This does not perform any
  // detection. Only returns types based on previously detected logins saved in
  // persistent memory.
  LoginDetectionType GetPersistentLoginDetection(const GURL& url) const;

 private:
  // Guaranteed to outlive |this|.
  raw_ptr<Profile> profile_;

  // Set of sites that should be treated as logged-in, retrieved from field
  // trial.
  const std::set<std::string, OriginComparator> field_trial_logged_in_sites_;

  const PasswordStoreSites profile_password_sites_;
  const PasswordStoreSites account_password_sites_;
};

}  // namespace login_detection

#endif  // CHROME_BROWSER_LOGIN_DETECTION_LOGIN_DETECTION_KEYED_SERVICE_H_
