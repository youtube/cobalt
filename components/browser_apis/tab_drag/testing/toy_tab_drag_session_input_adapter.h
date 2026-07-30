// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_INPUT_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_INPUT_ADAPTER_H_

#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api {

class ToyTabDragSessionInputAdapter : public TabDragSessionInputAdapter {
 public:
  ToyTabDragSessionInputAdapter();
  ~ToyTabDragSessionInputAdapter() override;

  // TabDragSessionInputAdapter overrides:
  base::expected<void, mojo_base::mojom::ErrorPtr> StartInputCapture(
      EventCallback callback) override;

  void ReleaseInputCapture() override;

  void SendToyEvent(TabDragInputEvent::Type type,
                    const gfx::Point& screen_point = {});

  bool capture_started() const { return capture_started_; }
  bool capture_released() const { return capture_released_; }

 private:
  bool capture_started_ = false;
  bool capture_released_ = false;
  EventCallback callback_;
};

class TabDragSessionListener;

class ToyTabDragSessionInjector : public TabDragSessionInjector {
 public:
  ToyTabDragSessionInjector(TabDragSessionInputAdapter& adapter,
                            TabDragSessionListener& listener,
                            DropTargetRegistry& registry)
      : adapter_(adapter), listener_(listener), registry_(registry) {}
  ~ToyTabDragSessionInjector() override = default;

  TabDragSessionInputAdapter& GetInputAdapter() override { return *adapter_; }
  TabDragSessionListener& GetSessionListener() override { return *listener_; }
  DropTargetRegistry& GetDropTargetRegistry() override { return *registry_; }

 private:
  const raw_ref<TabDragSessionInputAdapter> adapter_;
  const raw_ref<TabDragSessionListener> listener_;
  const raw_ref<DropTargetRegistry> registry_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_INPUT_ADAPTER_H_
