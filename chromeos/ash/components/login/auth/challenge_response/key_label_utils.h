// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_KEY_LABEL_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_KEY_LABEL_UTILS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"

namespace ash {

// Generates the cryptohome user key label for the given challenge-response key
// information. Currently the constraint is that |challenge_response_keys| must
// contain exactly one item.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
std::string GenerateChallengeResponseKeyLabel(
    const std::vector<ChallengeResponseKey>& challenge_response_keys);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_KEY_LABEL_UTILS_H_
