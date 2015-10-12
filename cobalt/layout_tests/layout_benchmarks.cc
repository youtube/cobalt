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
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/dom/benchmark_stat_names.h"
#include "cobalt/layout/benchmark_stat_names.h"
#include "cobalt/layout_tests/layout_snapshot.h"
#include "cobalt/layout_tests/test_parser.h"
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

class LayoutBenchmark : public trace_event::Benchmark {
 public:
  explicit LayoutBenchmark(const TestInfo& test_info);
  ~LayoutBenchmark() OVERRIDE {}

  void Experiment() OVERRIDE;
  void AnalyzeTraceEvent(
      const scoped_refptr<trace_event::EventParser::ScopedEvent>& event)
      OVERRIDE;
  std::vector<trace_event::Benchmark::Result> CompileResults() OVERRIDE;

 private:
  static std::string FilePathToBenchmarkName(const FilePath& filepath);

  TestInfo test_info_;
  typedef base::hash_map<std::string, std::vector<double> > SampleMap;
  SampleMap samples_;
};

LayoutBenchmark::LayoutBenchmark(const TestInfo& test_info)
    : test_info_(test_info) {
  // Setup the name's benchmark based on the test entry file path.
  set_name(FilePathToBenchmarkName(test_info_.base_file_path));

  // Define the set of event names that we would like to watch for by
  // initializing their map entries to the default constructed values (e.g.
  // std::vector<double>()).
  samples_[layout::kBenchmarkStatLayout];
  samples_[dom::kBenchmarkStatUpdateMatchingRules];
  samples_[dom::kBenchmarkStatUpdateComputedStyles];
  samples_[layout::kBenchmarkStatBoxGeneration];
  samples_[layout::kBenchmarkStatUpdateCrossReferences];
  samples_[layout::kBenchmarkStatUpdateUsedSizes];
  samples_[layout::kBenchmarkStatRenderAndAnimate];
  samples_["NodeAnimationsMap::Apply()"];
  samples_["VisitRenderTree"];
  samples_["Skia Flush"];
}

std::string LayoutBenchmark::FilePathToBenchmarkName(const FilePath& filepath) {
  std::vector<FilePath::StringType> components;
  filepath.GetComponents(&components);

  // Don't include the "benchmarks" directory as part of the benchmark name.
  components.erase(components.begin());

  return JoinString(components, '/');
}

void LayoutBenchmark::Experiment() {
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

  const math::Size kDefaultViewportSize(1920, 1080);
  math::Size viewport_size = test_info_.viewport_size
                                 ? *test_info_.viewport_size
                                 : kDefaultViewportSize;

  // Set up a WebModule, load the URL and trigger layout multiple times and get
  // layout benchmark results.  We start with tracing disabled so that we can
  // skip the first layout, which is typically an outlier.
  base::debug::TraceLog::GetInstance()->SetEnabled(false);
  const int kLayoutSampleCount = 10;
  for (int i = 0; i < kLayoutSampleCount + 1; ++i) {
    // Load and layout the scene.
    layout_results =
        SnapshotURL(test_info_.url, viewport_size,
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

void LayoutBenchmark::AnalyzeTraceEvent(
    const scoped_refptr<trace_event::EventParser::ScopedEvent>& event) {
  SampleMap::iterator found = samples_.find(event->name());
  if (found != samples_.end()) {
    found->second.push_back(event->in_scope_duration()->InSecondsF());
  }
}

std::vector<trace_event::Benchmark::Result> LayoutBenchmark::CompileResults() {
  std::vector<trace_event::Benchmark::Result> results;
  for (SampleMap::iterator iter = samples_.begin(); iter != samples_.end();
       ++iter) {
    results.push_back(trace_event::Benchmark::Result(
        iter->first + " in-scope duration in seconds", iter->second));
  }
  return results;
}

class LayoutBenchmarkCreator : public trace_event::BenchmarkCreator {
 public:
  ScopedVector<trace_event::Benchmark> CreateBenchmarks() OVERRIDE {
    ScopedVector<trace_event::Benchmark> benchmarks;

    std::vector<TestInfo> benchmark_infos = EnumerateLayoutTests("benchmarks");
    for (std::vector<TestInfo>::const_iterator iter = benchmark_infos.begin();
         iter != benchmark_infos.end(); ++iter) {
      benchmarks.push_back(new LayoutBenchmark(*iter));
    }

    return benchmarks.Pass();
  }

 private:
};
LayoutBenchmarkCreator g_benchmark_creator;

}  // namespace layout_tests
}  // namespace cobalt
