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

#include "cobalt/browser/html/html_body_element.h"
#include "cobalt/layout/layout.h"

namespace cobalt {
namespace layout {

LayoutManager::LayoutManager(
    const scoped_refptr<browser::Document>& document,
    const math::SizeF& viewport_size,
    render_tree::ResourceProvider* resource_provider,
    const OnRenderTreeProducedCallback& on_render_tree_produced)
    : document_(document),
      viewport_size_(viewport_size),
      resource_provider_(resource_provider),
      on_render_tree_produced_callback_(on_render_tree_produced) {
  document_->AddObserver(this);
}

LayoutManager::~LayoutManager() { document_->RemoveObserver(this); }

void LayoutManager::OnLoad() {}

void LayoutManager::OnMutation() {
  // Chrome lays out and renders entire document, not just body.
  // It doesn't make sense for Cobalt.
  if (document_->body()) {
    on_render_tree_produced_callback_.Run(
        layout::Layout(document_->body(), viewport_size_, resource_provider_));
  }
}

}  // namespace layout
}  // namespace cobalt
