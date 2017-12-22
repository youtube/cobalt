// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/dom/benchmark_stat_names.h"
#include "cobalt/layout/benchmark_stat_names.h"
#include "cobalt/layout_tests/layout_snapshot.h"
#include "cobalt/layout_tests/test_parser.h"
#include "cobalt/math/size.h"
#include "cobalt/renderer/pipeline.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/renderer/submission.h"
#include "cobalt/system_window/system_window.h"
#include "cobalt/trace_event/benchmark.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace layout_tests {

namespace {
const int kViewportWidth = 1920;
const int kViewportHeight = 1080;

// The RendererBenchmarkRunner sets up an environment where we can control
// the number of benchmark samples we acquire from the renderer by counting
// each time the renderer submit complete callback is made.  It also manages
// the skipping of the first frame to avoid the first frame outlier.
class RendererBenchmarkRunner {
 public:
  RendererBenchmarkRunner()
      : done_gathering_samples_(true, false),
        system_window_(new system_window::SystemWindow(
            &event_dispatcher_, math::Size(kViewportWidth, kViewportHeight))) {
    // Since we'd like to measure the renderer, we force it to rasterize each
    // frame despite the fact that the render tree may not be changing.
    renderer::RendererModule::Options renderer_options;
    renderer_options.submit_even_if_render_tree_is_unchanged = true;
    renderer_module_.emplace(system_window_.get(), renderer_options);
  }

  // Return the resource provider from the internal renderer so that it can
  // be used during layout.
  render_tree::ResourceProvider* GetResourceProvider() {
    return renderer_module_->pipeline()->GetResourceProvider();
  }

  // Run the renderer benchmarks and perform the measurements.
  void RunBenchmarks(const browser::WebModule::LayoutResults& layout_results,
                     int samples_to_gather) {
    // Initialize our per-RunBenchmarks() state.
    samples_to_gather_ = samples_to_gather;
    done_gathering_samples_.Reset();

    renderer::Submission submission_with_callback(layout_results.render_tree,
                                                  layout_results.layout_time);
    submission_with_callback.on_rasterized_callbacks.emplace_back(base::Bind(
        &RendererBenchmarkRunner::OnSubmitComplete, base::Unretained(this)));

    renderer_module_->pipeline()->Submit(submission_with_callback);

    done_gathering_samples_.Wait();

    renderer_module_->pipeline()->Clear();
  }

 private:
  // Called whenever the renderer completes a frame submission.  This allows us
  // to count how many frames have been submitted so we know when to stop (via
  // the signaling of the done_gathering_samples_ event).
  void OnSubmitComplete() {
    --samples_to_gather_;

    // We wait for samples_to_gather + 1 submits to complete because we actually
    // want samples_to_gather, but we skipped the first submit.
    if (samples_to_gather_ <= 0) {
      done_gathering_samples_.Signal();
    }
  }

  int samples_to_gather_;
  base::WaitableEvent done_gathering_samples_;
  base::EventDispatcher event_dispatcher_;

  scoped_ptr<system_window::SystemWindow> system_window_;
  base::optional<renderer::RendererModule> renderer_module_;
};

}  // namespace

class LayoutBenchmark : public trace_event::Benchmark {
 public:
  explicit LayoutBenchmark(const TestInfo& test_info);
  ~LayoutBenchmark() override {}

  void Experiment() override;
  void AnalyzeTraceEvent(
      const scoped_refptr<trace_event::EventParser::ScopedEvent>& event)
      override;
  std::vector<trace_event::Benchmark::Result> CompileResults() override;

 private:
  typedef base::hash_map<std::string, double> IntermediateResultsMap;
  typedef base::hash_map<std::string, std::vector<double> >
      FinalResultsSampleMap;

  void OnIterationComplete();
  static std::string FilePathToBenchmarkName(const FilePath& filepath);

  TestInfo test_info_;

  // During each iteration, we accumulate intermediate results by *adding*
  // task times together.  Only when the iteration is complete do we consider
  // our result a sample.
  IntermediateResultsMap intermediate_results_;

  // A list of accumulated intermediate results.  The vectors in this map are
  // pushed to at the end of each iteration.
  FinalResultsSampleMap layout_samples_;
  FinalResultsSampleMap renderer_samples_;

  // Is this our first iteration?
  bool first_iteration_;

  // We setup the renderer benchmark runner first so that we can gain access
  // to the resource provider and so that we can also benchmark the rendering
  // of the layed out web pages.
  RendererBenchmarkRunner renderer_benchmark_runner_;
};

LayoutBenchmark::LayoutBenchmark(const TestInfo& test_info)
    : test_info_(test_info), first_iteration_(true) {
  // Setup the name's benchmark based on the test entry file path.
  set_name(FilePathToBenchmarkName(test_info_.base_file_path));
  set_num_iterations(10, base::Bind(&LayoutBenchmark::OnIterationComplete,
                                    base::Unretained(this)));

  // Define the set of event names that we would like to watch for by
  // initializing their map entries to the default constructed values (e.g.
  // std::vector<double>()).
  layout_samples_[layout::kBenchmarkStatLayout];
  layout_samples_[dom::kBenchmarkStatUpdateSelectorTree];
  layout_samples_[dom::kBenchmarkStatUpdateComputedStyles];
  layout_samples_[layout::kBenchmarkStatBoxGeneration];
  layout_samples_[layout::kBenchmarkStatUpdateCrossReferences];
  layout_samples_[layout::kBenchmarkStatUpdateUsedSizes];
  layout_samples_[layout::kBenchmarkStatRenderAndAnimate];
  renderer_samples_["AnimateNode::Apply()"];
  renderer_samples_["VisitRenderTree"];
  renderer_samples_["Skia Flush"];
}

std::string LayoutBenchmark::FilePathToBenchmarkName(const FilePath& filepath) {
  std::vector<FilePath::StringType> components;
  filepath.GetComponents(&components);

  // Don't include the "benchmarks" directory as part of the benchmark name.
  components.erase(components.begin());

  return JoinString(components, '/');
}

void LayoutBenchmark::Experiment() {
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);

  // We prepare a layout_results variable where we place the results from each
  // layout.  We will then use the final layout_results as input to the renderer
  // benchmark.
  base::optional<browser::WebModule::LayoutResults> layout_results;

  const math::Size kDefaultViewportSize(1920, 1080);
  math::Size viewport_size = test_info_.viewport_size
                                 ? *test_info_.viewport_size
                                 : kDefaultViewportSize;

  // Set up a WebModule, load the URL and trigger layout to get layout benchmark
  // results.
  layout_results =
      SnapshotURL(test_info_.url, viewport_size,
                  renderer_benchmark_runner_.GetResourceProvider());

  // Finally run the renderer benchmarks to acquire performance data on
  // rendering.
  renderer_benchmark_runner_.RunBenchmarks(*layout_results, 60);
}

namespace {

// Return true if the event has an ancestor with the specified named event.
bool HasAncestorEvent(
    const scoped_refptr<trace_event::EventParser::ScopedEvent>& event,
    const std::string& ancestor_event_string) {
  scoped_refptr<trace_event::EventParser::ScopedEvent> ancestor_event =
      event->parent();
  while (ancestor_event) {
    if (ancestor_event->name() == ancestor_event_string) {
      break;
    }
    ancestor_event = ancestor_event->parent();
  }

  return ancestor_event.get() != NULL;
}

}  // namespace

void LayoutBenchmark::AnalyzeTraceEvent(
    const scoped_refptr<trace_event::EventParser::ScopedEvent>& event) {
  // Check if this is a layout sample.
  if (event->name() == layout::kBenchmarkStatNonMeasuredLayout) {
    // If this is a layout that should not be measured, use that as a signal
    // that we are measuring partial layout, and clear out all measured data
    // so far and start fresh for the eventual measured layout.
    intermediate_results_.clear();
    return;
  }

  FinalResultsSampleMap::iterator found_layout =
      layout_samples_.find(event->name());
  if (found_layout != layout_samples_.end() &&
      !HasAncestorEvent(event, layout::kBenchmarkStatNonMeasuredLayout)) {
    if (!ContainsKey(intermediate_results_, found_layout->first)) {
      intermediate_results_[found_layout->first] = 0;
    }

    double event_duration = event->in_scope_duration()->InSecondsF();
    intermediate_results_[found_layout->first] += event_duration;

    if (event->name() != layout::kBenchmarkStatLayout &&
        !HasAncestorEvent(event, layout::kBenchmarkStatLayout)) {
      // If the event (which we have specifically requested to include in
      // the benchmark results) does not fall under the scope of the official
      // layout event, artificially increase the tracked layout time to include
      // it.  This way events can occur at any time, and the scoped layout
      // event still gives us a way to measure unaccounted for time that we
      // know is devoted to layout.
      intermediate_results_[layout::kBenchmarkStatLayout] += event_duration;
    }
  }

  FinalResultsSampleMap::iterator found_renderer =
      renderer_samples_.find(event->name());
  if (found_renderer != renderer_samples_.end()) {
    found_renderer->second.push_back(event->in_scope_duration()->InSecondsF());
  }
}

void LayoutBenchmark::OnIterationComplete() {
  // Skip recording the results of the first iteration, since a few things
  // may have been lazily initialized and we'd like to avoid recording that.
  if (first_iteration_) {
    for (FinalResultsSampleMap::iterator iter = renderer_samples_.begin();
         iter != renderer_samples_.end(); ++iter) {
      iter->second.clear();
    }
  } else {
    // Save our finalized intermediate results into our finalized results, and
    // then clear out our intermediate results for the next iteration.
    for (FinalResultsSampleMap::iterator iter = layout_samples_.begin();
         iter != layout_samples_.end(); ++iter) {
      if (ContainsKey(intermediate_results_, iter->first)) {
        iter->second.push_back(intermediate_results_[iter->first]);
      }
    }
  }

  first_iteration_ = false;
  intermediate_results_.clear();
}

std::vector<trace_event::Benchmark::Result> LayoutBenchmark::CompileResults() {
  std::vector<trace_event::Benchmark::Result> results;
  for (FinalResultsSampleMap::iterator iter = layout_samples_.begin();
       iter != layout_samples_.end(); ++iter) {
    results.push_back(trace_event::Benchmark::Result(
        iter->first + " in-scope duration in seconds", iter->second));
  }
  for (FinalResultsSampleMap::iterator iter = renderer_samples_.begin();
       iter != renderer_samples_.end(); ++iter) {
    results.push_back(trace_event::Benchmark::Result(
        iter->first + " in-scope duration in seconds", iter->second));
  }
  return results;
}

class LayoutBenchmarkCreator : public trace_event::BenchmarkCreator {
 public:
  std::vector<CreateBenchmarkFunction> GetBenchmarkCreators() override {
    std::vector<CreateBenchmarkFunction> benchmarks;

    std::vector<TestInfo> benchmark_infos = EnumerateLayoutTests("benchmarks");
    for (std::vector<TestInfo>::const_iterator iter = benchmark_infos.begin();
         iter != benchmark_infos.end(); ++iter) {
      benchmarks.push_back(
          base::Bind(&LayoutBenchmarkCreator::CreateLayoutBenchmark, *iter));
    }

    return benchmarks;
  }

 private:
  static scoped_ptr<trace_event::Benchmark> CreateLayoutBenchmark(
      const TestInfo& test_info) {
    return scoped_ptr<trace_event::Benchmark>(new LayoutBenchmark(test_info));
  }
};
LayoutBenchmarkCreator g_benchmark_creator;

}  // namespace layout_tests
}  // namespace cobalt
