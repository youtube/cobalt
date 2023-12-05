// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/revocation_util.h"

#include "base/time/time.h"
#include "net/der/encode_values.h"
#include "net/der/parse_values.h"

namespace net {

namespace {

constexpr int64_t kMinValidTime = -62167219200;  // 0000-01-01 00:00:00 UTC
constexpr int64_t kMaxValidTime = 253402300799;  // 9999-12-31 23:59:59 UTC

}  // namespace

bool CheckRevocationDateValid(const der::GeneralizedTime& this_update,
                              const der::GeneralizedTime* next_update,
                              const base::Time& verify_time,
                              const base::TimeDelta& max_age) {
  der::GeneralizedTime verify_time_der;
  if (!der::EncodeTimeAsGeneralizedTime(verify_time, &verify_time_der))
    return false;

  if (this_update > verify_time_der)
    return false;  // Response is not yet valid.

  if (next_update && (*next_update <= verify_time_der))
    return false;  // Response is no longer valid.

  der::GeneralizedTime earliest_this_update;
  if (!der::EncodeTimeAsGeneralizedTime(verify_time - max_age,
                                        &earliest_this_update)) {
    return false;
  }
  if (this_update < earliest_this_update)
    return false;  // Response is too old.

  return true;
}

}  // namespace net
