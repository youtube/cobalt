// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/version.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/h5vcc_metrics/public/mojom/h5vcc_metrics.mojom.h"
#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"
#include "cobalt/browser/metrics/cobalt_memory_metrics_emitter.h"
#include "cobalt/browser/metrics/cobalt_metrics_log_uploader.h"
#include "cobalt/shell/common/shell_paths.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_client.h"
#include "media/base/mock_filters.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

using base::trace_event::MemoryAllocatorDump;
using ::testing::_;
using ::testing::ByMove;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

class TestProcessMemoryMetricsEmitter : public CobaltMemoryMetricsEmitter {
 public:
  TestProcessMemoryMetricsEmitter() = default;

  void FetchAndEmitProcessMemoryMetrics() override {
    // Prepare a dummy GlobalMemoryDump.
    memory_instrumentation::mojom::GlobalMemoryDumpPtr dump_ptr =
        memory_instrumentation::mojom::GlobalMemoryDump::New();

    // Browser process dump
    auto browser_dump = memory_instrumentation::mojom::ProcessMemoryDump::New();
    browser_dump->process_type =
        memory_instrumentation::mojom::ProcessType::BROWSER;
    browser_dump->os_dump = memory_instrumentation::mojom::OSMemDump::New();
    browser_dump->os_dump->private_footprint_kb = 10240;  // 10 MB
    browser_dump->os_dump->resident_set_kb = 20480;       // 20 MB
    browser_dump->os_dump->shared_footprint_kb = 5120;    // 5 MB

    // Add a blink_gc dump
    auto blink_gc_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    blink_gc_dump->numeric_entries["effective_size"] = 10 * 1024 * 1024;
    blink_gc_dump->numeric_entries["allocated_objects_size"] = 6 * 1024 * 1024;
    browser_dump->chrome_allocator_dumps["blink_gc"] = std::move(blink_gc_dump);

    // Add a blink_gc/main dump
    auto blink_gc_main_dump =
        memory_instrumentation::mojom::AllocatorMemDump::New();
    blink_gc_main_dump->numeric_entries["effective_size"] = 5 * 1024 * 1024;
    blink_gc_main_dump->numeric_entries["allocated_objects_size"] =
        3 * 1024 * 1024;
    browser_dump->chrome_allocator_dumps["blink_gc/main"] =
        std::move(blink_gc_main_dump);

    // Add a malloc dump
    auto malloc_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    malloc_dump->numeric_entries["effective_size"] = 10 * 1024 * 1024;
    malloc_dump->numeric_entries["allocated_objects_size"] = 8 * 1024 * 1024;
    browser_dump->chrome_allocator_dumps["malloc"] = std::move(malloc_dump);

    // Add Skia Glyph Cache dump
    auto skia_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    skia_dump->numeric_entries["size"] = 2 * 1024 * 1024;
    browser_dump->chrome_allocator_dumps["skia/sk_glyph_cache"] =
        std::move(skia_dump);

    // Add Font Caches dump
    auto font_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    font_dump->numeric_entries["size"] = 1024 * 1024;
    browser_dump->chrome_allocator_dumps["font_caches/shape_caches"] =
        std::move(font_dump);

    // Add blink_objects dumps
    auto doc_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    doc_dump->numeric_entries[MemoryAllocatorDump::kNameObjectCount] = 3;
    browser_dump->chrome_allocator_dumps["blink_objects/Document"] =
        std::move(doc_dump);

    auto frame_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    frame_dump->numeric_entries[MemoryAllocatorDump::kNameObjectCount] = 1;
    browser_dump->chrome_allocator_dumps["blink_objects/Frame"] =
        std::move(frame_dump);

    auto layout_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    layout_dump->numeric_entries[MemoryAllocatorDump::kNameObjectCount] = 10;
    browser_dump->chrome_allocator_dumps["blink_objects/LayoutObject"] =
        std::move(layout_dump);

    auto node_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    node_dump->numeric_entries[MemoryAllocatorDump::kNameObjectCount] = 50;
    browser_dump->chrome_allocator_dumps["blink_objects/Node"] =
        std::move(node_dump);

    // Add Java Heap dump
    auto java_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    java_dump->numeric_entries["effective_size"] = 4 * 1024 * 1024;
    browser_dump->chrome_allocator_dumps["java_heap"] = std::move(java_dump);

    // Add LevelDatabase dump
    auto leveldb_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    leveldb_dump->numeric_entries["effective_size"] = 512 * 1024;
    browser_dump->chrome_allocator_dumps["leveldatabase"] =
        std::move(leveldb_dump);

    // Add PartitionAlloc dump
    auto pa_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    pa_dump->numeric_entries["effective_size"] = 16 * 1024 * 1024;
    browser_dump->chrome_allocator_dumps["partition_alloc"] =
        std::move(pa_dump);

    auto pa_allocated_dump =
        memory_instrumentation::mojom::AllocatorMemDump::New();
    pa_allocated_dump->numeric_entries["effective_size"] = 12 * 1024 * 1024;
    browser_dump->chrome_allocator_dumps["partition_alloc/allocated_objects"] =
        std::move(pa_allocated_dump);

    // Add Skia dump (total)
    auto skia_total_dump =
        memory_instrumentation::mojom::AllocatorMemDump::New();
    skia_total_dump->numeric_entries["effective_size"] = 2 * 1024 * 1024;
    browser_dump->chrome_allocator_dumps["skia"] = std::move(skia_total_dump);

    // Add Sqlite dump
    auto sqlite_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    sqlite_dump->numeric_entries["effective_size"] = 1 * 1024;
    browser_dump->chrome_allocator_dumps["sqlite"] = std::move(sqlite_dump);

    // Add UI dump
    auto ui_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    ui_dump->numeric_entries["effective_size"] = 2 * 1024;
    browser_dump->chrome_allocator_dumps["ui"] = std::move(ui_dump);

    // Add V8 dump
    auto v8_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
    v8_dump->numeric_entries["effective_size"] = 12 * 1024 * 1024;
    v8_dump->numeric_entries["allocated_objects_size"] = 10 * 1024 * 1024;
    browser_dump->chrome_allocator_dumps["v8"] = std::move(v8_dump);

    dump_ptr->process_dumps.push_back(std::move(browser_dump));

    // Renderer process dump
    auto renderer_dump =
        memory_instrumentation::mojom::ProcessMemoryDump::New();
    renderer_dump->process_type =
        memory_instrumentation::mojom::ProcessType::RENDERER;
    renderer_dump->os_dump = memory_instrumentation::mojom::OSMemDump::New();
    renderer_dump->os_dump->private_footprint_kb = 20480;  // 20 MB

    auto renderer_blink_gc_dump =
        memory_instrumentation::mojom::AllocatorMemDump::New();
    renderer_blink_gc_dump->numeric_entries["effective_size"] = 8 * 1024 * 1024;
    renderer_blink_gc_dump->numeric_entries["allocated_objects_size"] =
        5 * 1024 * 1024;
    renderer_dump->chrome_allocator_dumps["blink_gc"] =
        std::move(renderer_blink_gc_dump);

    dump_ptr->process_dumps.push_back(std::move(renderer_dump));

    auto global_dump =
        memory_instrumentation::GlobalMemoryDump::MoveFrom(std::move(dump_ptr));

    // Manually trigger ReceivedMemoryDump with our dummy dump.
    ReceivedMemoryDump(true, std::move(global_dump));
  }

 protected:
  ~TestProcessMemoryMetricsEmitter() override = default;
};

class TestCpuMetricsEmitter : public CobaltCpuMetricsEmitter {
 public:
  TestCpuMetricsEmitter() = default;

  // Mock CPU usage to verify accurate recording of average.
  double GetCpuUsage() override {
    const int num_processors = base::SysInfo::NumberOfProcessors();
    double mock_usage = 50.0 * num_processors;  // 50% per core.
    return mock_usage;
  }

 protected:
  ~TestCpuMetricsEmitter() override = default;
};

// Mock for MetricsService to verify construction of a specific MetricsService
// in tests.
class MockMetricsService : public metrics::MetricsService {
 public:
  MockMetricsService(metrics::MetricsStateManager* state_manager,
                     metrics::MetricsServiceClient* client,
                     PrefService* local_state)
      : metrics::MetricsService(state_manager, client, local_state) {}
};

class MockCobaltMetricsLogUploader : public CobaltMetricsLogUploader {
 public:
  MockCobaltMetricsLogUploader()
      : CobaltMetricsLogUploader(
            ::metrics::MetricsLogUploader::MetricServiceType::UMA) {}
  MOCK_METHOD(void,
              UploadLog,
              (const std::string& compressed_log_data,
               const metrics::LogMetadata& log_metadata,
               const std::string& log_hash,
               const std::string& log_signature,
               const metrics::ReportingInfo& reporting_info),
              (override));
  MOCK_METHOD(
      void,
      SetMetricsListener,
      (mojo::PendingRemote<h5vcc_metrics::mojom::MetricsListener> listener),
      (override));

  // Provide a way to call the real GetWeakPtr for the mock framework.
  base::WeakPtr<CobaltMetricsLogUploader> RealGetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  // Mock GetWeakPtr itself if specific test scenarios require controlling it.
  MOCK_METHOD(base::WeakPtr<CobaltMetricsLogUploader>, GetWeakPtr, ());

  MOCK_METHOD(void,
              setOnUploadComplete,
              (const ::metrics::MetricsLogUploader::UploadCallback&),
              (override));

 private:
  base::WeakPtrFactory<MockCobaltMetricsLogUploader> weak_ptr_factory_{this};
};

class MockH5vccMetricsListener : public h5vcc_metrics::mojom::MetricsListener {
 public:
  MOCK_METHOD(void,
              OnMetrics,
              (h5vcc_metrics::mojom::H5vccMetricType metric_type,
               const std::string& serialized_proto),
              (override));
};

// Test version of CobaltMetricsServiceClient to allow injecting mock
// dependencies. This requires CobaltMetricsServiceClient to have virtual
// methods for creating its owned dependencies (MetricsService,
// CobaltMetricsLogUploader).
class TestCobaltMetricsServiceClient : public CobaltMetricsServiceClient {
 public:
  TestCobaltMetricsServiceClient(
      metrics::MetricsStateManager* state_manager,
      std::unique_ptr<variations::SyntheticTrialRegistry>
          synthetic_trial_registry,
      PrefService* local_state)
      : CobaltMetricsServiceClient(state_manager,
                                   std::move(synthetic_trial_registry),
                                   local_state) {}

  // Override internal factory for MetricsService
  std::unique_ptr<metrics::MetricsService> CreateMetricsServiceInternal(
      metrics::MetricsStateManager* state_manager,
      metrics::MetricsServiceClient* client,
      PrefService* local_state) override {
    auto mock_service = std::make_unique<StrictMock<MockMetricsService>>(
        state_manager, client, local_state);
    mock_metrics_service_ = mock_service.get();
    return std::move(mock_service);
  }

  // Override internal factory for CobaltMetricsLogUploader.
  std::unique_ptr<CobaltMetricsLogUploader> CreateLogUploaderInternal()
      override {
    auto mock_uploader =
        std::make_unique<StrictMock<MockCobaltMetricsLogUploader>>();
    // Setup the mock's GetWeakPtr to return a usable weak pointer.
    ON_CALL(*mock_uploader, GetWeakPtr())
        .WillByDefault([uploader_ptr = mock_uploader.get()]() {
          return static_cast<MockCobaltMetricsLogUploader*>(uploader_ptr)
              ->RealGetWeakPtr();
        });
    mock_log_uploader_ = mock_uploader.get();
    return std::move(mock_uploader);
  }

  scoped_refptr<CobaltMemoryMetricsEmitter> CreateMemoryMetricsEmitter()
      override {
    return base::MakeRefCounted<TestProcessMemoryMetricsEmitter>();
  }

  scoped_refptr<CobaltCpuMetricsEmitter> CreateCpuMetricsEmitter() override {
    return base::MakeRefCounted<TestCpuMetricsEmitter>();
  }

  void OnApplicationNotIdleInternal() override {
    on_application_not_idle_internal_called_ = true;
  }

  bool GetOnApplicationNotIdleInternalCalled() {
    return on_application_not_idle_internal_called_;
  }

  void ResetOnApplicationNotIdleInternalCalled() {
    on_application_not_idle_internal_called_ = false;
  }

  // Expose Initialize for finer-grained control in tests if necessary,
  // though the base class's static Create method normally handles this.
  void CallInitialize() { Initialize(); }

  StrictMock<MockMetricsService>* mock_metrics_service() const {
    return mock_metrics_service_;
  }
  StrictMock<MockCobaltMetricsLogUploader>* mock_log_uploader() const {
    return mock_log_uploader_;
  }

  // Expose the timer for inspection in tests.
  const base::RepeatingTimer& idle_refresh_timer() const {
    return idle_refresh_timer_;
  }

  void ClearMockUploaderPtr() { mock_log_uploader_ = nullptr; }

 private:
  raw_ptr<StrictMock<MockMetricsService>> mock_metrics_service_ = nullptr;
  raw_ptr<StrictMock<MockCobaltMetricsLogUploader>> mock_log_uploader_ =
      nullptr;
  bool on_application_not_idle_internal_called_ = false;
};

class CobaltMetricsServiceClientBaseTest : public ::testing::Test {
 protected:
  CobaltMetricsServiceClientBaseTest() { mojo::core::Init(); }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_CACHE, temp_dir_.GetPath());

    // Register all metric-related prefs, otherwise tests crash when calling
    // upstream metrics code.
    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    prefs_.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, false);

    // Make sure user_metrics.cc has a g_task_runner.
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    // The client under test receives MetricsStateManager, so create one.
    // A real CobaltEnabledStateProvider is simple enough.
    enabled_state_provider_ =
        std::make_unique<CobaltEnabledStateProvider>(&prefs_);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &prefs_, enabled_state_provider_.get(), std::wstring(),
        temp_dir_.GetPath(), metrics::StartupVisibility::kForeground);
    ASSERT_THAT(metrics_state_manager_, NotNull());

    // Instantiate mock media client for testing
#if BUILDFLAG(USE_STARBOARD_MEDIA)
    mock_media_client_ = std::make_unique<media::MockMediaClient>();
    media::SetMediaClient(mock_media_client_.get());
#endif
  }

  void TearDown() override {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
    media::SetMediaClient(nullptr);
    media::DecoderBuffer::Allocator::Set(nullptr);
#endif
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI};
  TestingPrefServiceSimple prefs_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> path_override_;
  std::unique_ptr<CobaltEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  std::unique_ptr<media::MockMediaClient> mock_media_client_;
#endif
};

class CobaltMetricsServiceClientTest
    : public CobaltMetricsServiceClientBaseTest {
 protected:
  void SetUp() override {
    CobaltMetricsServiceClientBaseTest::SetUp();

    InitializeClient();
  }

  std::unique_ptr<TestCobaltMetricsServiceClient> client_;
  base::raw_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;

 private:
  void InitializeClient() {
    auto synthetic_trial_registry =
        std::make_unique<variations::SyntheticTrialRegistry>();
    synthetic_trial_registry_ = synthetic_trial_registry.get();

    // Instantiate the test client and call Initialize to trigger mock creation.
    // This simulates the two-phase initialization of the static Create().
    client_ = std::make_unique<TestCobaltMetricsServiceClient>(
        metrics_state_manager_.get(), std::move(synthetic_trial_registry),
        &prefs_);
    client_->CallInitialize();  // This will use the overridden factory methods.

    ASSERT_THAT(client_->mock_metrics_service(), NotNull());
    ASSERT_THAT(client_->mock_log_uploader(), NotNull());
  }
};

TEST_F(CobaltMetricsServiceClientTest, PostCreateInitialization) {
  EXPECT_EQ(client_->GetMetricsService(), client_->mock_metrics_service());
}

TEST_F(CobaltMetricsServiceClientTest,
       GetSyntheticTrialRegistryReturnsInjectedInstance) {
  EXPECT_EQ(client_->GetSyntheticTrialRegistry(),
            synthetic_trial_registry_.get());
}

TEST_F(CobaltMetricsServiceClientTest,
       GetMetricsServiceReturnsCreatedInstance) {
  EXPECT_EQ(client_->GetMetricsService(), client_->mock_metrics_service());
  EXPECT_THAT(client_->GetMetricsService(), NotNull());
}

TEST_F(CobaltMetricsServiceClientTest, SetMetricsClientIdIsNoOp) {
  client_->SetMetricsClientId("test_client_id");
  // No specific side-effect to check other than not crashing.
  SUCCEED();
}

TEST_F(CobaltMetricsServiceClientTest, GetProductReturnsDefault) {
  EXPECT_EQ(0, client_->GetProduct());
}

TEST_F(CobaltMetricsServiceClientTest, GetApplicationLocaleReturnsDefault) {
  EXPECT_EQ("en-US", client_->GetApplicationLocale());
}

TEST_F(CobaltMetricsServiceClientTest,
       GetNetworkTimeTrackerNotImplementedReturnsNull) {
  // NOTIMPLEMENTED() for now.
  EXPECT_THAT(client_->GetNetworkTimeTracker(), IsNull());
}

TEST_F(CobaltMetricsServiceClientTest, GetBrandReturnsFalse) {
  std::string brand_code;
  EXPECT_FALSE(client_->GetBrand(&brand_code));
  EXPECT_TRUE(brand_code.empty());
}

TEST_F(CobaltMetricsServiceClientTest, GetChannelReturnsUnknown) {
  EXPECT_EQ(metrics::SystemProfileProto::CHANNEL_UNKNOWN,
            client_->GetChannel());
}

TEST_F(CobaltMetricsServiceClientTest, IsExtendedStableChannelReturnsFalse) {
  EXPECT_FALSE(client_->IsExtendedStableChannel());
}

TEST_F(CobaltMetricsServiceClientTest, GetVersionStringReturnsNonEmpty) {
  // base::Version().GetString() should provide a default like "0.0.0.0".
  EXPECT_EQ(base::Version().GetString(), client_->GetVersionString());
  EXPECT_FALSE(client_->GetVersionString().empty());
}

TEST_F(CobaltMetricsServiceClientTest, RecordCpuMetricsHistogram) {
  base::HistogramTester histogram_tester;

  // Trigger CPU usage dump manually for testing.
  base::RunLoop run_loop;
  client_->ScheduleCpuRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();

  task_environment_.FastForwardBy(base::Seconds(3));
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  EXPECT_GE(histogram_tester.GetBucketCount("CPU.Total.UsageInPercentage", 50),
            1);
}

TEST_F(CobaltMetricsServiceClientTest, RecordMemoryMetricsRecordsHistogram) {
  base::HistogramTester histogram_tester;

  // Trigger a memory dump manually for testing.
  base::RunLoop run_loop;
  client_->ScheduleMemoryRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();

  // Wait for the dump to be processed.
  task_environment_.FastForwardBy(base::Seconds(3));
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  // Verify process-specific and region-specific metrics.
  // Note: we check for >= 1 sample because periodic collection might also fire.
  EXPECT_GE(
      histogram_tester.GetAllSamples("Memory.Browser.PrivateMemoryFootprint")
          .size(),
      1u);
  EXPECT_GE(histogram_tester.GetAllSamples("Memory.Total.ResidentSet").size(),
            1u);
  EXPECT_GE(
      histogram_tester
          .GetAllSamples("Memory.Experimental.Browser2.Malloc.AllocatedObjects")
          .size(),
      1u);

  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.Tiny.NumberOfDocuments", 3),
            0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.Tiny.NumberOfFrames", 1),
            0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.Tiny.NumberOfLayoutObjects", 10),
            0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.Small.NumberOfNodes", 50),
            0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.Small.FontCaches", 1024),
            0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.JavaHeap", 4),
            0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.Small.LevelDatabase", 512),
            0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.Malloc", 10),
            0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.Malloc.AllocatedObjects", 8),
            0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.PartitionAlloc", 16),
            0);
  EXPECT_GT(
      histogram_tester.GetBucketCount(
          "Memory.Experimental.Browser2.PartitionAlloc.AllocatedObjects", 12),
      0);
  EXPECT_GT(
      histogram_tester.GetBucketCount("Memory.Experimental.Browser2.Skia", 2),
      0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.Small.Skia.SkGlyphCache", 2048),
            0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.Small.Sqlite", 1),
            0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.Small.UI", 2),
            0);
  EXPECT_GT(
      histogram_tester.GetBucketCount("Memory.Experimental.Browser2.V8", 12),
      0);
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.V8.AllocatedObjects", 10),
            0);

  // Fragmentation metrics
  EXPECT_GT(histogram_tester.GetBucketCount(
                "Memory.Experimental.Browser2.BlinkGC.Fragmentation", 40),
            0);
  EXPECT_GT(
      histogram_tester.GetBucketCount(
          "Memory.Experimental.Browser2.BlinkGC.Main.Heap.Fragmentation", 40),
      0);

  // TODO(b/491179673): Investigate why this metric is not firing:
  // Memory.Experimental.Browser2.Small.FontCaches
}

TEST_F(CobaltMetricsServiceClientTest, RecordMediaMemoryMetricsHistogram) {
  base::HistogramTester histogram_tester;

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  const size_t kSize = 2 * 1024 * 1024;
  auto buffer = base::MakeRefCounted<media::DecoderBuffer>(kSize);
  uint64_t allocated = media::MediaClient::GetMediaSourceTotalAllocatedMemory();
  ASSERT_GE(allocated, static_cast<uint64_t>(kSize));
#endif

  // Trigger a memory dump manually for testing.
  base::RunLoop run_loop;
  client_->ScheduleMemoryRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();

  // Wait for the dump to be processed.
  task_environment_.FastForwardBy(base::Seconds(3));
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  EXPECT_GE(
      histogram_tester.GetAllSamples("Memory.Media.AllocatedEncodedBuffer")
          .size(),
      1u);
  EXPECT_GE(
      histogram_tester.GetBucketCount("Memory.Media.AllocatedEncodedBuffer", 2),
      1);
#endif
}

TEST_F(CobaltMetricsServiceClientTest,
       CollectFinalMetricsForLogInvokesDoneCallbackAndNotifiesService) {
  base::MockCallback<base::OnceClosure> done_callback_mock;

  EXPECT_CALL(done_callback_mock, Run());
  client_->CollectFinalMetricsForLog(done_callback_mock.Get());
}

TEST_F(CobaltMetricsServiceClientTest, GetMetricsServerUrlReturnsPlaceholder) {
  EXPECT_EQ(GURL("https://youtube.com/tv/uma"), client_->GetMetricsServerUrl());
}

TEST_F(CobaltMetricsServiceClientTest,
       CreateUploaderReturnsMockUploaderAndSetsCallback) {
  base::MockCallback<metrics::MetricsLogUploader::UploadCallback>
      on_upload_complete_mock;

  // Variable to capture the callback passed to setOnUploadComplete().
  metrics::MetricsLogUploader::UploadCallback captured_callback;

  // Expect setOnUploadComplete to be called with any callback of the correct
  // type, and use .WillOnce() to save the actual argument passed into
  // 'captured_callback'.
  EXPECT_CALL(
      *client_->mock_log_uploader(),
      setOnUploadComplete(
          testing::A<const metrics::MetricsLogUploader::UploadCallback&>()))
      .WillOnce(testing::SaveArg<0>(&captured_callback));

  // The mock_log_uploader_ is already created and its GetWeakPtr is stubbed.
  // CreateUploader will move the uploader from the client's internal
  // unique_ptr. The returned uploader should be the one our factory provided.
  std::unique_ptr<metrics::MetricsLogUploader> uploader =
      client_->CreateUploader(GURL(), GURL(), "",
                              metrics::MetricsLogUploader::UMA,
                              on_upload_complete_mock.Get());

  // The uploader returned should be the same mock instance.
  EXPECT_EQ(uploader.get(), client_->mock_log_uploader());
  EXPECT_THAT(uploader, NotNull());

  // Verify that a callback was indeed captured by SaveArg.
  ASSERT_TRUE(captured_callback) << "Callback was not captured by SaveArg. "
                                    "Was setOnUploadComplete called?";

  // Invoke the 'captured_callback' and expect on_upload_complete_mock' (the
  // original one you created) to have its Run method called. This proves that
  // the callback set by CreateUploader is functionally the mock callback.
  const int kExpectedStatusCode = 204;
  const int kExpectedNetError = 0;
  const bool kExpectedHttps = true;
  const bool kExpectedForceDiscard = false;
  constexpr std::string_view kExpectedGuid = "test-guid-123";

  EXPECT_CALL(on_upload_complete_mock,
              Run(kExpectedStatusCode, kExpectedNetError, kExpectedHttps,
                  kExpectedForceDiscard, kExpectedGuid));

  // Run the callback that was captured from the actual call to
  // setOnUploadComplete.
  captured_callback.Run(kExpectedStatusCode, kExpectedNetError, kExpectedHttps,
                        kExpectedForceDiscard, kExpectedGuid);
  client_->ClearMockUploaderPtr();
}

TEST_F(CobaltMetricsServiceClientTest,
       GetStandardUploadIntervalReturnsDefaultOrSetValue) {
  EXPECT_EQ(kStandardUploadIntervalMinutes,
            client_->GetStandardUploadInterval());

  const base::TimeDelta new_interval = base::Minutes(23);
  client_->SetUploadInterval(new_interval);
  EXPECT_EQ(new_interval, client_->GetStandardUploadInterval());
}

TEST_F(CobaltMetricsServiceClientTest,
       SetMetricsListenerDelegatesToLogUploader) {
  mojo::PendingRemote<h5vcc_metrics::mojom::MetricsListener> listener_remote;

  EXPECT_CALL(*(client_->mock_log_uploader()), SetMetricsListener(_));

  client_->SetMetricsListener(std::move(listener_remote));
}

TEST_F(CobaltMetricsServiceClientTest, IdleTimerStartsOnInitialization) {
  // Initialization is done in SetUp().
  EXPECT_TRUE(client_->idle_refresh_timer().IsRunning());

  base::TimeDelta expected_delay = kStandardUploadIntervalMinutes / 2;
  if (expected_delay < kMinIdleRefreshInterval) {
    expected_delay = kMinIdleRefreshInterval;
  }
  EXPECT_EQ(expected_delay, client_->idle_refresh_timer().GetCurrentDelay());
}

TEST_F(CobaltMetricsServiceClientTest,
       IdleTimerRestartsOnSetUploadIntervalWithCorrectDelay) {
  const base::TimeDelta new_upload_interval = base::Minutes(50);
  client_->SetUploadInterval(new_upload_interval);

  EXPECT_TRUE(client_->idle_refresh_timer().IsRunning());
  base::TimeDelta expected_delay = new_upload_interval / 2;  // 25 minutes
  ASSERT_GT(expected_delay, kMinIdleRefreshInterval);
  EXPECT_EQ(expected_delay, client_->idle_refresh_timer().GetCurrentDelay());
}

TEST_F(CobaltMetricsServiceClientTest,
       IdleTimerDelayIsLimitedByMinIdleRefreshInterval) {
  // New interval is 40 seconds. Half of it is 20 seconds.
  const base::TimeDelta new_upload_interval = base::Seconds(40);
  client_->SetUploadInterval(new_upload_interval);

  EXPECT_TRUE(client_->idle_refresh_timer().IsRunning());
  // Expected delay should be kMinIdleRefreshInterval (30s) because 20s < 30s.
  EXPECT_EQ(kMinIdleRefreshInterval,
            client_->idle_refresh_timer().GetCurrentDelay());
}

TEST_F(CobaltMetricsServiceClientTest,
       IdleTimerCallbackInvokesOnApplicationNotIdleInternal) {
  client_->ResetOnApplicationNotIdleInternalCalled();  // Ensure clean state.

  // Use a known interval that results in a delay > kMinIdleRefreshInterval.
  const base::TimeDelta test_upload_interval = base::Minutes(2);  // 120s
  client_->SetUploadInterval(test_upload_interval);
  base::TimeDelta current_delay =
      client_->idle_refresh_timer().GetCurrentDelay();
  EXPECT_EQ(base::Minutes(1), current_delay);  // Half of 2 minutes.

  EXPECT_FALSE(client_->GetOnApplicationNotIdleInternalCalled());

  task_environment_.FastForwardBy(current_delay);
  EXPECT_TRUE(client_->GetOnApplicationNotIdleInternalCalled());
}

TEST_F(CobaltMetricsServiceClientTest, IdleTimerIsRepeatingAndKeepsFiring) {
  client_->ResetOnApplicationNotIdleInternalCalled();

  const base::TimeDelta test_upload_interval =
      base::Seconds(80);  // Results in 40s delay.
  client_->SetUploadInterval(test_upload_interval);
  base::TimeDelta current_delay =
      client_->idle_refresh_timer().GetCurrentDelay();
  EXPECT_EQ(base::Seconds(40), current_delay);  // 80s / 2 = 40s > 30s.

  EXPECT_FALSE(client_->GetOnApplicationNotIdleInternalCalled());

  // First fire.
  task_environment_.FastForwardBy(current_delay);
  EXPECT_TRUE(client_->GetOnApplicationNotIdleInternalCalled());

  client_->ResetOnApplicationNotIdleInternalCalled();  // Reset for next fire.
  EXPECT_FALSE(client_->GetOnApplicationNotIdleInternalCalled());

  // Second fire.
  task_environment_.FastForwardBy(current_delay);
  EXPECT_TRUE(client_->GetOnApplicationNotIdleInternalCalled());
}

TEST_F(CobaltMetricsServiceClientTest,
       StartIdleRefreshTimerStopsAndRestartsExistingTimer) {
  // Initial timer started with default interval (15 min delay) in SetUp.
  base::TimeDelta initial_delay =
      client_->idle_refresh_timer().GetCurrentDelay();
  EXPECT_EQ(kStandardUploadIntervalMinutes / 2, initial_delay);

  // Change interval to something different.
  const base::TimeDelta new_upload_interval =
      base::Minutes(10);  // New delay = 5 mins.
  client_->SetUploadInterval(new_upload_interval);

  EXPECT_TRUE(client_->idle_refresh_timer().IsRunning());
  base::TimeDelta new_delay = new_upload_interval / 2;
  ASSERT_GT(new_delay, kMinIdleRefreshInterval);
  EXPECT_EQ(new_delay, client_->idle_refresh_timer().GetCurrentDelay());

  client_->ResetOnApplicationNotIdleInternalCalled();
  EXPECT_FALSE(client_->GetOnApplicationNotIdleInternalCalled());

  // Fast forward by less than the initial_delay but more than or equal to
  // new_delay. If the timer wasn't restarted, it wouldn't fire. If it was
  // restarted, it should fire based on new_delay.
  task_environment_.FastForwardBy(new_delay);
  EXPECT_TRUE(client_->GetOnApplicationNotIdleInternalCalled());
}

TEST_F(CobaltMetricsServiceClientBaseTest, CobaltMetricsIntervalFeatureTest) {
  base::HistogramTester histogram_tester;
  const int kCustomInterval = 10;  // 10 seconds

  // Override feature flag and param.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltMetricsIntervalFeature,
      {{"memory-metrics-interval", base::NumberToString(kCustomInterval)},
       {"cpu-metrics-interval", base::NumberToString(kCustomInterval)}});

  // Re-initialize client to pick up new feature settings if needed,
  // but wait, CobaltMetricsServiceClient::Initialize calls
  // StartMemoryMetricsLogger. In our test SetUp, we already called
  // CallInitialize(). Let's create a fresh client to be sure.
  auto synthetic_trial_registry =
      std::make_unique<variations::SyntheticTrialRegistry>();
  auto custom_client = std::make_unique<TestCobaltMetricsServiceClient>(
      metrics_state_manager_.get(), std::move(synthetic_trial_registry),
      &prefs_);
  custom_client->CallInitialize();

  // Initially, no metrics recorded.
  EXPECT_EQ(0u, histogram_tester
                    .GetAllSamples("Memory.Browser.PrivateMemoryFootprint")
                    .size());
  EXPECT_EQ(
      0u, histogram_tester.GetAllSamples("CPU.Total.UsageInPercentage").size());

  // Fast forward by kCustomInterval - 1 second. Still no metrics.
  task_environment_.FastForwardBy(base::Seconds(kCustomInterval - 1));
  EXPECT_EQ(0u, histogram_tester
                    .GetAllSamples("Memory.Browser.PrivateMemoryFootprint")
                    .size());
  EXPECT_EQ(
      0u, histogram_tester.GetAllSamples("CPU.Total.UsageInPercentage").size());

  // Fast forward by 1 more second. Now metrics should be recorded.
  task_environment_.FastForwardBy(base::Seconds(1));
  base::StatisticsRecorder::ImportProvidedHistogramsSync();
  EXPECT_GE(
      histogram_tester.GetAllSamples("Memory.Browser.PrivateMemoryFootprint")
          .size(),
      1u);
  // CPU metrics should also be recorded.
  EXPECT_GE(
      histogram_tester.GetAllSamples("CPU.Total.UsageInPercentage").size(), 1u);
}

TEST_F(CobaltMetricsServiceClientBaseTest,
       MetricsIntervalDefaultProductionTest) {
  // Verify that the feature is disabled by default.
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(features::kCobaltMetricsIntervalFeature));

  base::HistogramTester histogram_tester;

  // Enable the feature without parameters to test the default param value
  // (300s).
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCobaltMetricsIntervalFeature);

  // Re-initialize client to pick up new feature settings.
  auto synthetic_trial_registry =
      std::make_unique<variations::SyntheticTrialRegistry>();
  auto production_client = std::make_unique<TestCobaltMetricsServiceClient>(
      metrics_state_manager_.get(), std::move(synthetic_trial_registry),
      &prefs_);
  production_client->CallInitialize();

  // Initially, no metrics recorded.
  EXPECT_EQ(0u, histogram_tester
                    .GetAllSamples("Memory.Browser.PrivateMemoryFootprint")
                    .size());
  EXPECT_EQ(
      0u, histogram_tester.GetAllSamples("CPU.Total.UsageInPercentage").size());

  // Fast forward by 299 seconds. Still no metrics.
  task_environment_.FastForwardBy(base::Seconds(299));
  EXPECT_EQ(0u, histogram_tester
                    .GetAllSamples("Memory.Browser.PrivateMemoryFootprint")
                    .size());
  EXPECT_EQ(
      0u, histogram_tester.GetAllSamples("CPU.Total.UsageInPercentage").size());

  // Fast forward by 1 more second (total 300s). Now metrics should be recorded.
  task_environment_.FastForwardBy(base::Seconds(1));
  base::StatisticsRecorder::ImportProvidedHistogramsSync();
  EXPECT_GE(
      histogram_tester.GetAllSamples("Memory.Browser.PrivateMemoryFootprint")
          .size(),
      1u);
  EXPECT_GE(
      histogram_tester.GetAllSamples("CPU.Total.UsageInPercentage").size(), 1u);
}

}  // namespace cobalt
