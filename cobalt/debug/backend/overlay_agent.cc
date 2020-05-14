// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/debug/backend/overlay_agent.h"

#include "cobalt/math/clamp.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/rect_node.h"

namespace cobalt {
namespace debug {
namespace backend {

using base::Value;
using math::Clamp;
using render_tree::ColorRGBA;

namespace {
// Returns the float value of a param, or 0.0 if undefined or non-numeric.
float GetFloatParam(const Value* params, base::StringPiece key) {
  if (!params || !params->is_dict()) return 0.0f;
  const Value* v = params->FindKey(key);
  if (!v || !(v->is_double() || v->is_int())) return 0.0f;
  return static_cast<float>(v->GetDouble());
}

// Returns an RGBA color defined by a param, or transparent if undefined.
ColorRGBA RenderColor(const Value* params) {
  float r = GetFloatParam(params, "r") / 255.0f;
  float g = GetFloatParam(params, "g") / 255.0f;
  float b = GetFloatParam(params, "b") / 255.0f;
  float a = GetFloatParam(params, "a");
  return ColorRGBA(Clamp(r, 0.0f, 1.0f), Clamp(g, 0.0f, 1.0f),
                   Clamp(b, 0.0f, 1.0f), Clamp(a, 0.0f, 1.0f));
}

// Returns a rectangle to render according to the params for the DevTools
// "Overlay.highlightRect" command.
// https://chromedevtools.github.io/devtools-protocol/tot/Overlay#method-highlightRect
scoped_refptr<render_tree::RectNode> RenderHighlightRect(const Value* params) {
  float x = GetFloatParam(params, "x");
  float y = GetFloatParam(params, "y");
  float width = GetFloatParam(params, "width");
  float height = GetFloatParam(params, "height");
  ColorRGBA color(RenderColor(params->FindKey("color")));
  const Value* outline_param = params->FindKey("outlineColor");
  ColorRGBA outline_color(RenderColor(outline_param));
  float outline_width = outline_param ? 1.0f : 0.0f;
  return base::MakeRefCounted<render_tree::RectNode>(
      math::RectF(x, y, width, height),
      std::make_unique<render_tree::SolidColorBrush>(color),
      std::make_unique<render_tree::Border>(render_tree::BorderSide(
          outline_width, render_tree::kBorderStyleSolid, outline_color)));
}

}  // namespace

OverlayAgent::OverlayAgent(DebugDispatcher* dispatcher,
                           std::unique_ptr<RenderLayer> render_layer)
    : AgentBase("Overlay", "overlay_agent.js", dispatcher),
      render_layer_(std::move(render_layer)) {
  DCHECK(render_layer_);

  commands_["highlightNode"] =
      base::Bind(&OverlayAgent::HighlightNode, base::Unretained(this));
  commands_["highlightRect"] =
      base::Bind(&OverlayAgent::HighlightRect, base::Unretained(this));
  commands_["hideHighlight"] =
      base::Bind(&OverlayAgent::HideHighlight, base::Unretained(this));
  commands_["setShowViewportSizeOnResize"] = base::Bind(
      &OverlayAgent::SetShowViewportSizeOnResize, base::Unretained(this));
}

void OverlayAgent::HighlightNode(Command command) {
  if (!EnsureEnabled(&command)) return;

  // Use the injected JavaScript helper to get the rectangles to highlight for
  // the specified node.
  JSONObject rects_response = dispatcher_->RunScriptCommand(
      "Overlay._highlightNodeRects", command.GetParams());
  const Value* highlight_rects =
      rects_response->FindPath({"result", "highlightRects"});
  if (!highlight_rects) {
    command.SendErrorResponse(Command::kInvalidParams,
                              "Can't get node highlights.");
    return;
  }

  // Render all the highlight rects as children of a CompositionNode.
  render_tree::CompositionNode::Builder builder;
  for (const Value& rect_params : highlight_rects->GetList()) {
    builder.AddChild(RenderHighlightRect(&rect_params));
  }
  render_layer_->SetFrontLayer(
      base::MakeRefCounted<render_tree::CompositionNode>(builder));

  command.SendResponse();
}

void OverlayAgent::HighlightRect(Command command) {
  if (!EnsureEnabled(&command)) return;

  JSONObject params = JSONParse(command.GetParams());
  render_layer_->SetFrontLayer(RenderHighlightRect(params.get()));
  command.SendResponse();
}

void OverlayAgent::HideHighlight(Command command) {
  if (!EnsureEnabled(&command)) return;

  render_layer_->SetFrontLayer(scoped_refptr<render_tree::Node>());
  command.SendResponse();
}

void OverlayAgent::SetShowViewportSizeOnResize(Command command) {
  // NO-OP implementation needed to prevent a frontend runtime exception when
  // Overlay settings are restored after the Performance panel stops tracing.
  command.SendResponse();
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
