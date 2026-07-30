// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/initiator_navigation_state_impl.h"

#include <utility>

#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"

namespace content {

InitiatorNavigationStateImpl::InitiatorNavigationStateImpl(
    const blink::LocalFrameToken& token,
    ChildProcessId process_id,
    scoped_refptr<PolicyContainerHost> policy_container_host,
    scoped_refptr<SiteInstanceImpl> site_instance)
    : frame_token_(token),
      process_id_(process_id),
      policy_container_host_(std::move(policy_container_host)),
      site_instance_(std::move(site_instance)) {
  CHECK(policy_container_host_);
  CHECK(site_instance_);
}

InitiatorNavigationStateImpl::~InitiatorNavigationStateImpl() = default;

}  // namespace content
