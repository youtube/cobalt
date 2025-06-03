// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_TIME_CONVERSIONS_H_
#define NET_CERT_TIME_CONVERSIONS_H_

#include <stddef.h>
#include <stdint.h>

#include "net/base/net_export.h"
#include "net/der/encode_values.h"

namespace base {
class Time;
}

namespace net {

struct GeneralizedTime;

// Encodes |time|, a UTC-based time, to DER |generalized_time|, for comparing
// against other GeneralizedTime objects. Returns true on success or false if
// the time is not representable as a Generalized time.The millisecond component
// of |time| is discarded.
NET_EXPORT bool EncodeTimeAsGeneralizedTime(
    const base::Time& time,
    der::GeneralizedTime* generalized_time);

// Converts a GeneralizedTime struct to a base::Time, returning true on success
// or false if |generalized| was invalid.
NET_EXPORT bool GeneralizedTimeToTime(const der::GeneralizedTime& generalized,
                                      base::Time* result);

}  // namespace net

#endif  // NET_CERT_TIME_CONVERSIONS_H_
