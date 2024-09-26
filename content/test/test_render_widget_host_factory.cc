// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_widget_host_factory.h"

#include "content/test/test_render_widget_host.h"

namespace content {

TestRenderWidgetHostFactory::TestRenderWidgetHostFactory() {
  RenderWidgetHostFactory::RegisterFactory(this);
}

TestRenderWidgetHostFactory::~TestRenderWidgetHostFactory() {
  RenderWidgetHostFactory::UnregisterFactory();
}

std::unique_ptr<RenderWidgetHostImpl>
TestRenderWidgetHostFactory::CreateRenderWidgetHost(
    FrameTree* frame_tree,
    RenderWidgetHostDelegate* delegate,
    base::SafeRef<SiteInstanceGroup> site_instance_group,
    int32_t routing_id,
    bool hidden) {
  return TestRenderWidgetHost::Create(
      frame_tree, delegate, std::move(site_instance_group), routing_id, hidden);
}

}  // namespace content
