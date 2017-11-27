// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/layout/layout_manager.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "cobalt/cssom/cascade_precedence.h"
#include "cobalt/dom/camera_3d.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/layout/benchmark_stat_names.h"
#include "cobalt/layout/block_formatting_block_container_box.h"
#include "cobalt/layout/initial_containing_block.h"
#include "cobalt/layout/layout.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/render_tree/matrix_transform_3d_node.h"
#include "third_party/icu/source/common/unicode/brkiter.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace cobalt {
namespace layout {

class LayoutManager::Impl : public dom::DocumentObserver {
 public:
  Impl(const std::string& name, const scoped_refptr<dom::Window>& window,
       const OnRenderTreeProducedCallback& on_render_tree_produced,
       const OnLayoutCallback& on_layout, LayoutTrigger layout_trigger,
       int dom_max_element_depth, float layout_refresh_rate,
       const std::string& language, bool enable_image_animations,
       LayoutStatTracker* layout_stat_tracker);
  ~Impl();

  // From dom::DocumentObserver.
  void OnLoad() override;
  void OnMutation() override;
  void OnFocusChanged() override {}

  // Called to perform a synchronous layout.
  void DoSynchronousLayout();

  void Suspend();
  void Resume();

  bool IsRenderTreePending() const;

 private:
  void DirtyLayout();
  void StartLayoutTimer();
  void DoLayoutAndProduceRenderTree();

#if defined(ENABLE_TEST_RUNNER)
  void DoTestRunnerLayoutCallback();
#endif  // ENABLE_TEST_RUNNER

  const scoped_refptr<dom::Window> window_;
  const icu::Locale locale_;
  const scoped_ptr<UsedStyleProvider> used_style_provider_;
  const OnRenderTreeProducedCallback on_render_tree_produced_callback_;
  const OnLayoutCallback on_layout_callback_;
  const LayoutTrigger layout_trigger_;

  bool produced_render_tree_;

  // Setting these flags triggers an update of the layout box tree and the
  // generation of a new render tree at a regular interval (e.g. 60Hz). Events
  // such as DOM mutations cause them to be set to true. While the render tree
  // is excusively produced at the regular interval, the box tree can also be
  // updated via a call to DoSynchronousLayout().
  bool are_computed_styles_and_box_tree_dirty_;
  base::CVal<bool> is_render_tree_pending_;

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

namespace {

void UpdateCamera(
    float width_to_height_aspect_ratio, scoped_refptr<input::Camera3D> camera,
    float max_horizontal_fov_rad, float max_vertical_fov_rad,
    render_tree::MatrixTransform3DNode::Builder* transform_node_builder) {
  float vertical_fov_rad =
      std::min(max_vertical_fov_rad,
               2 * static_cast<float>(atan(tan(max_horizontal_fov_rad * 0.5f) /
                                           width_to_height_aspect_ratio)));
  camera->UpdatePerspective(width_to_height_aspect_ratio, vertical_fov_rad);
  base::CameraTransform transform(
      camera->GetCameraTransformAndUpdateOrientation());
  DCHECK(!transform.right_eye);
  transform_node_builder->transform =
      transform.left_eye_or_mono.projection_matrix *
      transform.left_eye_or_mono.view_matrix;
}

scoped_refptr<render_tree::Node> AttachCameraNodes(
    const scoped_refptr<dom::Window> window,
    const scoped_refptr<render_tree::Node>& source,
    float max_horizontal_fov_rad, float max_vertical_fov_rad) {
  // Attach a 3D transform node that applies the current camera matrix transform
  // to the rest of the render tree.
  scoped_refptr<render_tree::MatrixTransform3DNode> transform_node =
      new render_tree::MatrixTransform3DNode(
          source, glm::mat4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1));

  // We setup an animation on the camera transform node such that the camera
  // is driven by the renderer thread and can bypass layout entirely.
  render_tree::animations::AnimateNode::Builder animate_node_builder;
  animate_node_builder.Add(
      transform_node,
      base::Bind(&UpdateCamera, window->inner_width() / window->inner_height(),
                 window->camera_3d()->impl(), max_horizontal_fov_rad,
                 max_vertical_fov_rad));
  return new render_tree::animations::AnimateNode(animate_node_builder,
                                                  transform_node);
}

}  // namespace

LayoutManager::Impl::Impl(
    const std::string& name, const scoped_refptr<dom::Window>& window,
    const OnRenderTreeProducedCallback& on_render_tree_produced,
    const OnLayoutCallback& on_layout, LayoutTrigger layout_trigger,
    int dom_max_element_depth, float layout_refresh_rate,
    const std::string& language, bool enable_image_animations,
    LayoutStatTracker* layout_stat_tracker)
    : window_(window),
      locale_(icu::Locale::createCanonical(language.c_str())),
      used_style_provider_(new UsedStyleProvider(
          window->html_element_context(), window->document()->font_cache(),
          base::Bind(&AttachCameraNodes, window), enable_image_animations)),
      on_render_tree_produced_callback_(on_render_tree_produced),
      on_layout_callback_(on_layout),
      layout_trigger_(layout_trigger),
      produced_render_tree_(false),
      are_computed_styles_and_box_tree_dirty_(true),
      is_render_tree_pending_(
          StringPrintf("%s.Layout.IsRenderTreePending", name.c_str()), true,
          "Non-zero when a new render tree is pending."),
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
    DirtyLayout();

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
    DirtyLayout();
  }
}

void LayoutManager::Impl::DoSynchronousLayout() {
  TRACE_EVENT0("cobalt::layout", "LayoutManager::Impl::DoSynchronousLayout()");
  if (suspended_) {
    return;
  }

  if (are_computed_styles_and_box_tree_dirty_) {
    layout::UpdateComputedStylesAndLayoutBoxTree(
        locale_, window_->document(), dom_max_element_depth_,
        used_style_provider_.get(), layout_stat_tracker_,
        line_break_iterator_.get(), character_break_iterator_.get(),
        &initial_containing_block_);
    are_computed_styles_and_box_tree_dirty_ = false;
  }
}

void LayoutManager::Impl::Suspend() {
  // Mark that we are suspended so that we don't try to perform any layouts.
  suspended_ = true;

  // Invalidate any cached layout boxes from the document prior to clearing
  // the initial containing block. That'll ensure that the full box tree is
  // destroyed when the containing block is destroyed and that no children of
  // |initial_containing_block_| are holding on to stale parent pointers.
  window_->document()->InvalidateLayoutBoxes();

  // Clear our reference to the initial containing block to allow any resources
  // like images that were referenced by it to be released.
  initial_containing_block_ = NULL;
}

void LayoutManager::Impl::Resume() {
  // Mark that we are no longer suspended and indicate that the layout is
  // dirty since when Suspend() was called we invalidated our previous layout.
  DirtyLayout();
  suspended_ = false;
}

bool LayoutManager::Impl::IsRenderTreePending() const {
  return is_render_tree_pending_;
}

#if defined(ENABLE_TEST_RUNNER)
void LayoutManager::Impl::DoTestRunnerLayoutCallback() {
  DCHECK_EQ(kTestRunnerMode, layout_trigger_);
  DirtyLayout();

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

void LayoutManager::Impl::DirtyLayout() {
  are_computed_styles_and_box_tree_dirty_ = true;
  is_render_tree_pending_ = true;
}

void LayoutManager::Impl::StartLayoutTimer() {
  // TODO: Eventually we would like to instead base our layouts off of a
  //       "refresh" signal generated by the rasterizer, instead of trying to
  //       match timers to the graphics' refresh rate, which is error prone.
  const int64_t timer_interval_in_microseconds =
      static_cast<int64_t>(base::Time::kMicrosecondsPerSecond * 1.0f /
                           (layout_refresh_rate_ + 1.0f));

  layout_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMicroseconds(timer_interval_in_microseconds),
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

  bool has_layout_processing_started = false;
  if (window_->HasPendingAnimationFrameCallbacks()) {
    if (are_computed_styles_and_box_tree_dirty_) {
      has_layout_processing_started = true;
      TRACE_EVENT_BEGIN0("cobalt::layout", kBenchmarkStatLayout);
      // Update our computed style before running animation callbacks, so that
      // any transitioning elements adjusted during the animation callback will
      // transition from their previously set value.
      document->UpdateComputedStyles();
    }

    // Note that according to:
    //     https://www.w3.org/TR/2015/WD-web-animations-1-20150707/#model-liveness,
    // "The time passed to a requestAnimationFrame callback will be equal to
    // document.timeline.currentTime".  In our case,
    // document.timeline.currentTime is derived from the latest sample time.
    window_->RunAnimationFrameCallbacks();
  }

  // It should never be possible for for the computed styles and box tree to
  // be dirty when a render tree is not pending.
  DCHECK(is_render_tree_pending_ || !are_computed_styles_and_box_tree_dirty_);
  if (is_render_tree_pending_) {
    if (!has_layout_processing_started) {
      // We want to catch the beginning of all layout processing.  If it didn't
      // begin before the call to RunAnimationFrameCallbacks(), then the flow
      // starts here instead.
      TRACE_EVENT_BEGIN0("cobalt::layout", kBenchmarkStatLayout);
    }

    if (are_computed_styles_and_box_tree_dirty_) {
      layout::UpdateComputedStylesAndLayoutBoxTree(
          locale_, window_->document(), dom_max_element_depth_,
          used_style_provider_.get(), layout_stat_tracker_,
          line_break_iterator_.get(), character_break_iterator_.get(),
          &initial_containing_block_);
      are_computed_styles_and_box_tree_dirty_ = false;
    }

    // If no render tree has been produced yet, check if html display
    // should prevent the first render tree.
    bool display_none_prevents_render =
        !produced_render_tree_ && !document->html()->IsDisplayed();

    if (!document->render_postponed() && !display_none_prevents_render) {
      scoped_refptr<render_tree::Node> render_tree_root =
          layout::GenerateRenderTreeFromBoxTree(used_style_provider_.get(),
                                                layout_stat_tracker_,
                                                &initial_containing_block_);
      bool run_on_render_tree_produced_callback = true;
      produced_render_tree_ = true;
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

      is_render_tree_pending_ = false;
    }
    TRACE_EVENT_END0("cobalt::layout", kBenchmarkStatLayout);
  }

  on_layout_callback_.Run();
}

LayoutManager::LayoutManager(
    const std::string& name, const scoped_refptr<dom::Window>& window,
    const OnRenderTreeProducedCallback& on_render_tree_produced,
    const OnLayoutCallback& on_layout, LayoutTrigger layout_trigger,
    const int dom_max_element_depth, const float layout_refresh_rate,
    const std::string& language, bool enable_image_animations,
    LayoutStatTracker* layout_stat_tracker)
    : impl_(new Impl(name, window, on_render_tree_produced, on_layout,
                     layout_trigger, dom_max_element_depth, layout_refresh_rate,
                     language, enable_image_animations, layout_stat_tracker)) {}

LayoutManager::~LayoutManager() {}

void LayoutManager::Suspend() { impl_->Suspend(); }
void LayoutManager::Resume() { impl_->Resume(); }
bool LayoutManager::IsRenderTreePending() const {
  return impl_->IsRenderTreePending();
}

}  // namespace layout
}  // namespace cobalt
