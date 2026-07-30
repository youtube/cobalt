// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"

#import "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

std::unique_ptr<ActorToolRequest> MakeSuccessfulActorToolRequest(
    web::WebStateID identifier) {
  return std::make_unique<ActorToolRequest>(
      MakeSuccessfulActorAction(identifier));
}

optimization_guide::proto::Action MakeSuccessfulActorAction(
    web::WebStateID identifier) {
  optimization_guide::proto::Action action;
  auto* wait = action.mutable_wait();
  wait->set_wait_time_ms(0);
  if (identifier.valid()) {
    wait->set_observe_tab_id(identifier.identifier());
  }
  return action;
}

std::unique_ptr<ActorToolRequest> MakeFailingActorToolRequest() {
  return std::make_unique<ActorToolRequest>(MakeFailingActorAction());
}

optimization_guide::proto::Action MakeFailingActorAction() {
  // This proto will fail initial validation when a tab_id is not set.
  optimization_guide::proto::Action action;
  action.mutable_click();
  return action;
}

}  // namespace actor
