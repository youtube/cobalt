// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"

#include <utility>

namespace autofill {

ActorLoginContext::ActorLoginContext() = default;

ActorLoginContext::ActorLoginContext(
    url::Origin origin,
    bool should_use_strong_matching,
    base::flat_map<content::FrameTreeNodeId, int> navigations_per_frame)
    : origin(std::move(origin)),
      should_use_strong_matching(should_use_strong_matching),
      navigations_per_frame(std::move(navigations_per_frame)) {}

ActorLoginContext::ActorLoginContext(const ActorLoginContext&) = default;

ActorLoginContext::ActorLoginContext(ActorLoginContext&&) = default;

ActorLoginContext& ActorLoginContext::operator=(const ActorLoginContext&) =
    default;

ActorLoginContext& ActorLoginContext::operator=(ActorLoginContext&&) = default;

ActorLoginContext::~ActorLoginContext() = default;

}  // namespace autofill
