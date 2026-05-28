// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_

#include "base/features.h"

namespace storage {

BASE_DECLARE_FEATURE(kCoalesceStorageAreaCommits);
BASE_DECLARE_FEATURE(kLocalStorageDeleteLockFile);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_FEATURES_H_
