// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_CREDENTIAL_STATE_H_
#define NET_SPDY_SPDY_CREDENTIAL_STATE_H_
#pragma once

#include <vector>

#include "googleurl/src/gurl.h"
#include "net/base/net_export.h"

namespace net {

// A class for tracking the credentials associated with a SPDY session.
class NET_EXPORT_PRIVATE SpdyCredentialState {
 public:
  explicit SpdyCredentialState(size_t num_slots);
  ~SpdyCredentialState();

  // Changes the number of credentials being tracked.  If the new size is
  // larger, then empty slots will be added to the end.  If the new size is
  // smaller than the current size, then the extra slots will be truncated
  // from the end.
  void Resize(size_t size);

  // Returns the one-based index in |slots_| for |url| or kNoEntry, if no entry
  // for |url| exists.
  size_t FindCredentialSlot(const GURL& url) const;

  // Returns true if there is a credential associated with |url|.
  bool HasCredential(const GURL& url) const;

  // Adds the new credentials to be associated with all origins matching
  // |url|.  If there is space, then it will add in the first available
  // position.  Otherwise, an existing credential will be evicted.  Returns
  // the slot in which this domain was added.
  size_t SetHasCredential(const GURL& url);

  // This value is defined as the default initial value in the SPDY spec unless
  // otherwise negotiated via SETTINGS.
  static const size_t kDefaultNumSlots;

  // Sentinel value to be returned by FindCredentialSlot when no entry exists.
  static const size_t kNoEntry;

 private:
  // Vector of origins that have credentials.
  std::vector<GURL> slots_;
  // Index of the last origin added to |slots_|.
  size_t last_added_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_CREDENTIAL_STATE_H_
