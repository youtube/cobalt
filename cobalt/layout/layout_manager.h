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

#ifndef LAYOUT_LAYOUT_MANAGER_H_
#define LAYOUT_LAYOUT_MANAGER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/timer.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/document_builder.h"
#include "cobalt/dom/window.h"
#include "cobalt/loader/image_cache.h"
#include "cobalt/render_tree/animations/node_animations_map.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace layout {

// Produces the render tree each time when the document needs layout update.
class LayoutManager : public dom::DocumentObserver {
 public:
  typedef base::Callback<
      void(const scoped_refptr<render_tree::Node>&,
           const scoped_refptr<render_tree::animations::NodeAnimationsMap>&)>
      OnRenderTreeProducedCallback;

  // Specifies what event should trigger a layout, and hence what event
  // will result in a render tree being produced and passed in a call to
  // on_render_tree_produced_callback_.
  enum LayoutTrigger {
    kOnDocumentMutation,
    kOnDocumentLoad,
    kNoTrigger,
  };

  LayoutManager(const scoped_refptr<dom::Window>& window,
                render_tree::ResourceProvider* resource_provider,
                const OnRenderTreeProducedCallback& on_render_tree_produced,
                cssom::CSSParser* css_parser,
                loader::FetcherFactory* fetcher_factory,
                LayoutTrigger layout_trigger, float layout_refresh_rate);
  ~LayoutManager();

  // From dom::DocumentObserver.
  void OnLoad() OVERRIDE;
  void OnMutation() OVERRIDE;

  void NotifyImageLoaded();

 private:
  void DoLayoutAndProduceRenderTree();

  const scoped_refptr<dom::Window> window_;
  const scoped_refptr<dom::Document> document_;
  const math::SizeF viewport_size_;
  render_tree::ResourceProvider* const resource_provider_;
  const OnRenderTreeProducedCallback on_render_tree_produced_callback_;
  const scoped_refptr<cssom::CSSStyleSheet> user_agent_style_sheet_;
  const LayoutTrigger layout_trigger_;
  scoped_ptr<loader::ImageCache> image_cache_;

  // This flag indicates whether or not we should do a re-layout.  The flag
  // is checked at a regular interval (e.g. 60Hz) and if it is set to true,
  // a layout is initiated and it is set back to false.  Events such as
  // DOM mutations will set this flag back to true.
  bool layout_dirty_;

  base::Timer layout_timer_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManager);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_LAYOUT_MANAGER_H_
