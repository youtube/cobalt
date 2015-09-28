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

#include <string>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/timer.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/layout/benchmark_stat_names.h"
#include "cobalt/layout/embedded_resources.h"  // Generated file.
#include "cobalt/layout/initial_containing_block.h"
#include "cobalt/layout/layout.h"
#include "third_party/icu/public/common/unicode/brkiter.h"

namespace cobalt {
namespace layout {

class LayoutManager::Impl : public dom::DocumentObserver {
 public:
  Impl(const scoped_refptr<dom::Window>& window,
       render_tree::ResourceProvider* resource_provider,
       const OnRenderTreeProducedCallback& on_render_tree_produced,
       cssom::CSSParser* css_parser, LayoutTrigger layout_trigger,
       float layout_refresh_rate);
  ~Impl();

  // From dom::DocumentObserver.
  void OnLoad() OVERRIDE;
  void OnMutation() OVERRIDE;

 private:
  void DoLayoutAndProduceRenderTree();

#if defined(ENABLE_TEST_RUNNER)
  void NotifyDone();
#endif  // ENABLE_TEST_RUNNER

  const scoped_refptr<dom::Window> window_;
  const scoped_refptr<dom::Document> document_;
  render_tree::ResourceProvider* const resource_provider_;
  const OnRenderTreeProducedCallback on_render_tree_produced_callback_;
  const scoped_refptr<cssom::CSSStyleSheet> user_agent_style_sheet_;
  const LayoutTrigger layout_trigger_;

  // This flag indicates whether or not we should do a re-layout.  The flag
  // is checked at a regular interval (e.g. 60Hz) and if it is set to true,
  // a layout is initiated and it is set back to false.  Events such as
  // DOM mutations will set this flag back to true.
  bool layout_dirty_;

  // Construction of |BreakIterator| requires a disk read, so we cache it
  // in the layout manager in order to reuse it with all layouts happening
  // in the context of one |WebModule|.
  //   http://userguide.icu-project.org/boundaryanalysis#TOC-Reuse
  scoped_ptr<icu::BreakIterator> line_break_iterator_;

  base::Timer layout_timer_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

namespace {

scoped_refptr<cssom::CSSStyleSheet> ParseUserAgentStyleSheet(
    cssom::CSSParser* css_parser) {
  const char kUserAgentStyleSheetFileName[] = "user_agent_style_sheet.css";

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

LayoutManager::Impl::Impl(
    const scoped_refptr<dom::Window>& window,
    render_tree::ResourceProvider* resource_provider,
    const OnRenderTreeProducedCallback& on_render_tree_produced,
    cssom::CSSParser* css_parser, LayoutTrigger layout_trigger,
    float layout_refresh_rate)
    : window_(window),
      document_(window->document()),
      resource_provider_(resource_provider),
      on_render_tree_produced_callback_(on_render_tree_produced),
      user_agent_style_sheet_(ParseUserAgentStyleSheet(css_parser)),
      layout_trigger_(layout_trigger),
      layout_dirty_(false),
      layout_timer_(true, true) {
  document_->AddObserver(this);

  UErrorCode status = U_ZERO_ERROR;
  line_break_iterator_ = make_scoped_ptr(icu::BreakIterator::createLineInstance(
      icu::Locale::getDefault(), status));
  DCHECK(U_SUCCESS(status));

#if defined(ENABLE_TEST_RUNNER)
  if (layout_trigger_ == kTestRunnerMode) {
    window_->test_runner()->set_notify_done_callback(
        base::Bind(&LayoutManager::Impl::NotifyDone, base::Unretained(this)));
  } else {
#else
  {
#endif  // ENABLE_TEST_RUNNER

    // TODO(***REMOVED***): Eventually we would like to instead base our layouts off of
    //               a "refresh" signal generated by the rasterizer, instead of
    //               trying to match timers to the graphics' refresh rate, which
    //               is error prone.
    //               b/20423772
    const int64_t timer_interval_in_mircoseconds =
        static_cast<int64_t>(base::Time::kMicrosecondsPerSecond * 1.0f /
                             (layout_refresh_rate + 1.0f));

    layout_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMicroseconds(timer_interval_in_mircoseconds),
        base::Bind(&LayoutManager::Impl::DoLayoutAndProduceRenderTree,
                   base::Unretained(this)));
  }
}

LayoutManager::Impl::~Impl() { document_->RemoveObserver(this); }

void LayoutManager::Impl::OnLoad() {
#if defined(ENABLE_TEST_RUNNER)
  if (layout_trigger_ == kTestRunnerMode &&
      !window_->test_runner()->should_wait()) {
    layout_dirty_ = true;
    DoLayoutAndProduceRenderTree();
  }
#endif  // ENABLE_TEST_RUNNER
}

void LayoutManager::Impl::OnMutation() {
  if (layout_trigger_ == kOnDocumentMutation) {
    layout_dirty_ = true;
  }
}

#if defined(ENABLE_TEST_RUNNER)
void LayoutManager::Impl::NotifyDone() {
  DCHECK_EQ(kTestRunnerMode, layout_trigger_);
  layout_dirty_ = true;
  DoLayoutAndProduceRenderTree();
}
#endif  // ENABLE_TEST_RUNNER

void LayoutManager::Impl::DoLayoutAndProduceRenderTree() {
  TRACE_EVENT0("cobalt::layout",
               "LayoutManager::Impl::DoLayoutAndProduceRenderTree()");

  if (!document_->html()) {
    return;
  }

  // Update the document's sample time, used for updating animations.
  document_->SampleTimelineTime();

  bool was_dirty = layout_dirty_;
  if (layout_dirty_) {
    TRACE_EVENT_BEGIN0("cobalt::layout", kBenchmarkStatLayout);
    // Update our computed style before running animation callbacks, so that
    // any transitioning elements adjusted during the animation callback will
    // transition from their previously set value.
    document_->UpdateComputedStyles(
        CreateInitialContainingBlockComputedStyle(window_),
        user_agent_style_sheet_);
  }

  // Note that according to:
  //     http://www.w3.org/TR/web-animations/#model-liveness,
  // "The time passed to a requestAnimationFrame callback will be equal to
  // document.timeline.currentTime".  In our case, document.timeline.currentTime
  // is derived from the latest sample time.
  window_->RunAnimationFrameCallbacks();

  if (layout_dirty_) {
    if (!was_dirty) {
      // We want to catch the beginning of all layout processing.  If we weren't
      // dirty before the call to RunAnimationFrameCallbacks(), then the flow
      // starts here instead of there.
      TRACE_EVENT_BEGIN0("cobalt::layout", kBenchmarkStatLayout);
    }

    RenderTreeWithAnimations render_tree_with_animations =
        layout::Layout(window_, user_agent_style_sheet_, resource_provider_,
                       line_break_iterator_.get());
    on_render_tree_produced_callback_.Run(
        render_tree_with_animations.render_tree,
        render_tree_with_animations.animations,
        *document_->timeline_sample_time());

    layout_dirty_ = false;

    TRACE_EVENT_END0("cobalt::layout", kBenchmarkStatLayout);
  }
}

LayoutManager::LayoutManager(
    const scoped_refptr<dom::Window>& window,
    render_tree::ResourceProvider* resource_provider,
    const OnRenderTreeProducedCallback& on_render_tree_produced,
    cssom::CSSParser* css_parser, LayoutTrigger layout_trigger,
    float layout_refresh_rate)
    : impl_(new Impl(window, resource_provider, on_render_tree_produced,
                     css_parser, layout_trigger, layout_refresh_rate)) {}

LayoutManager::~LayoutManager() {}

}  // namespace layout
}  // namespace cobalt
