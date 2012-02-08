// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_credential_state.h"

#include "base/logging.h"

namespace net {

const size_t SpdyCredentialState::kDefaultNumSlots = 8;
const size_t SpdyCredentialState::kNoEntry = -1;

SpdyCredentialState::SpdyCredentialState(size_t num_slots)
  : slots_(num_slots),
    last_added_(-1) {}

SpdyCredentialState::~SpdyCredentialState() {}

bool SpdyCredentialState::HasCredential(const HostPortPair& origin) const {
  return FindPosition(origin) != kNoEntry;
}

size_t SpdyCredentialState::SetHasCredential(const HostPortPair& origin) {
  size_t i = FindPosition(origin);
  if (i != kNoEntry)
    return i;
  // Add the new entry at the next index following the index of the last
  // entry added, or at index 0 if the last added index is the last index.
  if (last_added_ + 1 == slots_.size()) {
    last_added_ = 0;
  } else {
    last_added_++;
  }
  slots_[last_added_] = origin;
  return last_added_;
}

size_t SpdyCredentialState::FindPosition(const HostPortPair& origin) const {
  for (size_t i = 0; i < slots_.size(); i++) {
    if (slots_[i].Equals(origin))
      return i;
  }
  return kNoEntry;
}

void SpdyCredentialState::Resize(size_t size) {
  slots_.resize(size);
  if (last_added_ >= slots_.size())
    last_added_ = slots_.size() - 1;
}

}  // namespace net
