// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mman.h>

#include <vector>

#include "base/bits.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "media/gpu/chromeos/image_processor_factory.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/test/video_test_environment.h"
#include "media/gpu/video_frame_mapper_factory.h"
#include "testing/perf/perf_result_reporter.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/test/gl_surface_test_support.h"

// Only testing for V4L2 devices since we have no devices that need image
// processing and use VA-API.
#if !BUILDFLAG(USE_V4L2_CODEC)
#error "Only V4L2 implemented."
#endif

static constexpr int kMM21TileWidth = 32;
static constexpr int kMM21TileHeight = 16;

static constexpr int kNumberOfTestFrames = 10;
static constexpr int kNumberOfTestCycles = 1000;
static constexpr int kNumberOfCappedTestCycles = 300;

static constexpr int kTestImageWidth = 1920;
static constexpr int kTestImageHeight = 1088;

static constexpr int kRandomFrameSeed = 1000;

static constexpr int kUsecPerFrameAt60fps = 16666;

namespace media {
namespace {

const char* usage_msg =
    "usage: image_processor_perftest\n"
    "[--gtest_help] [--help] [-v=<level>] [--vmodule=<config>] \n";

const char* help_msg =
    "Run the image processor perf tests.\n\n"
    "The following arguments are supported:\n"
    "  --gtest_help          display the gtest help and exit.\n"
    "  --help                display this help and exit.\n"
    "   -v                   enable verbose mode, e.g. -v=2.\n"
    "  --vmodule             enable verbose mode for the specified module.\n";

media::test::VideoTestEnvironment* g_env;

bool SupportsNecessaryGLExtension() {
  scoped_refptr<gl::GLSurface> gl_surface =
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size());
  if (!gl_surface) {
    LOG(ERROR) << "Error creating GL surface";
    return false;
  }
  scoped_refptr<gl::GLContext> gl_context = gl::init::CreateGLContext(
      nullptr, gl_surface.get(), gl::GLContextAttribs());
  if (!gl_context) {
    LOG(ERROR) << "Error creating GL context";
    return false;
  }
  ui::ScopedMakeCurrent current_context(gl_context.get(), gl_surface.get());
  if (!current_context.IsContextCurrent()) {
    LOG(ERROR) << "Error making GL context current";
    return false;
  }

  return gl_context->HasExtension("GL_EXT_YUV_target");
}
scoped_refptr<VideoFrame> CreateNV12Frame(const gfx::Size& size,
                                          VideoFrame::StorageType type) {
  const gfx::Rect visible_rect(size);
  constexpr base::TimeDelta kNullTimestamp;
  if (type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    return CreateGpuMemoryBufferVideoFrame(
        VideoPixelFormat::PIXEL_FORMAT_NV12, size, visible_rect, size,
        kNullTimestamp, gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);
  } else if (type == VideoFrame::STORAGE_DMABUFS) {
    return CreatePlatformVideoFrame(VideoPixelFormat::PIXEL_FORMAT_NV12, size,
                                    visible_rect, size, kNullTimestamp,
                                    gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);
  } else {
    DCHECK_EQ(type, VideoFrame::STORAGE_OWNED_MEMORY);
    return VideoFrame::CreateFrame(VideoPixelFormat::PIXEL_FORMAT_NV12, size,
                                   visible_rect, size, kNullTimestamp);
  }
}

scoped_refptr<VideoFrame> CreateRandomMM21Frame(const gfx::Size& size,
                                                VideoFrame::StorageType type) {
  DCHECK_EQ(size.width(), base::bits::AlignUp(size.width(), kMM21TileWidth));
  DCHECK_EQ(size.height(), base::bits::AlignUp(size.height(), kMM21TileHeight));

  scoped_refptr<VideoFrame> frame = CreateNV12Frame(size, type);
  if (!frame) {
    LOG(ERROR) << "Failed to create MM21 frame";
    return nullptr;
  }

  scoped_refptr<VideoFrame> mapped_frame;
  if (type != VideoFrame::STORAGE_OWNED_MEMORY) {
    std::unique_ptr<VideoFrameMapper> frame_mapper =
        VideoFrameMapperFactory::CreateMapper(
            VideoPixelFormat::PIXEL_FORMAT_NV12, type,
            /*force_linear_buffer_mapper=*/true);
    mapped_frame = frame_mapper->Map(frame, PROT_READ | PROT_WRITE);
    if (!mapped_frame) {
      LOG(ERROR) << "Unable to map MM21 frame";
      return nullptr;
    }
  } else {
    mapped_frame = frame;
  }

  uint8_t* y_plane = mapped_frame->GetWritableVisibleData(VideoFrame::kYPlane);
  uint8_t* uv_plane =
      mapped_frame->GetWritableVisibleData(VideoFrame::kUVPlane);
  const auto y_plane_stride = mapped_frame->stride(VideoFrame::kYPlane);
  for (int row = 0; row < size.height(); row++, y_plane += y_plane_stride) {
    base::RandBytes(y_plane, size.width());
  }

  const auto uv_plane_stride = mapped_frame->stride(VideoFrame::kUVPlane);
  for (int row = 0; row < size.height() / 2;
       row++, uv_plane += uv_plane_stride) {
    base::RandBytes(uv_plane, size.width());
  }

  return frame;
}

class ImageProcessorPerfTest : public ::testing::Test {
 public:
  void InitializeImageProcessorTest(bool use_cpu_memory) {
    test_image_size_.SetSize(kTestImageWidth, kTestImageHeight);
    ASSERT_TRUE((test_image_size_.width() % kMM21TileWidth) == 0);
    ASSERT_TRUE((test_image_size_.height() % kMM21TileHeight) == 0);

    test_image_visible_rect_.set_size(test_image_size_);

    candidate_.size = test_image_size_;
    candidates_ = {candidate_};

    scoped_refptr<base::SequencedTaskRunner> client_task_runner_ =
        base::SequencedTaskRunner::GetCurrentDefault();
    base::RepeatingClosure quit_closure_ = run_loop_.QuitClosure();
    bool image_processor_error_ = false;

    input_frames_.reserve(kNumberOfTestFrames);
    for (int i = 0; i < kNumberOfTestFrames; i++) {
      input_frames_.push_back(CreateRandomMM21Frame(
          test_image_size_, use_cpu_memory ? VideoFrame::STORAGE_OWNED_MEMORY
                                           : VideoFrame::STORAGE_DMABUFS));
    }

    output_frame_ = CreateNV12Frame(test_image_size_,
                                    VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
    ASSERT_TRUE(output_frame_) << "Error creating output frame";

    error_cb_ = base::BindRepeating(
        [](scoped_refptr<base::SequencedTaskRunner> client_task_runner_,
           base::RepeatingClosure quit_closure_, bool* image_processor_error_) {
          CHECK(client_task_runner_->RunsTasksInCurrentSequence());
          ASSERT_TRUE(false);
          quit_closure_.Run();
        },
        client_task_runner_, quit_closure_, &image_processor_error_);

    pick_format_cb_ = base::BindRepeating(
        [](const std::vector<Fourcc>&, absl::optional<Fourcc>) {
          return absl::make_optional<Fourcc>(Fourcc::NV12);
        });
  }

  gfx::Size test_image_size_;
  gfx::Rect test_image_visible_rect_;
  ImageProcessor::PixelLayoutCandidate candidate_{Fourcc(Fourcc::MM21),
                                                  gfx::Size()};
  std::vector<ImageProcessor::PixelLayoutCandidate> candidates_;
  base::RunLoop run_loop_;
  std::vector<scoped_refptr<VideoFrame>> input_frames_;
  scoped_refptr<VideoFrame> output_frame_;
  ImageProcessor::ErrorCB error_cb_;
  ImageProcessorFactory::PickFormatCB pick_format_cb_;
};

// Tests GLImageProcessor by feeding in |kNumberOfTestFrames| unique input
// frames looped over |kNumberOfTestCycles| iterations to the GLImageProcessor
// as fast as possible. Will print out elapsed processing time.
TEST_F(ImageProcessorPerfTest, UncappedGLImageProcessorPerfTest) {
  gl::GLSurfaceTestSupport::InitializeOneOffImplementation(
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2),
      /*fallback_to_software_gl=*/false);

  if (!SupportsNecessaryGLExtension()) {
    GTEST_SKIP() << "Skipping GL Backend test, unsupported platform.";
  }

  InitializeImageProcessorTest(/*use_cpu_memory=*/false);

  scoped_refptr<base::SequencedTaskRunner> client_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  base::RepeatingClosure quit_closure = run_loop_.QuitClosure();
  std::unique_ptr<ImageProcessor> gl_image_processor = ImageProcessorFactory::
      CreateGLImageProcessorWithInputCandidatesForTesting(
          candidates_, test_image_visible_rect_, test_image_size_,
          /*num_buffers=*/1, client_task_runner, pick_format_cb_, error_cb_);
  ASSERT_TRUE(gl_image_processor) << "Error creating GLImageProcessor";

  LOG(INFO) << "Running GLImageProcessor Uncapped Perf Test";
  int outstanding_processors = kNumberOfTestCycles;
  for (int num_cycles = outstanding_processors; num_cycles > 0; num_cycles--) {
    ImageProcessor::FrameReadyCB gl_callback =
        base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
          CHECK(client_task_runner->RunsTasksInCurrentSequence());
          if (!(--outstanding_processors)) {
            quit_closure.Run();
          }
        });
    gl_image_processor->Process(input_frames_[num_cycles % kNumberOfTestFrames],
                                output_frame_, std::move(gl_callback));
  }

  auto start_time = base::TimeTicks::Now();
  run_loop_.Run();
  auto end_time = base::TimeTicks::Now();
  base::TimeDelta delta_time = end_time - start_time;
  const double fps = (kNumberOfTestCycles / delta_time.InSeconds());

  perf_test::PerfResultReporter reporter("GLImageProcessor", "Uncapped Test");
  reporter.RegisterImportantMetric(".frames_decoded", "frames");
  reporter.RegisterImportantMetric(".total_duration", "us");
  reporter.RegisterImportantMetric(".frames_per_second", "fps");

  reporter.AddResult(".frames_decoded",
                     static_cast<double>(kNumberOfTestCycles));
  reporter.AddResult(".total_duration",
                     static_cast<double>(delta_time.InMicroseconds()));
  reporter.AddResult(".frames_per_second", fps);
}

// Tests the LibYUV by feeding in |kNumberOfTestFrames| unique input
// frames looped over |kNumberOfTestCycles| iterations to the LibYUV
// as fast as possible. Will print out elapsed processing time.
TEST_F(ImageProcessorPerfTest, UncappedLibYUVPerfTest) {
  InitializeImageProcessorTest(/*use_cpu_memory=*/true);

  scoped_refptr<base::SequencedTaskRunner> client_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  base::RepeatingClosure quit_closure = run_loop_.QuitClosure();

  std::unique_ptr<ImageProcessor> libyuv_image_processor =
      ImageProcessorFactory::
          CreateLibYUVImageProcessorWithInputCandidatesForTesting(
              candidates_, test_image_visible_rect_, test_image_size_,
              /*num_buffers=*/1, client_task_runner, pick_format_cb_,
              error_cb_);
  ASSERT_TRUE(libyuv_image_processor) << "Error creating LibYUV";

  LOG(INFO) << "Running LibYUV Uncapped Perf Test";
  int outstanding_processors = kNumberOfTestCycles;
  for (int num_cycles = outstanding_processors; num_cycles > 0; num_cycles--) {
    ImageProcessor::FrameReadyCB libyuv_callback =
        base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
          CHECK(client_task_runner->RunsTasksInCurrentSequence());
          if (!(--outstanding_processors)) {
            quit_closure.Run();
          }
        });
    libyuv_image_processor->Process(
        input_frames_[num_cycles % kNumberOfTestFrames], output_frame_,
        std::move(libyuv_callback));
  }

  auto start_time = base::TimeTicks::Now();
  run_loop_.Run();
  auto end_time = base::TimeTicks::Now();
  base::TimeDelta delta_time = end_time - start_time;
  const double fps = (kNumberOfTestCycles / delta_time.InSeconds());

  perf_test::PerfResultReporter reporter("LibYUV", "Uncapped Test");
  reporter.RegisterImportantMetric(".frames_decoded", "frames");
  reporter.RegisterImportantMetric(".total_duration", "us");
  reporter.RegisterImportantMetric(".frames_per_second", "fps");

  reporter.AddResult(".frames_decoded",
                     static_cast<double>(kNumberOfTestCycles));
  reporter.AddResult(".total_duration",
                     static_cast<double>(delta_time.InMicroseconds()));
  reporter.AddResult(".frames_per_second", fps);
}

// Tests GLImageProcessor by feeding in |kNumberOfTestFrames| unique input
// frames looped over |kNumberOfCappedTestCycles| iterations to the
// GLImageProcessor at 60fps. Will print out elapsed processing time.
TEST_F(ImageProcessorPerfTest, CappedGLImageProcessorPerfTest) {
  InitializeImageProcessorTest(/*use_cpu_memory=*/false);

  scoped_refptr<base::SequencedTaskRunner> client_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  base::RepeatingClosure quit_closure = run_loop_.QuitClosure();
  std::unique_ptr<ImageProcessor> gl_image_processor = ImageProcessorFactory::
      CreateGLImageProcessorWithInputCandidatesForTesting(
          candidates_, test_image_visible_rect_, test_image_size_,
          /*num_buffers=*/1, client_task_runner, pick_format_cb_, error_cb_);
  ASSERT_TRUE(gl_image_processor) << "Error creating GLImageProcessor";

  LOG(INFO) << "Running GLImageProcessor Capped Perf Test";
  int loop_iterations = kNumberOfCappedTestCycles;
  base::TimeTicks start, end;
  base::RepeatingCallback<void(scoped_refptr<VideoFrame>)>* gl_callback_ptr;
  base::RepeatingCallback gl_callback =
      base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
        CHECK(client_task_runner->RunsTasksInCurrentSequence());

        if (!(--loop_iterations)) {
          quit_closure.Run();
        } else {
          end = base::TimeTicks::Now();
          base::TimeDelta delta_time = end - start;
          if (delta_time.InMicroseconds() < kUsecPerFrameAt60fps) {
            usleep(kUsecPerFrameAt60fps - delta_time.InMicroseconds());
          } else {
            LOG(WARNING) << "Frame detiling was late by "
                         << (delta_time.InMicroseconds() - kUsecPerFrameAt60fps)
                         << "us";
          }
          start = base::TimeTicks::Now();
          gl_image_processor->Process(
              input_frames_[loop_iterations % kNumberOfTestFrames],
              output_frame_, *gl_callback_ptr);
        }
      });

  gl_callback_ptr = &gl_callback;

  gl_image_processor->Process(input_frames_[0], output_frame_,
                              *gl_callback_ptr);

  start = base::TimeTicks::Now();
  auto total_start_time = base::TimeTicks::Now();
  run_loop_.Run();

  auto total_end_time = base::TimeTicks::Now();
  base::TimeDelta total_delta_time = total_end_time - total_start_time;
  const double fps = (kNumberOfCappedTestCycles / total_delta_time.InSeconds());

  perf_test::PerfResultReporter reporter("GLImageProcessor", "Capped Test");
  reporter.RegisterImportantMetric(".frames_decoded", "frames");
  reporter.RegisterImportantMetric(".total_duration", "us");
  reporter.RegisterImportantMetric(".frames_per_second", "fps");

  reporter.AddResult(".frames_decoded",
                     static_cast<double>(kNumberOfCappedTestCycles));
  reporter.AddResult(".total_duration",
                     static_cast<double>(total_delta_time.InMicroseconds()));
  reporter.AddResult(".frames_per_second", fps);
}

// Tests LibYUV by feeding in |kNumberOfTestFrames| unique input
// frames looped over |kNumberOfCappedTestCycles| iterations to the
// LibYUV at 60fps. Will print out elapsed processing time.
TEST_F(ImageProcessorPerfTest, CappedLibYUVPerfTest) {
  InitializeImageProcessorTest(/*use_cpu_memory=*/true);

  scoped_refptr<base::SequencedTaskRunner> client_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  base::RepeatingClosure quit_closure = run_loop_.QuitClosure();
  std::unique_ptr<ImageProcessor> libyuv_image_processor =
      ImageProcessorFactory::
          CreateLibYUVImageProcessorWithInputCandidatesForTesting(
              candidates_, test_image_visible_rect_, test_image_size_,
              /*num_buffers=*/1, client_task_runner, pick_format_cb_,
              error_cb_);
  ASSERT_TRUE(libyuv_image_processor) << "Error creating LibYUV";

  LOG(INFO) << "Running LibYUV Capped Perf Test";
  int loop_iterations = 0;
  base::TimeTicks start, end;
  base::RepeatingCallback<void(scoped_refptr<VideoFrame>)>* libyuv_callback_ptr;
  base::RepeatingCallback libyuv_callback =
      base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
        CHECK(client_task_runner->RunsTasksInCurrentSequence());

        if ((++loop_iterations) >= kNumberOfCappedTestCycles) {
          quit_closure.Run();
        } else {
          end = base::TimeTicks::Now();
          base::TimeDelta delta_time = end - start;
          if (delta_time.InMicroseconds() < kUsecPerFrameAt60fps) {
            usleep(kUsecPerFrameAt60fps - delta_time.InMicroseconds());
          } else {
            LOG(WARNING) << "Frame detiling was late by "
                         << (delta_time.InMicroseconds() - kUsecPerFrameAt60fps)
                         << "us";
          }
          start = base::TimeTicks::Now();
          libyuv_image_processor->Process(
              input_frames_[loop_iterations % kNumberOfTestFrames],
              output_frame_, *libyuv_callback_ptr);
        }
      });

  libyuv_callback_ptr = &libyuv_callback;

  libyuv_image_processor->Process(input_frames_[0], output_frame_,
                                  *libyuv_callback_ptr);

  start = base::TimeTicks::Now();
  auto total_start_time = base::TimeTicks::Now();
  run_loop_.Run();

  auto total_end_time = base::TimeTicks::Now();
  base::TimeDelta total_delta_time = total_end_time - total_start_time;
  const double fps = (kNumberOfCappedTestCycles / total_delta_time.InSeconds());

  perf_test::PerfResultReporter reporter("LibYUV", "Capped Test");
  reporter.RegisterImportantMetric(".frames_decoded", "frames");
  reporter.RegisterImportantMetric(".total_duration", "us");
  reporter.RegisterImportantMetric(".frames_per_second", "fps");

  reporter.AddResult(".frames_decoded",
                     static_cast<double>(kNumberOfCappedTestCycles));
  reporter.AddResult(".total_duration",
                     static_cast<double>(total_delta_time.InMicroseconds()));
  reporter.AddResult(".frames_per_second", fps);
}

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Print the help message if requested. This needs to be done before
  // initializing gtest, to overwrite the default gtest help message.
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  LOG_ASSERT(cmd_line);
  if (cmd_line->HasSwitch("help")) {
    std::cout << media::usage_msg << "\n" << media::help_msg;
    return 0;
  }

  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::usage_msg;
      return EXIT_FAILURE;
    }
  }
  srand(kRandomFrameSeed);
  testing::InitGoogleTest(&argc, argv);

  auto* const test_environment = new media::test::VideoTestEnvironment;
  media::g_env = reinterpret_cast<media::test::VideoTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));
  return RUN_ALL_TESTS();
}
