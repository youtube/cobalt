/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/dom/benchmark_stat_names.h"
#include "cobalt/layout/benchmark_stat_names.h"
#include "cobalt/layout_tests/layout_snapshot.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/system_window/create_system_window.h"
#include "cobalt/trace_event/benchmark.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace layout_tests {

namespace {

// The URL of the web page to load and perform the layout benchmarks on.  In the
// future this may be a list of URLs.
const char kURL[] =
    "file:///cobalt/browser/testdata/performance-spike/index.html";

// The RendererBenchmarkRunner sets up an environment where we can control
// the number of benchmark samples we acquire from the renderer by counting
// each time the renderer submit complete callback is made.  It also manages
// the skipping of the first frame to avoid the first frame outlier.
class RendererBenchmarkRunner {
 public:
  explicit RendererBenchmarkRunner(int samples_to_gather)
      : samples_to_gather_(samples_to_gather),
        submits_completed_(0),
        done_gathering_samples_(true, false),
        system_window_(system_window::CreateSystemWindow()),
        renderer_module_(system_window_.get(),
                         renderer::RendererModule::Options()) {}

  // Return the resource provider from the internal renderer so that it can
  // be used during layout.
  render_tree::ResourceProvider* GetResourceProvider() {
    return renderer_module_.pipeline()->GetResourceProvider();
  }

  // Run the renderer benchmarks and perform the measurements.
  void RunBenchmarks(const browser::WebModule::LayoutResults& layout_results) {
    base::debug::TraceLog::GetInstance()->SetEnabled(false);

    renderer::Pipeline::Submission submission_with_callback(
        layout_results.render_tree, layout_results.animations,
        layout_results.layout_time);
    submission_with_callback.submit_complete_callback = base::Bind(
        &RendererBenchmarkRunner::OnSubmitComplete, base::Unretained(this));

    renderer_module_.pipeline()->Submit(submission_with_callback);

    done_gathering_samples_.Wait();
  }

 private:
  // Called whenever the renderer completes a frame submission.  This allows us
  // to count how many frames have been submitted so we know when to stop (via
  // the signaling of the done_gathering_samples_ event).
  void OnSubmitComplete() {
    base::debug::TraceLog::GetInstance()->SetEnabled(true);

    ++submits_completed_;

    // We wait for samples_to_gather + 1 submits to complete because we actually
    // want samples_to_gather, but we skipped the first submit.
    if (submits_completed_ >= samples_to_gather_ + 1) {
      done_gathering_samples_.Signal();
    }
  }

  const int samples_to_gather_;
  int submits_completed_;
  base::WaitableEvent done_gathering_samples_;

  scoped_ptr<system_window::SystemWindow> system_window_;
  renderer::RendererModule renderer_module_;
};

}  // namespace

// clang-format off
TRACE_EVENT_BENCHMARK10(
    LayoutTimesForPerformanceSpikeInitialLayout,
    layout::kBenchmarkStatLayout, cobalt::trace_event::IN_SCOPE_DURATION,
    dom::kBenchmarkStatUpdateMatchingRules,
        cobalt::trace_event::IN_SCOPE_DURATION,
    dom::kBenchmarkStatUpdateComputedStyles,
        cobalt::trace_event::IN_SCOPE_DURATION,
    layout::kBenchmarkStatBoxGeneration,
        cobalt::trace_event::IN_SCOPE_DURATION,
    layout::kBenchmarkStatUpdateCrossReferences,
        cobalt::trace_event::IN_SCOPE_DURATION,
    layout::kBenchmarkStatUpdateUsedSizes,
        cobalt::trace_event::IN_SCOPE_DURATION,
    layout::kBenchmarkStatRenderAndAnimate,
        cobalt::trace_event::IN_SCOPE_DURATION,
    "NodeAnimationsMap::Apply()", cobalt::trace_event::IN_SCOPE_DURATION,
    "VisitRenderTree", cobalt::trace_event::IN_SCOPE_DURATION,
    "Skia Flush", cobalt::trace_event::IN_SCOPE_DURATION) {
  // clang-format on

  // Get rid of all log output so we only see benchmark results.
  logging::SetMinLogLevel(100);

  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);

  // We setup the renderer benchmark runner first so that we can gain access
  // to the resource provider.
  const int kRendererSampleCount = 60;
  RendererBenchmarkRunner renderer_benchmark_runner(kRendererSampleCount);

  // We prepare a layout_results variable where we place the results from each
  // layout.  We will then use the final layout_results as input to the renderer
  // benchmark.
  base::optional<browser::WebModule::LayoutResults> layout_results;

  // Set up a WebModule, load the URL and trigger layout multiple times and get
  // layout benchmark results.  We start with tracing disabled so that we can
  // skip the first layout, which is typically an outlier.
  base::debug::TraceLog::GetInstance()->SetEnabled(false);
  const int kLayoutSampleCount = 10;
  for (int i = 0; i < kLayoutSampleCount + 1; ++i) {
    // Load and layout the scene.
    layout_results =
        SnapshotURL(GURL(kURL), math::Size(1920, 1080),
                    renderer_benchmark_runner.GetResourceProvider());

    if (i == 0) {
      // Enable tracing after the first (outlier) layout is completed.
      base::debug::TraceLog::GetInstance()->SetEnabled(true);
    }
  }

  // Finally run the renderer benchmarks to acquire performance data on
  // rendering.
  renderer_benchmark_runner.RunBenchmarks(*layout_results);
}

}  // namespace layout_tests
}  // namespace cobalt
