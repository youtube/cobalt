// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_FILE_FORMAT_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_FILE_FORMAT_H_

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"

namespace safe_browsing {

// Thin wrapper around either a V4StoreFileFormat or a V5StoreFileFormat.
class SBStoreFileFormat {
 public:
  explicit SBStoreFileFormat(V4StoreFileFormat* v4_file_format)
      : v4_file_format_(v4_file_format) {}
  explicit SBStoreFileFormat(V5StoreFileFormat* v5_file_format)
      : v5_file_format_(v5_file_format) {}

  const V4StoreFileFormat& v4_file_format() const {
    CHECK(v4_file_format_);
    return *v4_file_format_;
  }
  V4StoreFileFormat* v4_file_format_mutable() {
    CHECK(v4_file_format_);
    return v4_file_format_;
  }

  const V5StoreFileFormat& v5_file_format() const {
    CHECK(v5_file_format_);
    return *v5_file_format_;
  }
  V5StoreFileFormat* v5_file_format_mutable() {
    CHECK(v5_file_format_);
    return v5_file_format_;
  }

 private:
  raw_ptr<V4StoreFileFormat> v4_file_format_ = nullptr;
  raw_ptr<V5StoreFileFormat> v5_file_format_ = nullptr;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_FILE_FORMAT_H_
