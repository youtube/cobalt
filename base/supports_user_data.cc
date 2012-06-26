// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/supports_user_data.h"

namespace base {

SupportsUserData::SupportsUserData() {}

SupportsUserData::Data* SupportsUserData::GetUserData(const void* key) const {
  DataMap::const_iterator found = user_data_.find(key);
  if (found != user_data_.end())
    return found->second.get();
  return NULL;
}

void SupportsUserData::SetUserData(const void* key, Data* data) {
  user_data_[key] = linked_ptr<Data>(data);
}

SupportsUserData::~SupportsUserData() {}

}  // namespace base
