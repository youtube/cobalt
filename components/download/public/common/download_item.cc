// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_item.h"

namespace download {

DownloadItem::Observer::~Observer() {
  CHECK(!IsInObserverList());
}

void DownloadItem::SetStateForTesting(DownloadState state) {
  NOTREACHED();
}

void DownloadItem::SetDownloadUrlForTesting(GURL url) {
  NOTREACHED();
}

}  // namespace download
