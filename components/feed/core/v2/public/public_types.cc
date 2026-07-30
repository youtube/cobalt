// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/types.h"

#include <ostream>
#include <tuple>
#include "components/feed/core/proto/v2/ui.pb.h"

namespace feed {

AccountInfo::AccountInfo() = default;
AccountInfo::AccountInfo(const GaiaId& gaia, const std::string& email)
    : gaia(gaia), email(email) {}
AccountInfo::AccountInfo(CoreAccountInfo account_info)
    : gaia(std::move(account_info.gaia)),
      email(std::move(account_info.email)) {}
bool AccountInfo::IsEmpty() const {
  DCHECK_EQ(gaia.empty(), email.empty());
  return gaia.empty();
}

std::ostream& operator<<(std::ostream& os, const AccountInfo& o) {
  if (o.IsEmpty()) {
    return os << "signed-out";
  }
  return os << o.gaia << ":" << o.email;
}

// operator<< functions below are for test purposes, and shouldn't be called
// from production code to avoid a binary size impact.

std::ostream& operator<<(std::ostream& os, const NetworkResponseInfo& o) {
  return os << "NetworkResponseInfo{"
            << " status_code=" << o.status_code
            << " fetch_duration=" << o.fetch_duration
            << " fetch_time=" << o.fetch_time
            << " bless_nonce=" << o.bless_nonce
            << " base_request_url=" << o.base_request_url
            << " response_body_bytes=" << o.response_body_bytes
            << " account_info=" << o.account_info << "}";
}

}  // namespace feed
