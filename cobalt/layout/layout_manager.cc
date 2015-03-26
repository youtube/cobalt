/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/layout/layout_manager.h"

#include "cobalt/dom/html_body_element.h"
#include "cobalt/layout/layout.h"
#include "cobalt/layout/embedded_resources.h"  // Generated file.

namespace cobalt {
namespace layout {

namespace {

scoped_refptr<cssom::CSSStyleSheet> ParseUserAgentStyleSheet(
    cssom::CSSParser* css_parser) {
  const char* kUserAgentStyleSheetFileName = "user_agent_style_sheet.css";

  // Parse the user agent style sheet from the html.css file that was compiled
  // into a header and included.  We embed it in the binary via C++ header file
  // so that we can avoid the time it would take to perform a disk read
  // when constructing the LayoutManager.  The cost of doing this is that
  // we end up with the contents of the file in memory at all times, even
  // after it is parsed here.
  GeneratedResourceMap resource_map;
  LayoutEmbeddedResources::GenerateMap(resource_map);
  FileContents html_css_file_contents =
      resource_map[kUserAgentStyleSheetFileName];

  return css_parser->ParseStyleSheet(
      std::string(reinterpret_cast<const char*>(html_css_file_contents.data),
                  static_cast<size_t>(html_css_file_contents.size)),
      base::SourceLocation(kUserAgentStyleSheetFileName, 1, 1));
}

}  // namespace

LayoutManager::LayoutManager(
    const scoped_refptr<dom::Window>& window,
    render_tree::ResourceProvider* resource_provider,
    const OnRenderTreeProducedCallback& on_render_tree_produced,
    cssom::CSSParser* css_parser,
    LayoutTrigger layout_trigger)
    : window_(window),
      document_(window->document()),
      viewport_size_(math::SizeF(static_cast<float>(window_->inner_width()),
                                 static_cast<float>(window_->inner_height()))),
      resource_provider_(resource_provider),
      on_render_tree_produced_callback_(on_render_tree_produced),
      user_agent_style_sheet_(ParseUserAgentStyleSheet(css_parser)),
      layout_trigger_(layout_trigger) {
  document_->AddObserver(this);
}

LayoutManager::~LayoutManager() { document_->RemoveObserver(this); }

void LayoutManager::OnLoad() {
  if (layout_trigger_ == kOnDocumentLoad) {
    DoLayoutAndProduceRenderTree();
  }
}

void LayoutManager::OnMutation() {
  if (layout_trigger_ == kOnDocumentMutation) {
    DoLayoutAndProduceRenderTree();
  }
}

void LayoutManager::DoLayoutAndProduceRenderTree() {
  // Chrome lays out and renders the entire document, not just <body>.
  // This enables rendering of <head>, but it's such an obscure feature,
  // it does not make sense to implement it in Cobalt.
  if (document_->body()) {
    on_render_tree_produced_callback_.Run(
        layout::Layout(document_->body(), viewport_size_, resource_provider_,
                       user_agent_style_sheet_));
  }
}

}  // namespace layout
}  // namespace cobalt
