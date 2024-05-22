// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_change_dispatcher.h"

namespace net {

const char* CookieChangeCauseToString(CookieChangeCause cause) {
  const char* cause_string = "INVALID";
  switch (cause) {
    case CookieChangeCause::INSERTED:
      cause_string = "inserted";
      break;
    case CookieChangeCause::EXPLICIT:
      cause_string = "explicit";
      break;
    case CookieChangeCause::UNKNOWN_DELETION:
      cause_string = "unknown";
      break;
    case CookieChangeCause::OVERWRITE:
      cause_string = "overwrite";
      break;
    case CookieChangeCause::EXPIRED:
      cause_string = "expired";
      break;
    case CookieChangeCause::EVICTED:
      cause_string = "evicted";
      break;
    case CookieChangeCause::EXPIRED_OVERWRITE:
      cause_string = "expired_overwrite";
      break;
  }
  return cause_string;
}

CookieChangeInfo::CookieChangeInfo() = default;

CookieChangeInfo::CookieChangeInfo(const CanonicalCookie& cookie,
                                   CookieAccessResult access_result,
                                   CookieChangeCause cause)
    : cookie(cookie), access_result(access_result), cause(cause) {
  DCHECK(access_result.status.IsInclude());
  if (CookieChangeCauseIsDeletion(cause)) {
    DCHECK_EQ(access_result.effective_same_site,
              CookieEffectiveSameSite::UNDEFINED);
  }
}

CookieChangeInfo::~CookieChangeInfo() = default;

bool CookieChangeCauseIsDeletion(CookieChangeCause cause) {
  return cause != CookieChangeCause::INSERTED;
}

}  // namespace net
