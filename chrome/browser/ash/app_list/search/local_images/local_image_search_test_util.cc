// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_images/local_image_search_test_util.h"

#include "chrome/browser/ash/app_list/search/local_images/annotation_storage.h"

namespace app_list {

bool operator==(const ImageInfo& i1, const ImageInfo& i2) {
  return i1.path == i2.path && i1.annotations == i2.annotations &&
         i1.last_modified == i2.last_modified;
}

bool operator==(const FileSearchResult& f1, const FileSearchResult& f2) {
  return f1.path == f2.path && f1.last_modified == f2.last_modified &&
         std::abs(f1.relevance - f2.relevance) < 0.0000001;
}

}  // namespace app_list
