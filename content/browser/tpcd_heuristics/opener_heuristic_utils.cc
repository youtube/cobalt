// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tpcd_heuristics/opener_heuristic_utils.h"

#include "url/gurl.h"

namespace content {

PopupProvider GetPopupProvider(const GURL& popup_url) {
  if (popup_url.DomainIs("google.com")) {
    return PopupProvider::kGoogle;
  }
  return PopupProvider::kUnknown;
}

}  // namespace content
