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

#include "cobalt/debug/backend/page_agent.h"

#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/dom/document.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/text_node.h"

namespace cobalt {
namespace debug {
namespace backend {

PageAgent::PageAgent(DebugDispatcher* dispatcher, dom::Window* window,
                     std::unique_ptr<RenderLayer> render_layer,
                     render_tree::ResourceProvider* resource_provider)
    : AgentBase("Page", dispatcher),
      window_(window),
      render_layer_(std::move(render_layer)),
      resource_provider_(resource_provider) {
  DCHECK(window_);
  DCHECK(window_->document());
  DCHECK(render_layer_);
  DCHECK(resource_provider_);

  commands_["reload"] = base::Bind(&PageAgent::Reload, base::Unretained(this));
  commands_["getResourceTree"] =
      base::Bind(&PageAgent::GetResourceTree, base::Unretained(this));
  commands_["setOverlayMessage"] =
      base::Bind(&PageAgent::SetOverlayMessage, base::Unretained(this));
}

void PageAgent::Reload(Command command) {
  if (!EnsureEnabled(&command)) return;

  // We don't care about the 'ignoreCache' parameter since navigating creates a
  // new WebModule with a new cache (i.e. cache is always cleared on navigate).
  window_->location()->Reload();
  command.SendResponse();
}

void PageAgent::GetResourceTree(Command command) {
  if (!EnsureEnabled(&command)) return;

  JSONObject response(new base::DictionaryValue());
  JSONObject frame(new base::DictionaryValue());
  frame->SetString("id", "Cobalt");
  frame->SetString("loaderId", "Cobalt");
  frame->SetString("mimeType", "text/html");
  frame->SetString("securityOrigin", window_->document()->url());
  frame->SetString("url", window_->document()->url());
  response->Set("result.frameTree.frame", std::move(frame));
  response->Set("result.frameTree.resources",
                std::make_unique<base::ListValue>());
  command.SendResponse(response);
}

void PageAgent::SetOverlayMessage(Command command) {
  if (!EnsureEnabled(&command)) return;

  std::string message;
  JSONObject params = JSONParse(command.GetParams());
  bool got_message = false;
  if (params) {
    got_message = params->GetString("message", &message);
  }

  if (got_message) {
    const float font_size = 16.0f;
    const float padding = 12.0f;
    render_tree::ColorRGBA bg_color(1.0f, 1.0f, 0.75f, 1.0f);
    render_tree::ColorRGBA text_color(0.0f, 0.0f, 0.0f, 1.0f);

    scoped_refptr<render_tree::Font> font =
        resource_provider_
            ->GetLocalTypeface("monospace", render_tree::FontStyle())
            ->CreateFontWithSize(font_size);
    scoped_refptr<render_tree::GlyphBuffer> glyph_buffer(
        resource_provider_->CreateGlyphBuffer(message, font));
    const math::RectF bounds(glyph_buffer->GetBounds());

    const float bg_width = bounds.width() + 2.0f * padding;
    const float bg_height = bounds.height() + 2.0f * padding;
    const float bg_x = 0.5f * (window_->inner_width() - bg_width);
    const float bg_y = padding;
    const float text_x = bg_x - bounds.x() + padding;
    const float text_y = bg_y - bounds.y() + padding;

    render_tree::CompositionNode::Builder composition_builder;
    composition_builder.AddChild(new render_tree::RectNode(
        math::RectF(bg_x, bg_y, bg_width, bg_height),
        std::unique_ptr<render_tree::Brush>(
            new render_tree::SolidColorBrush(bg_color))));
    composition_builder.AddChild(new render_tree::TextNode(
        math::Vector2dF(text_x, text_y), glyph_buffer, text_color));

    render_layer_->SetFrontLayer(
        new render_tree::CompositionNode(std::move(composition_builder)));
  } else {
    render_layer_->SetFrontLayer(scoped_refptr<render_tree::Node>());
  }

  command.SendResponse();
}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt
