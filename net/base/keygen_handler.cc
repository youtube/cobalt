// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/keygen_handler.h"

#include "base/logging.h"

namespace net {

KeygenHandler::Cache* KeygenHandler::Cache::GetInstance() {
  return Singleton<Cache>::get();
}

void KeygenHandler::Cache::Insert(const std::string& public_key_info,
                                     const KeyLocation& location) {
  AutoLock lock(lock_);

  DCHECK(!public_key_info.empty()) << "Only insert valid public key structures";
  cache_[public_key_info] = location;
}

bool KeygenHandler::Cache::Find(const std::string& public_key_info,
                                KeyLocation* location) {
  AutoLock lock(lock_);

  KeyLocationMap::iterator iter = cache_.find(public_key_info);

  if (iter == cache_.end())
    return false;

  *location = iter->second;
  return true;
}

}  // namespace net
