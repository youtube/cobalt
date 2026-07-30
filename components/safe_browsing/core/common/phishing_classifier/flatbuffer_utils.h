// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_FLATBUFFER_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_FLATBUFFER_UTILS_H_

#include "components/safe_browsing/core/common/fbs/client_model_generated.h"

namespace safe_browsing {

// Verifies that the indices and fields of the flatbuffer model are valid.
bool VerifyCSDFlatBufferIndicesAndFields(const flat::ClientSideModel* model);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_PHISHING_CLASSIFIER_FLATBUFFER_UTILS_H_
