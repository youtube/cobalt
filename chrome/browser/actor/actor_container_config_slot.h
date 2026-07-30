// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_CONTAINER_CONFIG_SLOT_H_
#define CHROME_BROWSER_ACTOR_ACTOR_CONTAINER_CONFIG_SLOT_H_

#include "base/types/optional_ref.h"
#include "chrome/browser/actor/actor_container_config.h"

namespace optimization_guide::proto {
class AgentContainerConfig;
}

namespace actor {

// A slot that optionally holds a ActorContainerConfig. The slot may be assigned
// to at most once, and it may be locked in the "empty" state.
class ActorContainerConfigSlot {
 public:
  ActorContainerConfigSlot();
  ActorContainerConfigSlot(const ActorContainerConfigSlot&) = delete;
  ActorContainerConfigSlot& operator=(const ActorContainerConfigSlot&) = delete;
  ActorContainerConfigSlot(ActorContainerConfigSlot&&) = delete;
  ActorContainerConfigSlot& operator=(ActorContainerConfigSlot&) = delete;
  ~ActorContainerConfigSlot();

  // Assigns the `config` to this instance, if provided. This method is a no-op
  // except for the first time it is called (even if the first call provided
  // `std::nullopt`).
  void Assign(
      base::optional_ref<const optimization_guide::proto::AgentContainerConfig>
          config);

  bool has_value() const { return config_.has_value(); }

  const ActorContainerConfig& value() const { return config_.value(); }

 private:
  bool assigned_ = false;
  std::optional<ActorContainerConfig> config_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_CONTAINER_CONFIG_SLOT_H_
