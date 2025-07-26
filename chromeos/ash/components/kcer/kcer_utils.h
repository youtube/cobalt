// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KCER_KCER_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_KCER_KCER_UTILS_H_

#include "chromeos/ash/components/kcer/kcer.h"

namespace kcer {

// Generate a vector with all the signing schemes that a key can perform based
// on the `key_type` and whether the token supports PSS.
std::vector<SigningScheme> GetSupportedSigningSchemes(bool supports_pss,
                                                      KeyType key_type);

}  // namespace kcer

#endif  // CHROMEOS_ASH_COMPONENTS_KCER_KCER_UTILS_H_
