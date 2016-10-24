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
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "cobalt/cssom/cascade_precedence.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/layout/benchmark_stat_names.h"
#include "cobalt/layout/block_formatting_block_container_box.h"
#include "cobalt/layout/initial_containing_block.h"
#include "cobalt/layout/layout.h"
#include "third_party/icu/public/common/unicode/brkiter.h"
#include "third_party/icu/public/common/unicode/locid.h"

namespace cobalt {
namespace layout {

class LayoutManager::Impl : public dom::DocumentObserver {
 public:
  Impl(const scoped_refptr<dom::Window>& window,
       const OnRenderTreeProducedCallback& on_render_tree_produced,
       LayoutTrigger layout_trigger, int dom_max_element_depth,
       float layout_refresh_rate, const std::string& language,
       LayoutStatTracker* layout_stat_tracker);
  ~Impl();

  // From dom::DocumentObserver.
  void OnLoad() OVERRIDE;
  void OnMutation() OVERRIDE;

  // Called to perform a synchronous layout.
  void DoSynchronousLayout();

  void Suspend();
  void Resume();

  void SetLayoutDirty();

 private:
  void StartLayoutTimer();
  void DoLayoutAndProduceRenderTree();

#if defined(ENABLE_TEST_RUNNER)
  void DoTestRunnerLayoutCallback();
#endif  // ENABLE_TEST_RUNNER

  const scoped_refptr<dom::Window> window_;
  const icu::Locale locale_;
  const scoped_ptr<UsedStyleProvider> used_style_provider_;
  const OnRenderTreeProducedCallback on_render_tree_produced_callback_;
  const LayoutTrigger layout_trigger_;

  // This flag indicates whether or not we should do a re-layout.  The flag
  // is checked at a regular interval (e.g. 60Hz) and if it is set to true,
  // a layout is initiated and it is set back to false.  Events such as
  // DOM mutations will set this flag back to true.
  bool layout_dirty_;

  // Construction of |BreakIterator| requires a disk read, so we cache them
  // in the layout manager in order to reuse them with all layouts happening
  // in the context of one |WebModule|.
  //   http://userguide.icu-project.org/boundaryanalysis#TOC-Reuse
  scoped_ptr<icu::BreakIterator> line_break_iterator_;
  scoped_ptr<icu::BreakIterator> character_break_iterator_;

  base::Timer layout_timer_;
  int dom_max_element_depth_;
  float layout_refresh_rate_;

  LayoutStatTracker* const layout_stat_tracker_;

  // The initial containing block is kept until the next layout, so that
  // the box tree remains valid.
  scoped_refptr<BlockLevelBlockContainerBox> initial_containing_block_;

  bool suspended_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

LayoutManager::Impl::Impl(
    const scoped_refptr<dom::Window>& window,
    const OnRenderTreeProducedCallback& on_render_tree_produced,
    LayoutTrigger layout_trigger, int dom_max_element_depth,
    float layout_refresh_rate, const std::string& language,
    LayoutStatTracker* layout_stat_tracker)
    : window_(window),
      locale_(icu::Locale::createCanonical(language.c_str())),
      used_style_provider_(
          new UsedStyleProvider(window->html_element_context()->image_cache(),
                                window->document()->font_cache())),
      on_render_tree_produced_callback_(on_render_tree_produced),
      layout_trigger_(layout_trigger),
      layout_dirty_(true),
      layout_timer_(true, true, true),
      dom_max_element_depth_(dom_max_element_depth),
      layout_refresh_rate_(layout_refresh_rate),
      layout_stat_tracker_(layout_stat_tracker),
      suspended_(false) {
  window_->document()->AddObserver(this);
  window_->SetSynchronousLayoutCallback(
      base::Bind(&Impl::DoSynchronousLayout, base::Unretained(this)));

  UErrorCode status = U_ZERO_ERROR;
  line_break_iterator_ =
      make_scoped_ptr(icu::BreakIterator::createLineInstance(locale_, status));
  CHECK(U_SUCCESS(status));
  status = U_ZERO_ERROR;
  character_break_iterator_ = make_scoped_ptr(
      icu::BreakIterator::createCharacterInstance(locale_, status));
  CHECK(U_SUCCESS(status));

#if defined(ENABLE_TEST_RUNNER)
  if (layout_trigger_ == kTestRunnerMode) {
    window_->test_runner()->set_trigger_layout_callback(
        base::Bind(&LayoutManager::Impl::DoTestRunnerLayoutCallback,
                   base::Unretained(this)));
  }
#endif  // ENABLE_TEST_RUNNER
}

LayoutManager::Impl::~Impl() { window_->document()->RemoveObserver(this); }

void LayoutManager::Impl::OnLoad() {
#if defined(ENABLE_TEST_RUNNER)
  if (layout_trigger_ != kTestRunnerMode) {
#else
  {
#endif
    // Start the layout timer.  If the TestRunner is active, then we do not
    // start a timer as the TestRunner will drive the triggering of layouts.
    StartLayoutTimer();
  }

#if defined(ENABLE_TEST_RUNNER)
  if (layout_trigger_ == kTestRunnerMode &&
      !window_->test_runner()->should_wait()) {
    SetLayoutDirty();

    // Run the |DoLayoutAndProduceRenderTree| task after onload event finished.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&LayoutManager::Impl::DoLayoutAndProduceRenderTree,
                   base::Unretained(this)));
  }
#endif  // ENABLE_TEST_RUNNER
}

void LayoutManager::Impl::OnMutation() {
  if (layout_trigger_ == kOnDocumentMutation) {
    SetLayoutDirty();
  }
}

void LayoutManager::Impl::DoSynchronousLayout() {
  TRACE_EVENT0("cobalt::layout", "LayoutManager::Impl::DoSynchronousLayout()");
  if (suspended_) {
    return;
  }

  layout::UpdateComputedStylesAndLayoutBoxTree(
      locale_, window_->document(), dom_max_element_depth_,
      used_style_provider_.get(), layout_stat_tracker_,
      line_break_iterator_.get(), character_break_iterator_.get(),
      &initial_containing_block_);
}

void LayoutManager::Impl::Suspend() {
  // Mark that we are suspended so that we don't try to perform any layouts.
  suspended_ = true;

  // Clear our reference to the initial containing block to allow any resources
  // like images that were referenced by it to be released.
  initial_containing_block_ = NULL;

  // Invalidate the document's layout so that all references to any resources
  // such as images will be released.
  window_->document()->InvalidateLayout();
}

void LayoutManager::Impl::Resume() {
  // Mark that we are no longer suspended and indicate that the layout is
  // dirty since when Suspend() was called we invalidated our previous layout.
  SetLayoutDirty();
  suspended_ = false;
}

void LayoutManager::Impl::SetLayoutDirty() {
  layout_dirty_ = true;
  layout_stat_tracker_->OnLayoutDirty();
}

#if defined(ENABLE_TEST_RUNNER)
void LayoutManager::Impl::DoTestRunnerLayoutCallback() {
  DCHECK_EQ(kTestRunnerMode, layout_trigger_);
  SetLayoutDirty();

  if (layout_trigger_ == kTestRunnerMode &&
      window_->test_runner()->should_wait()) {
    TRACE_EVENT_BEGIN0("cobalt::layout", kBenchmarkStatNonMeasuredLayout);
  }

  DoLayoutAndProduceRenderTree();

  if (layout_trigger_ == kTestRunnerMode &&
      window_->test_runner()->should_wait()) {
    TRACE_EVENT_END0("cobalt::layout", kBenchmarkStatNonMeasuredLayout);
  }
}
#endif  // ENABLE_TEST_RUNNER

void LayoutManager::Impl::StartLayoutTimer() {
  // TODO: Eventually we would like to instead base our layouts off of a
  //       "refresh" signal generated by the rasterizer, instead of trying to
  //       match timers to the graphics' refresh rate, which is error prone.
  const int64_t timer_interval_in_mircoseconds =
      static_cast<int64_t>(base::Time::kMicrosecondsPerSecond * 1.0f /
                           (layout_refresh_rate_ + 1.0f));

  layout_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMicroseconds(timer_interval_in_mircoseconds),
      base::Bind(&LayoutManager::Impl::DoLayoutAndProduceRenderTree,
                 base::Unretained(this)));
}

void LayoutManager::Impl::DoLayoutAndProduceRenderTree() {
  TRACE_EVENT0("cobalt::layout",
               "LayoutManager::Impl::DoLayoutAndProduceRenderTree()");

  if (suspended_) return;

  const scoped_refptr<dom::Document>& document = window_->document();

  if (!document->html()) {
    return;
  }

  // Update the document's sample time, used for updating animations.
  document->SampleTimelineTime();

  bool was_dirty = layout_dirty_;
  if (layout_dirty_) {
    TRACE_EVENT_BEGIN0("cobalt::layout", kBenchmarkStatLayout);
    // Update our computed style before running animation callbacks, so that
    // any transitioning elements adjusted during the animation callback will
    // transition from their previously set value.
    document->UpdateComputedStyles();
  }

  // Note that according to:
  //     https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#model-liveness,
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

    scoped_refptr<render_tree::Node> render_tree_root = layout::Layout(
        locale_, window_->document(), dom_max_element_depth_,
        used_style_provider_.get(), layout_stat_tracker_,
        line_break_iterator_.get(), character_break_iterator_.get(),
        &initial_containing_block_);
    bool run_on_render_tree_produced_callback = true;
#if defined(ENABLE_TEST_RUNNER)
    if (layout_trigger_ == kTestRunnerMode &&
        window_->test_runner()->should_wait()) {
      run_on_render_tree_produced_callback = false;
    }
#endif  // ENABLE_TEST_RUNNER
    if (run_on_render_tree_produced_callback) {
      on_render_tree_produced_callback_.Run(LayoutResults(
          render_tree_root, base::TimeDelta::FromMillisecondsD(
                                *document->timeline()->current_time())));
    }

    layout_dirty_ = false;
    layout_stat_tracker_->OnLayoutClean();

    TRACE_EVENT_END0("cobalt::layout", kBenchmarkStatLayout);
  }
}

LayoutManager::LayoutManager(
    const scoped_refptr<dom::Window>& window,
    const OnRenderTreeProducedCallback& on_render_tree_produced,
    LayoutTrigger layout_trigger, const int dom_max_element_depth,
    const float layout_refresh_rate, const std::string& language,
    LayoutStatTracker* layout_stat_tracker)
    : impl_(new Impl(window, on_render_tree_produced, layout_trigger,
                     dom_max_element_depth, layout_refresh_rate, language,
                     layout_stat_tracker)) {}

LayoutManager::~LayoutManager() {}

void LayoutManager::Suspend() { impl_->Suspend(); }
void LayoutManager::Resume() { impl_->Resume(); }

}  // namespace layout
}  // namespace cobalt
