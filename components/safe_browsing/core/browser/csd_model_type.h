// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CSD_MODEL_TYPE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CSD_MODEL_TYPE_H_

namespace safe_browsing {

// TODO(crbug.com/502615476): After client_side_phishing_model.h is moved into
// core, move enum definition back into it and delete this file.
enum class CSDModelType { kNone = 0, kFlatbuffer = 1 };

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CSD_MODEL_TYPE_H_
