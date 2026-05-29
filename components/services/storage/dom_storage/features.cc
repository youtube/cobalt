// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/features.h"

namespace storage {

BASE_FEATURE(kCoalesceStorageAreaCommits,
             "CoalesceStorageAreaCommits",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_COBALT)  
BASE_FEATURE(kLocalStorageDeleteLockFile,
             "LocalStorageDeleteLockFile",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace storage
