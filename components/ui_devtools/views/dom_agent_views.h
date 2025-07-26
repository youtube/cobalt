// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_VIEWS_H_
#define COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_VIEWS_H_

#include "components/ui_devtools/dom.h"
#include "components/ui_devtools/dom_agent.h"

namespace ui_devtools {

class DOMAgentViews : public DOMAgent {
 public:
  DOMAgentViews(const DOMAgentViews&) = delete;
  DOMAgentViews& operator=(const DOMAgentViews&) = delete;

  ~DOMAgentViews() override;
  static std::unique_ptr<DOMAgentViews> Create();

 protected:
  DOMAgentViews();

  virtual std::unique_ptr<protocol::DOM::Node> BuildTreeForWindow(
      UIElement* window_element_root) = 0;
  std::unique_ptr<protocol::DOM::Node> BuildTreeForRootWidget(
      UIElement* widget_element);
  std::unique_ptr<protocol::DOM::Node> BuildTreeForView(
      UIElement* view_element);

  std::unique_ptr<protocol::DOM::Node> BuildTreeForUIElement(
      UIElement* ui_element) override;
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIEWS_DOM_AGENT_VIEWS_H_
