// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_LOGIN_CONTEXT_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_LOGIN_CONTEXT_H_

#include "base/containers/flat_map.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "url/origin.h"

namespace autofill {

// Encapsulates the navigation state observed when an actor login attempt
// begins. Used by `ActorOneTimeTokenFillingService` to track navigations across
// targeted login frames and determine if subsequent OTP filling belongs to
// the same login flow.
struct ActorLoginContext {
  ActorLoginContext();
  ActorLoginContext(
      url::Origin origin,
      bool should_use_strong_matching,
      base::flat_map<content::FrameTreeNodeId, int> navigations_per_frame);
  ActorLoginContext(const ActorLoginContext&);
  ActorLoginContext(ActorLoginContext&&);
  ActorLoginContext& operator=(const ActorLoginContext&);
  ActorLoginContext& operator=(ActorLoginContext&&);
  ~ActorLoginContext();

  url::Origin origin;
  bool should_use_strong_matching = false;
  // Maps `FrameTreeNodeId` to its specific navigation count, the main frame
  // is part of this map as well.
  base::flat_map<content::FrameTreeNodeId, int> navigations_per_frame;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_LOGIN_CONTEXT_H_
