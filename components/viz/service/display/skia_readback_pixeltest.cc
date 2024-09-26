// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "cc/test/pixel_test.h"
#include "cc/test/pixel_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/common/frame_sinks/blit_request.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/buildflags.h"
#include "components/viz/test/gl_scaler_test_util.h"
#include "components/viz/test/paths.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
namespace {

constexpr float kAvgAbsoluteErrorLimit = 8.f;
constexpr int kMaxAbsoluteErrorLimit = 32;

cc::FuzzyPixelComparator GetDefaultFuzzyPixelComparator() {
  return cc::FuzzyPixelComparator()
      .SetErrorPixelsPercentageLimit(100.f)
      .SetAvgAbsErrorLimit(kAvgAbsoluteErrorLimit)
      .SetAbsErrorLimit(kMaxAbsoluteErrorLimit);
}

base::FilePath GetTestFilePath(const base::FilePath::CharType* basename) {
  base::FilePath test_dir;
  base::PathService::Get(Paths::DIR_TEST_DATA, &test_dir);
  return test_dir.Append(base::FilePath(basename));
}

SharedQuadState* CreateSharedQuadState(AggregatedRenderPass* render_pass,
                                       const gfx::Rect& rect) {
  const gfx::Rect layer_rect = rect;
  const gfx::Rect visible_layer_rect = rect;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), layer_rect, visible_layer_rect,
                       gfx::MaskFilterInfo(), /*clip_rect=*/absl::nullopt,
                       /*are_contents_opaque=*/false, /*opacity=*/1.0f,
                       SkBlendMode::kSrcOver,
                       /*sorting_context_id=*/0);
  return shared_state;
}

base::span<const uint8_t> MakePixelSpan(const SkBitmap& bitmap) {
  return base::make_span(static_cast<const uint8_t*>(bitmap.getPixels()),
                         bitmap.computeByteSize());
}

void DeleteSharedImage(scoped_refptr<ContextProvider> context_provider,
                       gpu::Mailbox mailbox,
                       const gpu::SyncToken& sync_token,
                       bool is_lost) {
  DCHECK(context_provider);
  gpu::SharedImageInterface* sii = context_provider->SharedImageInterface();
  DCHECK(sii);
  sii->DestroySharedImage(sync_token, mailbox);
}

void ReadbackTextureOnGpuThread(gpu::SharedImageManager* shared_image_manager,
                                gpu::SharedContextState* context_state,
                                const gpu::Mailbox& mailbox,
                                const gfx::Size& texture_size,
                                SkColorType color_type,
                                SkBitmap& out_bitmap) {
  DCHECK(color_type == kAlpha_8_SkColorType ||
         color_type == kR8G8_unorm_SkColorType);

  if (!context_state->MakeCurrent(nullptr))
    return;

  auto representation = shared_image_manager->ProduceSkia(
      mailbox, context_state->memory_type_tracker(), context_state);

  SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  auto scoped_write = representation->BeginScopedWriteAccess(
      /*final_msaa_count=*/1, surface_props, &begin_semaphores, &end_semaphores,
      gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);

  auto* surface = scoped_write->surface();

  surface->wait(begin_semaphores.size(), begin_semaphores.data());

  size_t row_bytes =
      texture_size.width() * (color_type == kAlpha_8_SkColorType ? 1 : 2);
  size_t num_bytes = row_bytes * texture_size.height();

  DCHECK_EQ(num_bytes, out_bitmap.computeByteSize());
  DCHECK_EQ(row_bytes, out_bitmap.rowBytes());
  DCHECK_EQ(
      static_cast<size_t>(out_bitmap.width() * out_bitmap.bytesPerPixel()),
      out_bitmap.rowBytes());

  SkPixmap pixmap(SkImageInfo::Make(texture_size.width(), texture_size.height(),
                                    color_type, kUnpremul_SkAlphaType),
                  out_bitmap.getAddr(0, 0), row_bytes);

  bool success = surface->readPixels(pixmap, 0, 0);
  DCHECK(success);

  GrFlushInfo flush_info;
  flush_info.fNumSemaphores = end_semaphores.size();
  flush_info.fSignalSemaphores = end_semaphores.data();

  gpu::AddVulkanCleanupTaskForSkiaFlush(context_state->vk_context_provider(),
                                        &flush_info);

  surface->flush(SkSurface::BackendSurfaceAccess::kNoAccess, flush_info);
}

// Reads back NV12 planes from textures returned in the result.
// Will issue a task to the GPU thread and block on its completion.
// The |texture_size| needs to be passed in explicitly, because if the request
// was issued with an appended BlitRequest, the |result->size()| only describes
// the size of the region that was populated in the caller-provided textures,
// *not* the entire texture.
void ReadbackNV12Planes(TestGpuServiceHolder* gpu_service_holder,
                        CopyOutputResult& result,
                        const gfx::Size& texture_size,
                        SkBitmap& out_luma_plane,
                        SkBitmap& out_chroma_planes) {
  base::WaitableEvent wait;

  // Some shared image implementations don't allow concurrent read/write to
  // a same image. At this point, compositor GPU thread might be reading the
  // image so it's better we issue the readback on the compositor GPU thread to
  // avoid contention.
  gpu_service_holder->ScheduleCompositorGpuTask(base::BindLambdaForTesting(
      [&out_luma_plane, &out_chroma_planes, &result, &wait, &texture_size]() {
        auto* shared_image_manager = TestGpuServiceHolder::GetInstance()
                                         ->gpu_service()
                                         ->shared_image_manager();
        auto* context_state = TestGpuServiceHolder::GetInstance()
                                  ->GetCompositorGpuThreadSharedContextState()
                                  .get();

        ReadbackTextureOnGpuThread(shared_image_manager, context_state,
                                   result.GetTextureResult()->planes[0].mailbox,
                                   texture_size, kAlpha_8_SkColorType,
                                   out_luma_plane);

        ReadbackTextureOnGpuThread(
            shared_image_manager, context_state,
            result.GetTextureResult()->planes[1].mailbox,
            gfx::Size(texture_size.width() / 2, texture_size.height() / 2),
            kR8G8_unorm_SkColorType, out_chroma_planes);

        wait.Signal();
      }));

  wait.Wait();
}

// Generates a sequence of bytes of specified length, using the given pattern.
// The pattern will be repeated in the generated sequence, and does not have to
// fit in the |num_bytes| evenly. For example, for byte pattern A B C and length
// 7, the result will be: A B C A B C A.
std::vector<uint8_t> GeneratePixels(size_t num_bytes,
                                    base::span<const uint8_t> pattern) {
  std::vector<uint8_t> result;
  result.reserve(num_bytes);

  while (result.size() < num_bytes) {
    result.push_back(pattern[result.size() % pattern.size()]);
  }

  return result;
}

}  // namespace

// Superclass providing common functionality to SkiaReadbackPixelTest variants.
class SkiaReadbackPixelTest : public cc::PixelTest {
 public:
  bool ScaleByHalf() const {
    DCHECK(is_initialized_);
    return scale_by_half_;
  }

  gfx::Size GetSourceSize() const {
    DCHECK(is_initialized_);
    return source_size_;
  }

  const SkBitmap& GetSourceBitmap() const {
    DCHECK(is_initialized_);
    return source_bitmap_;
  }

  // Gets the SkBitmap containing expected output corresponding to the
  // source bitmap. The dimensions depend on whether the issued request will be
  // scaled or not.
  const SkBitmap& GetExpectedOutputBitmap() const {
    DCHECK(is_initialized_);
    return expected_bitmap_;
  }

  // In order to test coordinate calculations the tests will issue copy
  // requests for a small region just to the right and below the center of the
  // entire source texture/framebuffer.
  gfx::Rect GetRequestArea() const {
    DCHECK(is_initialized_);
    gfx::Rect result(source_size_.width() / 2, source_size_.height() / 2,
                     source_size_.width() / 4, source_size_.height() / 4);

    if (scale_by_half_) {
      return gfx::ScaleToEnclosingRect(result, 0.5f);
    }

    return result;
  }

  // Force subclasses to override SetUp(). All subclasses should call
  // `SetUpReadbackPixeltest()` from within their `SetUp()` override.
  void SetUp() override = 0;

 protected:
  // Returns filepath for expected output PNG.
  base::FilePath GetExpectedPath() const {
    return GetTestFilePath(
        scale_by_half_ ? FILE_PATH_LITERAL("half_of_one_of_16_color_rects.png")
                       : FILE_PATH_LITERAL("one_of_16_color_rects.png"));
  }

  // All subclasses should call it from within their virtual SetUp() method.
  void SetUpReadbackPixeltest(bool scale_by_half) {
    DCHECK(!is_initialized_);

    SetUpSkiaRenderer(gfx::SurfaceOrigin::kBottomLeft);

    scale_by_half_ = scale_by_half;

    ASSERT_TRUE(cc::ReadPNGFile(
        GetTestFilePath(FILE_PATH_LITERAL("16_color_rects.png")),
        &source_bitmap_));
    source_bitmap_.setImmutable();

    ASSERT_TRUE(cc::ReadPNGFile(GetExpectedPath(), &expected_bitmap_));
    expected_bitmap_.setImmutable();

    source_size_ = gfx::Size(source_bitmap_.width(), source_bitmap_.height());

    is_initialized_ = true;
  }

  // Issues a CopyOutputRequest and blocks until it has completed.
  // The issued request can be configured via a |configure_request| callback.
  std::unique_ptr<CopyOutputResult> IssueCopyOutputRequestAndRender(
      CopyOutputRequest::ResultFormat format,
      CopyOutputRequest::ResultDestination destination,
      base::OnceCallback<void(CopyOutputRequest&)> configure_request) {
    const SkBitmap& bitmap = GetSourceBitmap();

    std::unique_ptr<CopyOutputResult> result;
    {
      auto pass = GenerateRootRenderPass(bitmap);

      base::RunLoop loop;
      auto request = std::make_unique<CopyOutputRequest>(
          format, destination,
          base::BindOnce(
              [](std::unique_ptr<CopyOutputResult>* result_out,
                 base::OnceClosure quit_closure,
                 std::unique_ptr<CopyOutputResult> result_from_copier) {
                *result_out = std::move(result_from_copier);
                std::move(quit_closure).Run();
              },
              &result, loop.QuitClosure()));

      std::move(configure_request).Run(*request);

      pass->copy_requests.push_back(std::move(request));

      AggregatedRenderPassList pass_list;
      SurfaceDamageRectList surface_damage_rect_list;
      pass_list.push_back(std::move(pass));

      renderer_->DrawFrame(
          &pass_list, 1.0f, gfx::Size(bitmap.width(), bitmap.height()),
          gfx::DisplayColorSpaces(), std::move(surface_damage_rect_list));
      // Call SwapBuffersSkipped(), so the renderer can have a chance to release
      // resources.
      renderer_->SwapBuffersSkipped();

      loop.Run();
    }

    return result;
  }

 private:
  // Creates a RenderPass that embeds a single quad containing |bitmap|.
  std::unique_ptr<AggregatedRenderPass> GenerateRootRenderPass(
      const SkBitmap& bitmap) {
    const gfx::Size source_size = gfx::Size(bitmap.width(), bitmap.height());

    SharedImageFormat format =
        (bitmap.info().colorType() == kBGRA_8888_SkColorType)
            ? SinglePlaneFormat::kBGRA_8888
            : SinglePlaneFormat::kRGBA_8888;
    ResourceId resource_id =
        CreateGpuResource(source_size, format, MakePixelSpan(bitmap));

    std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
        cc::SendResourceAndGetChildToParentMap(
            {resource_id}, resource_provider_.get(),
            child_resource_provider_.get(), child_context_provider_.get());
    ResourceId mapped_resource_id = resource_map[resource_id];

    const gfx::Rect output_rect(source_size);
    auto pass = std::make_unique<AggregatedRenderPass>();
    pass->SetNew(AggregatedRenderPassId{1}, output_rect, output_rect,
                 gfx::Transform());

    SharedQuadState* sqs = CreateSharedQuadState(pass.get(), output_rect);

    auto* quad = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
    quad->SetNew(sqs, output_rect, output_rect, /*needs_blending=*/false,
                 mapped_resource_id, gfx::RectF(output_rect), source_size,
                 /*is_premultiplied=*/true, /*nearest_neighbor=*/true,
                 /*force_anti_aliasing_off=*/false);
    return pass;
  }

  // TODO(kylechar): Create an OOP-R style GPU resource with no GL dependencies.
  ResourceId CreateGpuResource(const gfx::Size& size,
                               SharedImageFormat format,
                               base::span<const uint8_t> pixels) {
    gpu::SharedImageInterface* sii =
        child_context_provider_->SharedImageInterface();
    DCHECK(sii);
    gpu::Mailbox mailbox = sii->CreateSharedImage(
        format, size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
        kPremul_SkAlphaType, gpu::SHARED_IMAGE_USAGE_DISPLAY_READ, "TestPixels",
        pixels);
    gpu::SyncToken sync_token = sii->GenUnverifiedSyncToken();

    TransferableResource gl_resource = TransferableResource::MakeGpu(
        mailbox, GL_TEXTURE_2D, sync_token, size, format,
        /*is_overlay_candidate=*/false);
    auto release_callback =
        base::BindOnce(&DeleteSharedImage, child_context_provider_, mailbox);
    return child_resource_provider_->ImportResource(
        gl_resource, std::move(release_callback));
  }

  bool is_initialized_ = false;

  gfx::Size source_size_;
  bool scale_by_half_;
  SkBitmap source_bitmap_;
  SkBitmap expected_bitmap_;
};

class SkiaReadbackPixelTestRGBA : public SkiaReadbackPixelTest,
                                  public testing::WithParamInterface<bool> {
 public:
  CopyOutputResult::Format RequestFormat() const {
    return CopyOutputResult::Format::RGBA;
  }

  // TODO(kylechar): Add parameter to also test RGBA_TEXTURE when it's
  // supported with the Skia readback API.
  CopyOutputResult::Destination RequestDestination() const {
    return CopyOutputResult::Destination::kSystemMemory;
  }

  void SetUp() override {
    SkiaReadbackPixelTest::SetUpReadbackPixeltest(GetParam());
  }
};

// Test that SkiaRenderer RGBA readback works correctly.
TEST_P(SkiaReadbackPixelTestRGBA, ExecutesCopyRequest) {
  // Generates a RenderPass which contains one quad that spans the full output.
  // The quad has our source image, so the framebuffer should contain the source
  // image after DrawFrame().

  const gfx::Rect result_selection = GetRequestArea();

  std::unique_ptr<CopyOutputResult> result = IssueCopyOutputRequestAndRender(
      RequestFormat(), RequestDestination(),
      base::BindLambdaForTesting(
          [this, &result_selection](CopyOutputRequest& request) {
            // Build CopyOutputRequest based on test parameters.
            if (ScaleByHalf()) {
              request.SetUniformScaleRatio(2, 1);
            }

            request.set_result_selection(result_selection);
          }));

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  EXPECT_EQ(result_selection, result->rect());

  // Examine the image in the |result|, and compare it to the baseline PNG file.
  auto scoped_bitmap = result->ScopedAccessSkBitmap();
  auto actual = scoped_bitmap.bitmap();

  base::FilePath expected_path = GetExpectedPath();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRebaselinePixelTests)) {
    EXPECT_TRUE(cc::WritePNGFile(actual, expected_path, false));
  }

  if (!cc::MatchesPNGFile(actual, expected_path, cc::ExactPixelComparator())) {
    LOG(ERROR) << "Entire source: " << cc::GetPNGDataUrl(GetSourceBitmap());
    ADD_FAILURE();
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SkiaReadbackPixelTestRGBA,
                         // Result scaling: Scale by half?
                         testing::Values(true, false));

class SkiaReadbackPixelTestNV12
    : public SkiaReadbackPixelTest,
      public testing::WithParamInterface<
          std::tuple<bool, CopyOutputResult::Destination>> {
 public:
  CopyOutputResult::Destination RequestDestination() const {
    return std::get<1>(GetParam());
  }

  CopyOutputResult::Format RequestFormat() const {
    return CopyOutputResult::Format::NV12_PLANES;
  }

  void SetUp() override {
    SkiaReadbackPixelTest::SetUpReadbackPixeltest(std::get<0>(GetParam()));
  }
};

// Test that SkiaRenderer NV12 readback works correctly.
TEST_P(SkiaReadbackPixelTestNV12, ExecutesCopyRequest) {
  // Generates a RenderPass which contains one quad that spans the full output.
  // The quad has our source image, so the framebuffer should contain the source
  // image after DrawFrame().

  const gfx::Rect result_selection = GetRequestArea();

  // Check if request's width and height are even (required for NV12 format).
  // The test case expects the result size to match the request size exactly,
  // which is not possible with NV12 when the request size dimensions aren't
  // even.
  ASSERT_TRUE(result_selection.width() % 2 == 0 &&
              result_selection.height() % 2 == 0)
      << " request size is not even, result_selection.size()="
      << result_selection.size().ToString();

  // Additionally, the test uses helpers that assume pixel data can be packed (4
  // 8-bit values in 1 32-bit pixel).
  ASSERT_TRUE(result_selection.width() % 4 == 0)
      << " request width is not divisible by 4, result_selection.width()="
      << result_selection.width();

  std::unique_ptr<CopyOutputResult> result = IssueCopyOutputRequestAndRender(
      RequestFormat(), RequestDestination(),
      base::BindLambdaForTesting(
          [this, &result_selection](CopyOutputRequest& request) {
            // Build CopyOutputRequest based on test parameters.
            if (ScaleByHalf()) {
              request.SetUniformScaleRatio(2, 1);
            }

            request.set_result_selection(result_selection);
          }));

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  ASSERT_EQ(result_selection, result->rect());

  // Examine the image in the |result|, and compare it to the baseline PNG file.
  // Approach is the same as the one in GLNV12ConverterPixelTest.

  SkBitmap luma_plane;
  SkBitmap chroma_planes;

  // Packed plane sizes:
  const gfx::Size luma_plane_size =
      gfx::Size(result->size().width() / 4, result->size().height());
  const gfx::Size chroma_planes_size =
      gfx::Size(luma_plane_size.width(), luma_plane_size.height() / 2);

  if (RequestDestination() == CopyOutputResult::Destination::kSystemMemory) {
    // Create a bitmap with packed Y values:
    luma_plane = GLScalerTestUtil::AllocateRGBABitmap(luma_plane_size);
    chroma_planes = GLScalerTestUtil::AllocateRGBABitmap(chroma_planes_size);

    result->ReadNV12Planes(static_cast<uint8_t*>(luma_plane.getAddr(0, 0)),
                           result->size().width(),
                           static_cast<uint8_t*>(chroma_planes.getAddr(0, 0)),
                           result->size().width());
  } else {
    luma_plane = GLScalerTestUtil::AllocateRGBABitmap(luma_plane_size);
    chroma_planes = GLScalerTestUtil::AllocateRGBABitmap(chroma_planes_size);

    ReadbackNV12Planes(gpu_service_holder_, *result, result->size(), luma_plane,
                       chroma_planes);
  }

  // Allocate new bitmap & populate it with Y & UV data.
  SkBitmap actual = GLScalerTestUtil::AllocateRGBABitmap(result->size());
  actual.eraseColor(SkColorSetARGB(0xff, 0x00, 0x00, 0x00));

  GLScalerTestUtil::UnpackPlanarBitmap(luma_plane, 0, &actual);
  GLScalerTestUtil::UnpackUVBitmap(chroma_planes, &actual);

  SkBitmap expected =
      GLScalerTestUtil::CopyAndConvertToRGBA(GetExpectedOutputBitmap());
  GLScalerTestUtil::ConvertRGBABitmapToYUV(&expected);

  EXPECT_TRUE(
      cc::MatchesBitmap(actual, expected, GetDefaultFuzzyPixelComparator()));
}

#if !BUILDFLAG(IS_ANDROID_EMULATOR)
INSTANTIATE_TEST_SUITE_P(
    ,
    SkiaReadbackPixelTestNV12,
    // Result scaling: Scale by half?
    testing::Combine(
        testing::Values(true, false),
        testing::Values(CopyOutputResult::Destination::kSystemMemory,
                        CopyOutputResult::Destination::kNativeTextures)));
#else
// Don't instantiate the NV12 tests when run on Android emulator, they won't
// work since the SkiaRenderer currently does not support CopyOutputRequests
// with NV12 format if the platform does not support GL_EXT_texture_rg extension
// in GL ES 2.0 (which is the case on Android emulator).
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SkiaReadbackPixelTestNV12);
#endif

class SkiaReadbackPixelTestNV12WithBlit
    : public SkiaReadbackPixelTest,
      public testing::WithParamInterface<
          std::tuple<bool, LetterboxingBehavior, bool>> {
 public:
  CopyOutputResult::Destination RequestDestination() const {
    return CopyOutputResult::Destination::kNativeTextures;
  }

  CopyOutputResult::Format RequestFormat() const {
    return CopyOutputResult::Format::NV12_PLANES;
  }

  void SetUp() override {
    SkiaReadbackPixelTest::SetUpReadbackPixeltest(std::get<0>(GetParam()));
  }

  LetterboxingBehavior GetLetterboxingBehavior() const {
    return std::get<1>(GetParam());
  }

  // Test parameter that will return `true` if we'll claim that the textures we
  // create come from GpuMemoryBuffer, `false` otherwise. This exercises a
  // different code path in SkiaRenderer.
  bool populates_gpu_memory_buffer() const { return std::get<2>(GetParam()); }
};

// Test that SkiaRenderer NV12 readback works correctly using existing textures.
TEST_P(SkiaReadbackPixelTestNV12WithBlit, ExecutesCopyRequestWithBlit) {
  const gfx::Rect result_selection = GetRequestArea();

  // Check if request's width and height are even (required for NV12 format).
  // The test case expects the result size to match the request size exactly,
  // which is not possible with NV12 when the request size dimensions aren't
  // even.
  ASSERT_TRUE(result_selection.width() % 2 == 0 &&
              result_selection.height() % 2 == 0)
      << " request size is not even, result_selection.size()="
      << result_selection.size().ToString();

  // Additionally, the test uses helpers that assume pixel data can be packed (4
  // 8-bit values in 1 32-bit pixel).
  ASSERT_TRUE(result_selection.width() % 4 == 0)
      << " request width is not divisible by 4, result_selection.width()="
      << result_selection.width();

  // Generate 2 shared images that will be owned by us. They will be used as the
  // destination for the issued BlitRequest. The logical size of the image will
  // be the same as kSourceSize. The destination region will be the same size of
  // |result_selection| rectangle, with the same center as the center of
  // kSourceSize rectangle. As a consequence, the CopyOutputResult should
  // contain the pixels from the source image in the middle, and the rest should
  // remain unchanged.

  const gfx::Size source_size = GetSourceSize();
  gfx::Rect destination_subregion = gfx::Rect(source_size);
  destination_subregion.ClampToCenteredSize(result_selection.size());

  ASSERT_TRUE(destination_subregion.x() % 2 == 0 &&
              destination_subregion.y() % 2 == 0)
      << " The test case expects the blit region's origin to be even for NV12 "
         "blit requests";

  const SkColor4f yuv_red = {0.245331, 0.399356, 0.939216, 1.0};

  const std::vector<uint8_t> luma_pattern = {
      static_cast<uint8_t>(yuv_red.fR * 255.0f)};
  const std::vector<uint8_t> chromas_pattern = {
      static_cast<uint8_t>(yuv_red.fG * 255.0f),
      static_cast<uint8_t>(yuv_red.fB * 255.0f)};

  std::array<gpu::MailboxHolder, CopyOutputResult::kMaxPlanes> mailboxes;
  for (size_t i = 0; i < CopyOutputResult::kNV12MaxPlanes; ++i) {
    const auto format =
        i == 0 ? SinglePlaneFormat::kR_8 : SinglePlaneFormat::kRG_88;
    const gfx::Size plane_size =
        i == 0 ? source_size
               : gfx::Size(source_size.width() / 2, source_size.height() / 2);
    const size_t plane_size_in_bytes =
        plane_size.GetArea() * (format == SinglePlaneFormat::kR_8 ? 1 : 2);

    std::vector<uint8_t> pixels =
        (i == 0) ? GeneratePixels(plane_size_in_bytes, luma_pattern)
                 : GeneratePixels(plane_size_in_bytes, chromas_pattern);

    mailboxes[i].mailbox =
        child_context_provider_->SharedImageInterface()->CreateSharedImage(
            format, plane_size, gfx::ColorSpace::CreateREC709(),
            kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
            gpu::SHARED_IMAGE_USAGE_DISPLAY_READ, "TestLabels", pixels);
    DCHECK(!mailboxes[i].mailbox.IsZero());
  }

  std::unique_ptr<CopyOutputResult> result = IssueCopyOutputRequestAndRender(
      RequestFormat(), RequestDestination(),
      base::BindLambdaForTesting([this, &result_selection,
                                  &destination_subregion,
                                  &mailboxes](CopyOutputRequest& request) {
        // Build CopyOutputRequest based on test parameters.
        if (ScaleByHalf()) {
          request.SetUniformScaleRatio(2, 1);
        }

        request.set_result_selection(result_selection);

        request.set_blit_request(BlitRequest(
            destination_subregion.origin(), GetLetterboxingBehavior(),
            mailboxes, populates_gpu_memory_buffer()));
      }));

  // Check that a result was produced and is of the expected rect/size.
  ASSERT_TRUE(result);
  ASSERT_FALSE(result->IsEmpty());
  ASSERT_EQ(result_selection, result->rect());
  ASSERT_EQ(result->destination(),
            CopyOutputResult::Destination::kNativeTextures);

  // Packed plane sizes. Note that for blit request, the size of the returned
  // textures is caller-controlled, and we have issued a COR w/ blit request
  // that is supposed to write to an image of |source_size| size.
  const gfx::Size luma_plane_size =
      gfx::Size(source_size.width() / 4, source_size.height());
  const gfx::Size chroma_planes_size =
      gfx::Size(luma_plane_size.width(), luma_plane_size.height() / 2);

  SkBitmap luma_plane = GLScalerTestUtil::AllocateRGBABitmap(luma_plane_size);
  SkBitmap chroma_planes =
      GLScalerTestUtil::AllocateRGBABitmap(chroma_planes_size);

  ReadbackNV12Planes(gpu_service_holder_, *result, source_size, luma_plane,
                     chroma_planes);

  for (size_t i = 0; i < CopyOutputResult::kNV12MaxPlanes; ++i) {
    child_context_provider_->SharedImageInterface()->DestroySharedImage(
        result->GetTextureResult()->planes[i].sync_token,
        result->GetTextureResult()->planes[i].mailbox);
  }

  // Allocate new bitmap & populate it with Y & UV data.
  SkBitmap actual = GLScalerTestUtil::AllocateRGBABitmap(source_size);
  actual.eraseColor(SkColorSetARGB(0xff, 0x00, 0x00, 0x00));

  GLScalerTestUtil::UnpackPlanarBitmap(luma_plane, 0, &actual);
  GLScalerTestUtil::UnpackUVBitmap(chroma_planes, &actual);

  // Load the expected subregion from a file - we will then write it on top of
  // a new, all-red bitmap:
  SkBitmap expected_subregion =
      GLScalerTestUtil::CopyAndConvertToRGBA(GetExpectedOutputBitmap());

  // The textures that we passed in to BlitRequest contained NV12 plane data for
  // an all-red image, let's re-create such a bitmap:
  SkBitmap expected = GLScalerTestUtil::AllocateRGBABitmap(source_size);

  if (GetLetterboxingBehavior() == LetterboxingBehavior::kLetterbox) {
    // We have requested the results to be letterboxed, so everything that
    // CopyOutputRequest is not populating w/ render pass contents should be
    // black:
    expected.eraseColor(SK_ColorBLACK);
  } else {
    // We have requested the results to not be letterboxed, so everything that
    // CopyOutputRequest is not populating w/ render pass will have original
    // contents (red in our case):
    expected.eraseColor(SK_ColorRED);
  }

  // Blit request should "stitch" the pixels from the source image into a
  // sub-region of caller-provided texture - let's write our expected pixels
  // loaded from a file into the same subregion of an all-red texture:
  expected.writePixels(expected_subregion.pixmap(), destination_subregion.x(),
                       destination_subregion.y());

  // Now let's convert it to YUV so we can compare with the result:
  GLScalerTestUtil::ConvertRGBABitmapToYUV(&expected);

  EXPECT_TRUE(
      cc::MatchesBitmap(actual, expected, GetDefaultFuzzyPixelComparator()));
}

#if !BUILDFLAG(IS_ANDROID_EMULATOR)
INSTANTIATE_TEST_SUITE_P(
    ,
    SkiaReadbackPixelTestNV12WithBlit,
    testing::Combine(
        testing::Bool(),  // Result scaling: Scale by half?
        testing::Values(LetterboxingBehavior::kDoNotLetterbox,
                        LetterboxingBehavior::kLetterbox),
        testing::Bool()  // Should behave as if COR is populating a GMB?
        ));
#else
// Don't instantiate the NV12 tests when run on Android emulator, they won't
// work since the SkiaRenderer currently does not support CopyOutputRequests
// with NV12 format if the platform does not support GL_EXT_texture_rg extension
// in GL ES 2.0 (which is the case on Android emulator).
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    SkiaReadbackPixelTestNV12WithBlit);
#endif

}  // namespace viz
