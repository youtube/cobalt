// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_credential_state.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/server_bound_cert_service.h"

namespace net {

namespace {

GURL GetCanonicalOrigin(const GURL& url) {
  std::string domain =
      ServerBoundCertService::GetDomainForHost(url.host());
  DCHECK(!domain.empty());
  if (domain == url.host())
    return url.GetOrigin();
  return GURL(url.scheme() + "://" + domain + ":" + url.port());
}

}  // namespace

const size_t SpdyCredentialState::kDefaultNumSlots = 8;
const size_t SpdyCredentialState::kNoEntry = 0;

SpdyCredentialState::SpdyCredentialState(size_t num_slots)
  : slots_(num_slots),
    last_added_(-1) {}

SpdyCredentialState::~SpdyCredentialState() {}

bool SpdyCredentialState::HasCredential(const GURL& origin) const {
  return FindCredentialSlot(origin) != kNoEntry;
}

size_t SpdyCredentialState::SetHasCredential(const GURL& origin) {
  size_t i = FindCredentialSlot(origin);
  if (i != kNoEntry)
    return i;
  // Add the new entry at the next index following the index of the last
  // entry added, or at index 0 if the last added index is the last index.
  if (last_added_ + 1 == slots_.size()) {
    last_added_ = 0;
  } else {
    last_added_++;
  }
  slots_[last_added_] = GetCanonicalOrigin(origin);
  return last_added_ + 1;
}

size_t SpdyCredentialState::FindCredentialSlot(const GURL& origin) const {
  GURL url = GetCanonicalOrigin(origin);
  for (size_t i = 0; i < slots_.size(); i++) {
    if (url == slots_[i])
      return i + 1;
  }
  return kNoEntry;
}

void SpdyCredentialState::Resize(size_t size) {
  slots_.resize(size);
  if (last_added_ >= slots_.size())
    last_added_ = slots_.size() - 1;
}

}  // namespace net
