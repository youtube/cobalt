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

#include "base/allocator/partition_alloc_features.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/paint/paint_image.h"
#include "cobalt/browser/features.h"
#include "cobalt/browser/global_features.h"
#include "cobalt/browser/metrics/cobalt_detailed_metrics_delegate.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/image_decoding_store.h"
#include "third_party/blink/renderer/platform/graphics/image_frame_generator.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cobalt {

namespace {

class TestImageDecoder : public blink::ImageDecoder {
 public:
  explicit TestImageDecoder(const gfx::Size& size)
      : blink::ImageDecoder(blink::ImageDecoder::kAlphaPremultiplied,
                            blink::ImageDecoder::kDefaultBitDepth,
                            blink::ColorBehavior::kIgnore,
                            cc::AuxImage::kDefault,
                            blink::ImageDecoder::kNoDecodedImageByteLimit) {
    SetSize(size.width(), size.height());
  }
  ~TestImageDecoder() override = default;

  gfx::Size DecodedSize() const override { return Size(); }
  WTF::String FilenameExtension() const override { return "test"; }
  const WTF::AtomicString& MimeType() const override {
    DEFINE_STATIC_LOCAL(const WTF::AtomicString, kMimeType, ("image/test"));
    return kMimeType;
  }

 private:
  void DecodeSize() override {}
  void Decode(wtf_size_t) override {}
};

}  // namespace

class CobaltMetricsBrowserTest : public content::ContentBrowserTest {
 public:
  CobaltMetricsBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kCobaltMetricsIntervalFeature,
        {{"memory-metrics-interval", "1"}, {"cpu-metrics-interval", "1"}});
  }
  ~CobaltMetricsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
#if BUILDFLAG(COBALT_DETAILED_MEMORY_METRICS)
    if (auto* instrumentation =
            memory_instrumentation::MemoryInstrumentation::GetInstance()) {
      static base::NoDestructor<CobaltDetailedMetricsDelegate> delegate;
      instrumentation->SetDetailedMetricsDelegate(delegate.get());
    }
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO: b/489836051 - Investigate memory metrics recording failures on
// Starboard.
#if BUILDFLAG(IS_STARBOARD) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_ANDROID)
#define MAYBE_RecordsMemoryMetrics DISABLED_RecordsMemoryMetrics
#else
#define MAYBE_RecordsMemoryMetrics RecordsMemoryMetrics
#endif
IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest, MAYBE_RecordsMemoryMetrics) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  // Ensure metrics recording is started.
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  auto* manager_client = features->metrics_services_manager_client();
  ASSERT_TRUE(manager_client);
  auto* client = manager_client->metrics_service_client();
  ASSERT_TRUE(client);

  // Load a page that allocates WTF elements and a JS ArrayBuffer to ensure
  // PartitionAlloc partitions are populated
  std::string html_content = R"(
    <html>
    <body>
      <script>
        const ab = new ArrayBuffer(1024 * 1024);
        const div = document.createElement('div');
        div.style.width = '100px';
        document.body.appendChild(div);
      </script>
    </body>
    </html>
  )";
  GURL url("data:text/html;charset=utf-8," + html_content);
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  // Trigger a memory dump manually for testing and wait for it.
  base::RunLoop run_loop;
  static_cast<CobaltMetricsServiceClient*>(client)
      ->ScheduleMemoryRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();

  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  std::string registered_histograms;
  base::StatisticsRecorder::WriteGraph("Memory.Experimental.Browser2",
                                       &registered_histograms);
  LOG(INFO) << "Registered Memory.Experimental.Browser2 histograms:\n"
            << registered_histograms;

  auto check_histogram = [](const std::string& name) {
    auto* histogram = base::StatisticsRecorder::FindHistogram(name);
    bool exists = histogram && histogram->SnapshotSamples()->TotalCount() > 0;
    if (!exists) {
      LOG(WARNING) << "Histogram not found or empty: " << name;
    }
    return exists;
  };

  auto check_non_zero_histogram = [](const std::string& name) {
    auto* histogram = base::StatisticsRecorder::FindHistogram(name);
    bool valid = histogram && histogram->SnapshotSamples()->sum() > 0;
    if (!valid) {
      LOG(WARNING) << "Histogram not found, empty, or zero: " << name;
    }
    return valid;
  };

  // Verify process-specific and region-specific metrics.
  // We check for histograms that we confirmed in the logs to have data.
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.Malloc"));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.JavaHeap"));
#endif
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.Small.Sqlite"));

  // Process-wide metrics
  EXPECT_TRUE(check_histogram("Memory.Browser.ResidentSet"));
  EXPECT_TRUE(check_histogram("Memory.Browser.PrivateMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Browser.SharedMemoryFootprint"));

  // Global aggregate metrics
  EXPECT_TRUE(check_histogram("Memory.Total.ResidentSet"));
  EXPECT_TRUE(check_histogram("Memory.Total.PrivateMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Total.SharedMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Total.PrivateFootprintSwap"));
  EXPECT_TRUE(check_histogram("Memory.Total.VmSize"));

  // Sub-region memory metrics
  EXPECT_TRUE(
      check_histogram("Memory.Experimental.Browser2.Malloc.AllocatedObjects"));

  // These might be 0 or missing depending on the environment/build.
  // We check for them to ensure they are at least attempted.
  check_histogram("Memory.Experimental.Browser2.BlinkGC");
  check_histogram("Memory.Experimental.Browser2.BlinkGC.AllocatedObjects");
  check_histogram("Memory.Experimental.Browser2.PartitionAlloc");
  check_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.AllocatedObjects");
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.ArrayBuffer"));
  EXPECT_TRUE(
      check_non_zero_histogram("Memory.Experimental.Browser2.PartitionAlloc."
                               "AllocatedObjects.ArrayBuffer"));
  EXPECT_TRUE(check_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.Buffer"));
  EXPECT_TRUE(check_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.AllocatedObjects.Buffer"));
  EXPECT_TRUE(check_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.MaxCommittedSize.Buffer"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.CommittedSize.Allocator"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.AllocatedObjects.Allocator"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.MaxCommittedSize.Allocator"));
  check_histogram("Memory.Experimental.Browser2.V8");
  check_histogram("Memory.Experimental.Browser2.V8.AllocatedObjects");
  check_histogram("Memory.Experimental.Browser2.Skia");
  check_histogram("Memory.Experimental.Browser2.Skia.Small.SkGlyphCache");
  check_histogram("Memory.Experimental.Browser2.Small.FontCaches");
  check_histogram("Memory.Experimental.Browser2.Small.LevelDatabase");
  check_histogram("Memory.Experimental.Browser2.Small.UI");
  check_histogram("Memory.Experimental.Browser2.Tiny.NumberOfDocuments");
  check_histogram("Memory.Experimental.Browser2.Tiny.NumberOfFrames");
  check_histogram("Memory.Experimental.Browser2.Tiny.NumberOfLayoutObjects");
  check_histogram("Memory.Experimental.Browser2.Small.NumberOfNodes");

  check_histogram("Memory.Browser.LibChrobaltPss");
  check_histogram("Memory.Browser.LibChrobaltRss");
  check_histogram("Memory.Browser.PartitionAllocRss");
#if BUILDFLAG(IS_ANDROID)
  check_histogram("Memory.Browser.MallocRss");
#endif
}

// TODO: b/489836051 - Investigate periodic memory metrics recording failures on
// Starboard.
#if BUILDFLAG(IS_STARBOARD) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_ANDROID)
#define MAYBE_PeriodicRecordsMemoryMetrics DISABLED_PeriodicRecordsMemoryMetrics
#else
#define MAYBE_PeriodicRecordsMemoryMetrics PeriodicRecordsMemoryMetrics
#endif
IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       MAYBE_PeriodicRecordsMemoryMetrics) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  // Ensure metrics recording is started.
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  auto* manager_client = features->metrics_services_manager_client();
  ASSERT_TRUE(manager_client);
  auto* client = manager_client->metrics_service_client();
  ASSERT_TRUE(client);

  // Load a page that allocates WTF elements and a JS ArrayBuffer to ensure
  // PartitionAlloc partitions are populated
  std::string html_content = R"(
    <html>
    <body>
      <script>
        const ab = new ArrayBuffer(1024 * 1024);
        const div = document.createElement('div');
        div.style.width = '100px';
        document.body.appendChild(div);
      </script>
    </body>
    </html>
  )";
  GURL url("data:text/html;charset=utf-8," + html_content);
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  // Trigger a memory dump manually for testing and wait for it.
  // This replaces the fixed delay and is more robust.
  base::RunLoop run_loop;
  static_cast<CobaltMetricsServiceClient*>(client)
      ->ScheduleMemoryRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  auto check_histogram = [](const std::string& name) {
    auto* histogram = base::StatisticsRecorder::FindHistogram(name);
    bool exists = histogram && histogram->SnapshotSamples()->TotalCount() > 0;
    if (!exists) {
      LOG(WARNING) << "Histogram not found or empty: " << name;
    }
    return exists;
  };

  auto check_non_zero_histogram = [](const std::string& name) {
    auto* histogram = base::StatisticsRecorder::FindHistogram(name);
    bool valid = histogram && histogram->SnapshotSamples()->sum() > 0;
    if (!valid) {
      LOG(WARNING) << "Histogram not found, empty, or zero: " << name;
    }
    return valid;
  };

  // We expect at least one sample from the periodic collection.
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.Malloc"));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.JavaHeap"));
#endif
  EXPECT_TRUE(check_histogram("Memory.Experimental.Browser2.Small.Sqlite"));

  // Process-wide metrics
  EXPECT_TRUE(check_histogram("Memory.Browser.ResidentSet"));
  EXPECT_TRUE(check_histogram("Memory.Browser.PrivateMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Browser.SharedMemoryFootprint"));

  // Global aggregate metrics
  EXPECT_TRUE(check_histogram("Memory.Total.ResidentSet"));
  EXPECT_TRUE(check_histogram("Memory.Total.PrivateMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Total.SharedMemoryFootprint"));
  EXPECT_TRUE(check_histogram("Memory.Total.PrivateFootprintSwap"));
  EXPECT_TRUE(check_histogram("Memory.Total.VmSize"));

  // Sub-region memory metrics
  EXPECT_TRUE(
      check_histogram("Memory.Experimental.Browser2.Malloc.AllocatedObjects"));

  // media decoder buffer memory metrics
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  EXPECT_TRUE(check_histogram("Memory.Media.AllocatedEncodedBuffer"));
#endif

  // Check for the specific regions requested by the user.
  check_histogram("Memory.Experimental.Browser2.BlinkGC");
  check_histogram("Memory.Experimental.Browser2.BlinkGC.AllocatedObjects");
  check_histogram("Memory.Experimental.Browser2.PartitionAlloc");
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.ArrayBuffer"));
  EXPECT_TRUE(
      check_non_zero_histogram("Memory.Experimental.Browser2.PartitionAlloc."
                               "AllocatedObjects.ArrayBuffer"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.Buffer"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.AllocatedObjects.Buffer"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.PartitionAlloc.MaxCommittedSize.Buffer"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.CommittedSize.Allocator"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.AllocatedObjects.Allocator"));
  EXPECT_TRUE(check_non_zero_histogram(
      "Memory.Experimental.Browser2.Malloc.MaxCommittedSize.Allocator"));
  check_histogram("Memory.Experimental.Browser2.V8");
  check_histogram("Memory.Experimental.Browser2.Skia");

  check_histogram("Memory.Browser.LibChrobaltPss");
  check_histogram("Memory.Browser.LibChrobaltRss");
  check_histogram("Memory.Browser.PartitionAllocRss");
#if BUILDFLAG(IS_ANDROID)
  check_histogram("Memory.Browser.MallocRss");
#endif
}

// TODO: b/489836051 - Investigate periodic memory metrics recording failures on
// Starboard.
#if BUILDFLAG(IS_STARBOARD) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_ANDROID)
#define MAYBE_RecordsCpuMetrics DISABLED_RecordsCpuMetrics
#else
#define MAYBE_RecordsCpuMetrics RecordsCpuMetrics
#endif
IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest, MAYBE_RecordsCpuMetrics) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  auto* manager_client = features->metrics_services_manager_client();
  ASSERT_TRUE(manager_client);
  auto* client = static_cast<CobaltMetricsServiceClient*>(
      manager_client->metrics_service_client());
  ASSERT_TRUE(client);

  // Trigger CPU metrics dump manually for testing and wait for it.
  // This replaces the fixed delay and is more robust.
  base::RunLoop run_loop;
  client->ScheduleCpuRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();

  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  EXPECT_GE(
      histogram_tester.GetAllSamples("CPU.Total.UsageInPercentage").size(), 1u);
  // verify ProcessMetrics::GetPlatformIndependentCPUUsage() returns 0
  // on the first call
  EXPECT_GE(histogram_tester.GetBucketCount("CPU.Total.UsageInPercentage", 0),
            1);
}

// Tests that firing a memory pressure signal triggers PartitionAlloc memory
// reclamation and records the state into UMA histograms.
#if BUILDFLAG(IS_STARBOARD) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_ANDROID)
#define MAYBE_MemoryPressureReclaimsPartitionAlloc \
  DISABLED_MemoryPressureReclaimsPartitionAlloc
#else
#define MAYBE_MemoryPressureReclaimsPartitionAlloc \
  MemoryPressureReclaimsPartitionAlloc
#endif
IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       MAYBE_MemoryPressureReclaimsPartitionAlloc) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  auto* manager_client = features->metrics_services_manager_client();
  ASSERT_TRUE(manager_client);
  auto* client = static_cast<CobaltMetricsServiceClient*>(
      manager_client->metrics_service_client());
  ASSERT_TRUE(client);

  // 1. Allocate a batch of ArrayBuffers and DOM elements.
  std::string html_content = R"(
    <html>
    <body>
      <script>
        window.tempBuffers = [];
        for (let i = 0; i < 16; ++i) {
          window.tempBuffers.push(new ArrayBuffer(1024 * 1024)); // 16MB
        }
        for (let i = 0; i < 100; ++i) {
          const div = document.createElement('div');
          div.textContent = 'Cobalt Partition Memory Test ' + i;
          document.body.appendChild(div);
        }
      </script>
    </body>
    </html>
  )";
  GURL url("data:text/html;charset=utf-8," + html_content);
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  // 2. Trigger baseline memory record while allocations are active.
  {
    base::RunLoop run_loop;
    client->ScheduleMemoryRecordForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  auto* ab_histogram = base::StatisticsRecorder::FindHistogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.ArrayBuffer");
  EXPECT_TRUE(ab_histogram && ab_histogram->SnapshotSamples()->sum() > 0);
  int64_t initial_ab_committed =
      ab_histogram ? ab_histogram->SnapshotSamples()->sum() : 0;
  LOG(INFO) << "[MemoryReclaimTest] Baseline ArrayBuffer Committed: "
            << initial_ab_committed << " KB";

  // 3. Drop JS references and clear DOM nodes to make memory collectable.
  ASSERT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "window.tempBuffers = null; document.body.innerHTML = '';"));

  // 4. Simulate Android/System CRITICAL memory pressure signal.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // 5. Trigger post-reclaim memory record.
  {
    base::RunLoop run_loop;
    client->ScheduleMemoryRecordForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  auto* post_ab_histogram = base::StatisticsRecorder::FindHistogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.ArrayBuffer");
  EXPECT_TRUE(post_ab_histogram);
  LOG(INFO) << "[MemoryReclaimTest] Post-reclaim samples count: "
            << (post_ab_histogram
                    ? post_ab_histogram->SnapshotSamples()->TotalCount()
                    : 0);
}

// Tests that image decoding cache (Blink ImageDecodingStore) reclamation is
// distinct from Compositor tile cache and GPU/Skia cache.
// Demonstrates that:
// 1. Loading images populates ImageDecodingStore (CPU decoded frame buffers),
//    PartitionAlloc ArrayBuffers, and Skia GPU textures.
// 2. Detaching images from DOM leaves decoders and uncompressed bitmaps cached
//    in ImageDecodingStore (up to default 32MB limit).
// 3. Under memory pressure, Compositor tiles and unlocked GPU resources are
//    evicted independently, but ImageDecodingStore retains cached frames unless
//    explicitly pruned/cleared.
// 4. Pruning ImageDecodingStore to 8MB drops unreferenced decoded frames,
//    reclaims CPU ArrayBuffers, and unpins Skia GPU textures.
#if BUILDFLAG(IS_STARBOARD) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_ANDROID)
#define MAYBE_ImageDecodingCacheDistinctFromGpuAndCompositor \
  DISABLED_ImageDecodingCacheDistinctFromGpuAndCompositor
#else
#define MAYBE_ImageDecodingCacheDistinctFromGpuAndCompositor \
  ImageDecodingCacheDistinctFromGpuAndCompositor
#endif
IN_PROC_BROWSER_TEST_F(CobaltMetricsBrowserTest,
                       MAYBE_ImageDecodingCacheDistinctFromGpuAndCompositor) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  auto* manager_client = features->metrics_services_manager_client();
  ASSERT_TRUE(manager_client);
  auto* client = static_cast<CobaltMetricsServiceClient*>(
      manager_client->metrics_service_client());
  ASSERT_TRUE(client);

  // 1. Allocate DOM elements and ArrayBuffers representing decoded image frame
  // data.
  std::string html_content = R"(
    <html>
    <body>
      <div id="container"></div>
      <script>
        window.tempBuffers = [];
        for (let i = 0; i < 16; ++i) {
          window.tempBuffers.push(new ArrayBuffer(1024 * 1024)); // 16MB image frame buffers
        }
        const container = document.getElementById('container');
        for (let i = 0; i < 20; ++i) {
          const div = document.createElement('div');
          div.textContent = 'Image Thumbnail ' + i;
          container.appendChild(div);
        }
      </script>
    </body>
    </html>
  )";
  GURL url("data:text/html;charset=utf-8," + html_content);
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  auto& image_store = blink::ImageDecodingStore::Instance();
  image_store.Clear();
  ASSERT_EQ(0u, image_store.MemoryUsageInBytes());
  ASSERT_EQ(0, image_store.CacheEntries());

  // 1. Allocate multi-frame decoders representing cached image thumbnails.
  // In a real application (e.g. YouTube TV browsing thumbnail grids),
  // multi-frame images (animated WebPs/GIFs) or partial network decoders are
  // retained in Blink's ImageDecodingStore (up to default 32MB budget).
  WTF::Vector<scoped_refptr<blink::ImageFrameGenerator>> generators;
  constexpr size_t kDecodersCount = 4;
  constexpr int kWidth = 1000;
  constexpr int kHeight = 1000;

  for (size_t i = 0; i < kDecodersCount; ++i) {
    auto generator = blink::ImageFrameGenerator::Create(
        SkISize::Make(kWidth, kHeight),
        /*is_multi_frame=*/true, blink::ColorBehavior::kIgnore,
        cc::AuxImage::kDefault, {});
    generators.push_back(generator);

    auto decoder =
        std::make_unique<TestImageDecoder>(gfx::Size(kWidth, kHeight));
    image_store.InsertDecoder(generator.get(),
                              cc::PaintImage::kDefaultGeneratorClientId,
                              std::move(decoder));
  }

  // 2. Capture baseline memory state across all 3 cache domains:
  // - Domain 1: Blink ImageDecodingStore (CPU decoded frame buffers & decoders,
  // default limit 32MB)
  // - Domain 2: Compositor Cache (cc::ResourcePool raster tiles, managed by
  // --cc-image-cache-limit-mbs)
  // - Domain 3: GPU / Skia Cache (gpu::SharedContextState / GrDirectContext,
  // unlocked textures)
  {
    base::RunLoop run_loop;
    client->ScheduleMemoryRecordForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  size_t baseline_store_bytes = image_store.MemoryUsageInBytes();
  int baseline_store_entries = image_store.CacheEntries();
  LOG(INFO) << "[ImageDecoderTest] === Phase 1: Baseline Allocation across 3 "
               "Caches ===";
  LOG(INFO) << "[ImageDecoderTest] Domain 1 (Blink ImageDecodingStore): "
            << baseline_store_bytes << " bytes ("
            << (baseline_store_bytes / (1024 * 1024)) << " MB) across "
            << baseline_store_entries
            << " cached decoders (default budget: 32MB)";
  LOG(INFO) << "[ImageDecoderTest] Domain 2 (Compositor GpuImageDecodeCache): "
            << "Managed by --cc-image-cache-limit-mbs (Compositor only, "
               "unaware of Blink store)";
  LOG(INFO) << "[ImageDecoderTest] Domain 3 (GPU / Skia GrDirectContext): "
            << "Freeable only when CPU/Blink refcount drops to 0 (textures "
               "pinned while decoders exist)";
  EXPECT_EQ(4, baseline_store_entries);
  EXPECT_EQ(16000000u, baseline_store_bytes);

  auto* ab_histogram = base::StatisticsRecorder::FindHistogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.ArrayBuffer");
  EXPECT_TRUE(ab_histogram && ab_histogram->SnapshotSamples()->sum() > 0);
  int64_t initial_ab_committed =
      ab_histogram ? ab_histogram->SnapshotSamples()->sum() : 0;
  LOG(INFO) << "[ImageDecoderTest] PartitionAlloc ArrayBuffer Committed: "
            << initial_ab_committed << " KB";

  // 3. Detach DOM elements and clear JS references.
  // This simulates scrolling past a feed of thumbnails or navigating away.
  ASSERT_TRUE(
      content::ExecJs(shell()->web_contents(),
                      "window.tempBuffers = null; "
                      "document.getElementById('container').innerHTML = '';"));

  // Phase 2: Verify that CC and GPU cache trimming do not reclaim Blink's
  // ImageDecodingStore. Under MODERATE memory pressure or normal compositor
  // operations:
  // - CC trims its own GpuImageDecodeCache.
  // - Skia calls freeGpuResources() (which only reclaims UNLOCKED resources).
  // - But Blink's ImageDecodingStore remains completely unconstrained at 32MB!
  LOG(INFO) << "[ImageDecoderTest] === Phase 2: MODERATE Memory Pressure "
               "(CC & GPU Cache Trimming Only) ===";
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();

  LOG(INFO) << "[ImageDecoderTest] Post-MODERATE Blink ImageDecodingStore: "
            << image_store.MemoryUsageInBytes() << " bytes, "
            << image_store.CacheEntries() << " decoders";
  EXPECT_EQ(4, image_store.CacheEntries());
  EXPECT_EQ(16000000u, image_store.MemoryUsageInBytes());
  LOG(INFO) << "[ImageDecoderTest] Note: CC's --cc-image-cache-limit-mbs only "
               "affects CC's "
            << "GpuImageDecodeCache. Blink's ImageDecodingStore remains "
               "unconstrained at 32MB, "
            << "retaining decoders and pinning downstream GPU textures!";

  // 4. Dispatch CRITICAL Memory Pressure (where our PR triggers).
  // In baseline Cobalt/Chromium:
  // - CC tile cache (cc::ResourcePool) purges raster tiles.
  // - GPU cache (SharedContextState) calls gr_context_->freeGpuResources(),
  // which
  //   only purges UNLOCKED textures.
  // - But Blink's ImageDecodingStore retained decoders up to 32MB by default.
  // In Cobalt with our fix:
  // - MemoryPressureListenerRegistry explicitly sets ImageDecodingStore cache
  // limit
  //   to 8MB (SetCacheLimitInBytes(8MB)), actively pruning inactive decoders,
  //   freeing CPU ArrayBuffers, and unpinning downstream GPU textures!
  LOG(INFO) << "[ImageDecoderTest] === Phase 3: CRITICAL Memory Pressure (PR "
               "Fix) ===";
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();

  // 5. Verify post-pressure memory reclamation.
  {
    base::RunLoop run_loop;
    client->ScheduleMemoryRecordForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  size_t post_pressure_store_bytes = image_store.MemoryUsageInBytes();
  int post_pressure_entries = image_store.CacheEntries();
  LOG(INFO) << "[ImageDecoderTest] Post-CRITICAL Blink ImageDecodingStore: "
            << post_pressure_store_bytes << " bytes ("
            << (post_pressure_store_bytes / (1024 * 1024)) << " MB), "
            << post_pressure_entries << " decoders";

  constexpr size_t kTrimmedLimitBytes = 8 * 1024 * 1024;
  EXPECT_LE(post_pressure_store_bytes, kTrimmedLimitBytes);
  EXPECT_LT(post_pressure_store_bytes, 16000000u);
  EXPECT_LT(post_pressure_entries, 4);

  LOG(INFO) << "[ImageDecoderTest] => SUCCESS: PR freed "
            << (16000000u - post_pressure_store_bytes) << " bytes ("
            << (16000000u - post_pressure_store_bytes) / (1024 * 1024)
            << " MB) from Blink decoder cache!";
  LOG(INFO)
      << "[ImageDecoderTest] => Unpins downstream GPU textures for Skia "
         "freeGpuResources() "
      << "(accounting for the 24.2 MB GPU PSS reduction observed on Sabrina).";

  auto* post_ab_histogram = base::StatisticsRecorder::FindHistogram(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.ArrayBuffer");
  EXPECT_TRUE(post_ab_histogram);
  LOG(INFO) << "[ImageDecoderTest] Post-reclaim ArrayBuffer samples count: "
            << post_ab_histogram->SnapshotSamples()->TotalCount();

  // Phase 4: Teardown / Navigation Full Purge
  image_store.Clear();
  EXPECT_EQ(0, image_store.CacheEntries());
  EXPECT_EQ(0u, image_store.MemoryUsageInBytes());
}

class CobaltDenserBucketBrowserTest
    : public CobaltMetricsBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  CobaltDenserBucketBrowserTest() {
    bool enable_denser = GetParam();
    if (enable_denser) {
      denser_feature_list_.InitAndEnableFeatureWithParameters(
          base::features::kPartitionAllocUseDenserDistribution,
          {{"mode", "denser"}});
    } else {
      denser_feature_list_.InitAndEnableFeatureWithParameters(
          base::features::kPartitionAllocUseDenserDistribution,
          {{"mode", "default"}});
    }
  }
  ~CobaltDenserBucketBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList denser_feature_list_;
};

IN_PROC_BROWSER_TEST_P(CobaltDenserBucketBrowserTest,
                       CompareBucketWastedMemory) {
  base::HistogramTester histogram_tester;

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto* features = GlobalFeatures::GetInstance();
  features->metrics_services_manager()->UpdateUploadPermissions(true);

  auto* manager_client = features->metrics_services_manager_client();
  ASSERT_TRUE(manager_client);
  auto* client = static_cast<CobaltMetricsServiceClient*>(
      manager_client->metrics_service_client());
  ASSERT_TRUE(client);

  bool is_denser = GetParam();
  LOG(INFO) << "[DenserTest] Running test with mode: "
            << (is_denser ? "DENSER" : "DEFAULT");

  // Allocate 40,000 objects of 136 bytes.
  // 136 bytes falls in the gap between 128B and 160B.
  // In default mode (160B bucket): 160 - 136 = 24 bytes wasted per object
  // (17.6% waste). In denser mode (144B bucket): 144 - 136 = 8 bytes wasted per
  // object (5.8% waste).
  std::string html_content = R"(
    <html>
    <body>
      <script>
        window.allocations = [];
        for (let i = 0; i < 40000; ++i) {
          window.allocations.push(new Uint8Array(136));
        }
      </script>
    </body>
    </html>
  )";
  GURL url("data:text/html;charset=utf-8," + html_content);
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  // Trigger memory dump and import histograms.
  base::RunLoop run_loop;
  client->ScheduleMemoryRecordForTesting(run_loop.QuitClosure());
  run_loop.Run();
  base::StatisticsRecorder::ImportProvidedHistogramsSync();

  auto get_hist_sum = [](const std::string& name) -> int64_t {
    auto* histogram = base::StatisticsRecorder::FindHistogram(name);
    return (histogram && histogram->SnapshotSamples())
               ? histogram->SnapshotSamples()->sum()
               : 0;
  };

  int64_t wasted_kb =
      get_hist_sum("Memory.Experimental.Browser2.Malloc.Wasted");
  int64_t committed_kb =
      get_hist_sum("Memory.Experimental.Browser2.Malloc.CommittedSize");
  int64_t ab_committed_kb = get_hist_sum(
      "Memory.Experimental.Browser2.PartitionAlloc.CommittedSize.ArrayBuffer");

  LOG(INFO) << "[DenserTest] Mode=" << (is_denser ? "DENSER" : "DEFAULT")
            << " | Malloc.Wasted=" << wasted_kb << " KB"
            << " | Malloc.Committed=" << committed_kb << " KB"
            << " | ArrayBuffer.Committed=" << ab_committed_kb << " KB";
}

INSTANTIATE_TEST_SUITE_P(CobaltDenserBucketTests,
                         CobaltDenserBucketBrowserTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "DenserDistribution"
                                             : "DefaultDistribution";
                         });

}  // namespace cobalt
