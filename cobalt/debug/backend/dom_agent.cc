// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/debug/backend/dom_agent.h"

#include <string>

#include "base/bind.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/rect_node.h"

namespace cobalt {
namespace debug {
namespace backend {

namespace {
// File to load JavaScript DOM debugging domain implementation from.
const char kScriptFile[] = "dom.js";

// Parameter names:
const char kA[] = "a";
const char kB[] = "b";
const char kContentColor[] = "contentColor";
const char kG[] = "g";
const char kHighlightConfig[] = "highlightConfig";
const char kR[] = "r";
}  // namespace

DOMAgent::DOMAgent(DebugDispatcher* dispatcher,
                   scoped_ptr<RenderLayer> render_layer)
    : dispatcher_(dispatcher),
      render_layer_(render_layer.Pass()),
      ALLOW_THIS_IN_INITIALIZER_LIST(commands_(this)) {
  DCHECK(dispatcher_);

  commands_["DOM.disable"] = &DOMAgent::Disable;
  commands_["DOM.enable"] = &DOMAgent::Enable;
  commands_["DOM.highlightNode"] = &DOMAgent::HighlightNode;
  commands_["DOM.hideHighlight"] = &DOMAgent::HideHighlight;

  dispatcher_->AddDomain("DOM", commands_.Bind());
}

void DOMAgent::Enable(const Command& command) {
  bool initialized = dispatcher_->RunScriptFile(kScriptFile);
  if (initialized) {
    command.SendResponse();
  } else {
    command.SendErrorResponse(Command::kInternalError,
                              "Cannot create DOM inspector.");
  }
}

void DOMAgent::Disable(const Command& command) { command.SendResponse(); }

// Unlike most other DOM command handlers, this one is not fully implemented
// in JavaScript. Instead, the JS object is used to look up the node from the
// parameters and return its bounding client rect, then the highlight itself
// is rendered by calling the C++ function |RenderHighlight| to set the render
// overlay.
void DOMAgent::HighlightNode(const Command& command) {
  // Get the bounding rectangle of the specified node.
  JSONObject json_dom_rect = dispatcher_->RunScriptCommand(
      "dom.getBoundingClientRect", command.GetParams());
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
  json_dom_rect->GetDouble("result.x", &x);
  json_dom_rect->GetDouble("result.y", &y);
  json_dom_rect->GetDouble("result.width", &width);
  json_dom_rect->GetDouble("result.height", &height);

  scoped_refptr<dom::DOMRect> dom_rect(
      new dom::DOMRect(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(width), static_cast<float>(height)));

  // |highlight_config_value| still owned by |params|.
  JSONObject params = JSONParse(command.GetParams());
  base::DictionaryValue* highlight_config_value = NULL;
  bool got_highlight_config =
      params->GetDictionary(kHighlightConfig, &highlight_config_value);
  DCHECK(got_highlight_config);
  DCHECK(highlight_config_value);

  RenderHighlight(dom_rect, highlight_config_value);

  command.SendResponse();
}

void DOMAgent::HideHighlight(const Command& command) {
  render_layer_->SetFrontLayer(scoped_refptr<render_tree::Node>());
  command.SendResponse();
}

void DOMAgent::RenderHighlight(
    const scoped_refptr<dom::DOMRect>& bounding_rect,
    const base::DictionaryValue* highlight_config_value) {
  // TODO: Should also render borders, etc.

  // Content color is optional in the parameters, so use a fallback.
  int r = 112;
  int g = 168;
  int b = 219;
  double a = 0.66;
  const base::DictionaryValue* content_color = NULL;
  bool got_content_color =
      highlight_config_value->GetDictionary(kContentColor, &content_color);
  if (got_content_color && content_color) {
    content_color->GetInteger(kR, &r);
    content_color->GetInteger(kG, &g);
    content_color->GetInteger(kB, &b);
    content_color->GetDouble(kA, &a);
  }
  render_tree::ColorRGBA color(r / 255.0f, g / 255.0f, b / 255.0f,
                               static_cast<float>(a));

  scoped_ptr<render_tree::Brush> background_brush(
      new render_tree::SolidColorBrush(color));
  scoped_refptr<render_tree::Node> rect = new render_tree::RectNode(
      math::RectF(bounding_rect->x(), bounding_rect->y(),
                  bounding_rect->width(), bounding_rect->height()),
      background_brush.Pass());
  render_layer_->SetFrontLayer(rect);
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
