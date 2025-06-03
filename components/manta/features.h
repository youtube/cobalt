// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_FEATURES_H_
#define COMPONENTS_MANTA_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace manta::features {

// This flag is used for enabling the Manta Service, a profile keyed service for
// the google chrome Manta project.
COMPONENT_EXPORT(MANTA) BASE_DECLARE_FEATURE(kMantaService);

COMPONENT_EXPORT(MANTA) bool IsMantaServiceEnabled();

}  // namespace manta::features

#endif  // COMPONENTS_MANTA_FEATURES_H_
