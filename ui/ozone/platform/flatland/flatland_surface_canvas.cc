// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_surface_canvas.h"

#include <fuchsia/sysmem/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/vsync_provider.h"

namespace ui {

size_t RoundUp(size_t value, size_t alignment) {
  return ((value + alignment - 1) / alignment) * alignment;
}

// How long we want to wait for release-fence from scenic for previous frames.
constexpr base::TimeDelta kFrameReleaseTimeout = base::Milliseconds(500);

FlatlandSurfaceCanvas::Frame::Frame() = default;
FlatlandSurfaceCanvas::Frame::~Frame() = default;

class FlatlandSurfaceCanvas::VSyncProviderImpl : public gfx::VSyncProvider {
 public:
  VSyncProviderImpl() = default;
  ~VSyncProviderImpl() override {}

  base::WeakPtr<VSyncProviderImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetState(base::TimeTicks timebase, base::TimeDelta interval) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    timebase_ = timebase;
    interval_ = interval;

    if (get_params_callback_) {
      std::move(get_params_callback_).Run(timebase_, interval_);
    }
  }

  // gfx::VSyncProvider implementation.
  void GetVSyncParameters(UpdateVSyncCallback callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (timebase_.is_null()) {
      get_params_callback_ = std::move(callback);
      return;
    }

    std::move(callback).Run(timebase_, interval_);
  }

  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!timebase_.is_null()) {
      return false;
    }
    *timebase = timebase_;
    *interval = interval_;
    return true;
  }

  bool SupportGetVSyncParametersIfAvailable() const override { return true; }
  bool IsHWClock() const override { return false; }

 private:
  base::TimeTicks timebase_;
  base::TimeDelta interval_;
  UpdateVSyncCallback get_params_callback_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<VSyncProviderImpl> weak_ptr_factory_{this};
};

void FlatlandSurfaceCanvas::Frame::InitializeBuffer(
    fuchsia::sysmem::VmoBuffer vmo,
    gfx::Size size,
    size_t stride) {
  size_t buffer_size = stride * size.height();
  base::WritableSharedMemoryRegion memory_region =
      base::WritableSharedMemoryRegion::Deserialize(
          base::subtle::PlatformSharedMemoryRegion::Take(
              std::move(vmo.vmo),
              base::subtle::PlatformSharedMemoryRegion::Mode::kWritable,
              buffer_size, base::UnguessableToken::Create()));
  memory_mapping = memory_region.Map();

  if (!memory_mapping.IsValid()) {
    LOG(WARNING) << "Failed to map memory for FlatlandSurfaceCanvas.";
    Reset();
    return;
  }

  // Initialize `surface`.
  SkSurfaceProps props;
  surface = SkSurface::MakeRasterDirect(
      SkImageInfo::MakeN32Premul(size.width(), size.height()),
      memory_mapping.memory(), stride, &props);
  dirty_region.setRect(gfx::RectToSkIRect(gfx::Rect(size)));
}

void FlatlandSurfaceCanvas::Frame::Reset() {
  memory_mapping = {};
  surface = nullptr;
  dirty_region = {};
}

void FlatlandSurfaceCanvas::Frame::CopyDirtyRegionFrom(const Frame& frame) {
  int stride = surface->width() * SkColorTypeBytesPerPixel(kN32_SkColorType);
  for (SkRegion::Iterator i(dirty_region); !i.done(); i.next()) {
    uint8_t* dst_ptr =
        static_cast<uint8_t*>(memory_mapping.memory()) +
        i.rect().x() * SkColorTypeBytesPerPixel(kN32_SkColorType) +
        i.rect().y() * stride;
    frame.surface->readPixels(
        SkImageInfo::MakeN32Premul(i.rect().width(), i.rect().height()),
        dst_ptr, stride, i.rect().x(), i.rect().y());
  }
  dirty_region.setEmpty();
}

FlatlandSurfaceCanvas::FlatlandSurfaceCanvas(
    fuchsia::sysmem::Allocator_Sync* sysmem_allocator,
    fuchsia::ui::composition::Allocator* flatland_allocator)
    : sysmem_allocator_(sysmem_allocator),
      flatland_allocator_(flatland_allocator),
      flatland_("Chromium FlatlandSurface",
                base::BindOnce(&FlatlandSurfaceCanvas::OnFlatlandError,
                               base::Unretained(this))) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  root_transform_id_ = flatland_.NextTransformId();
  flatland_.flatland()->CreateTransform(root_transform_id_);
  flatland_.flatland()->SetRootTransform(root_transform_id_);
}

FlatlandSurfaceCanvas::~FlatlandSurfaceCanvas() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

mojo::PlatformHandle FlatlandSurfaceCanvas::CreateView() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  fuchsia::ui::views::ViewportCreationToken parent_token;
  fuchsia::ui::views::ViewCreationToken child_token;
  auto status = zx::channel::create(0, &parent_token.value, &child_token.value);
  DCHECK_EQ(status, ZX_OK);

  flatland_.flatland()->CreateView(std::move(child_token),
                                   parent_viewport_watcher_.NewRequest());

  return mojo::PlatformHandle(std::move(parent_token.value));
}

void FlatlandSurfaceCanvas::ResizeCanvas(const gfx::Size& viewport_size,
                                         float scale) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Drop old images if any.
  for (auto& frame : frames_) {
    if (frame.image_id.value) {
      flatland_.flatland()->ReleaseImage(frame.image_id);
      frame.image_id = {};
    }
  }

  viewport_size_ = viewport_size;
  viewport_size_.SetToMax(gfx::Size(1, 1));

  fuchsia::sysmem::BufferCollectionTokenSyncPtr collection_token;
  sysmem_allocator_->AllocateSharedCollection(collection_token.NewRequest());

  fuchsia::sysmem::BufferCollectionTokenSyncPtr collection_token_for_flatland;
  collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                              collection_token_for_flatland.NewRequest());

  collection_token->Sync();

  fuchsia::ui::composition::BufferCollectionExportToken export_token;
  zx_status_t status =
      zx::eventpair::create(0, &export_token.value, &import_token_.value);
  ZX_DCHECK(status == ZX_OK, status);

  fuchsia::ui::composition::RegisterBufferCollectionArgs args;
  args.set_export_token(std::move(export_token));
  args.set_buffer_collection_token(std::move(collection_token_for_flatland));
  args.set_usage(
      fuchsia::ui::composition::RegisterBufferCollectionUsage::DEFAULT);

  flatland_allocator_->RegisterBufferCollection(
      std::move(args),
      [](fuchsia::ui::composition::Allocator_RegisterBufferCollection_Result
             result) {
        if (result.is_err()) {
          LOG(FATAL) << "RegisterBufferCollection failed";
        }
      });

  sysmem_allocator_->BindSharedCollection(std::move(collection_token),
                                          buffer_collection_.NewRequest());

  fuchsia::sysmem::BufferCollectionConstraints constraints;
  constraints.usage.cpu =
      fuchsia::sysmem::cpuUsageRead | fuchsia::sysmem::cpuUsageWrite;
  constraints.min_buffer_count = kNumBuffers;

  constraints.has_buffer_memory_constraints = true;
  constraints.buffer_memory_constraints.ram_domain_supported = true;
  constraints.buffer_memory_constraints.cpu_domain_supported = true;

  constraints.image_format_constraints_count = 1;
  auto& image_constraints = constraints.image_format_constraints[0];
  image_constraints.color_spaces_count = 1;
  image_constraints.color_space[0] = fuchsia::sysmem::ColorSpace{
      .type = fuchsia::sysmem::ColorSpaceType::SRGB};
  image_constraints.pixel_format.type =
      fuchsia::sysmem::PixelFormatType::BGRA32;
  image_constraints.pixel_format.has_format_modifier = true;
  image_constraints.pixel_format.format_modifier.value =
      fuchsia::sysmem::FORMAT_MODIFIER_LINEAR;
  image_constraints.min_coded_width = viewport_size_.width();
  image_constraints.min_coded_height = viewport_size_.height();

  buffer_collection_->SetConstraints(/*has_constraints=*/true,
                                     std::move(constraints));
}

void FlatlandSurfaceCanvas::FinalizeBufferAllocation() {
  int32_t wait_status;
  fuchsia::sysmem::BufferCollectionInfo_2 buffer_info;
  zx_status_t status =
      buffer_collection_->WaitForBuffersAllocated(&wait_status, &buffer_info);
  ZX_LOG_IF(FATAL, status != ZX_OK, status) << "Sysmem connection failed.";

  if (wait_status != ZX_OK) {
    ZX_LOG(WARNING, wait_status) << "WaitForBuffersAllocated";
    return;
  }

  buffer_collection_->Close();
  buffer_collection_.Unbind();

  CHECK_GE(buffer_info.buffer_count, kNumBuffers);
  DCHECK(import_token_.value.is_valid());

  for (size_t i = 0; i < kNumBuffers; ++i) {
    fuchsia::ui::composition::ImageProperties image_properties;
    image_properties.set_size(
        fuchsia::math::SizeU{static_cast<uint32_t>(viewport_size_.width()),
                             static_cast<uint32_t>(viewport_size_.height())});
    frames_[i].image_id = flatland_.NextContentId();

    fuchsia::ui::composition::BufferCollectionImportToken token;
    status = import_token_.Clone(&token);
    ZX_DCHECK(status == ZX_OK, status);

    flatland_.flatland()->CreateImage(frames_[i].image_id, std::move(token), i,
                                      std::move(image_properties));

    // TODO(crbug.com/1330950): We should set SRC blend mode when Chrome has a
    // reliable signal for opaque background.
    flatland_.flatland()->SetImageBlendingFunction(
        frames_[i].image_id, fuchsia::ui::composition::BlendMode::SRC_OVER);
  }
  import_token_.value.reset();

  const fuchsia::sysmem::ImageFormatConstraints& format =
      buffer_info.settings.image_format_constraints;
  size_t stride =
      RoundUp(std::max(format.min_bytes_per_row, viewport_size_.width() * 4U),
              format.bytes_per_row_divisor);

  for (size_t i = 0; i < kNumBuffers; ++i) {
    frames_[i].InitializeBuffer(std::move(buffer_info.buffers[i]),
                                viewport_size_, stride);
  }
}

SkCanvas* FlatlandSurfaceCanvas::GetCanvas() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (buffer_collection_) {
    FinalizeBufferAllocation();
  }

  if (viewport_size_.IsEmpty() || frames_[current_frame_].is_empty()) {
    return nullptr;
  }

  // Wait for the buffer to become available. This call has to be blocking
  // because GetSurface() and PresentCanvas() are synchronous.
  if (frames_[current_frame_].release_fence) {
    auto status = frames_[current_frame_].release_fence.wait_one(
        ZX_EVENT_SIGNALED,
        zx::deadline_after(zx::duration(kFrameReleaseTimeout.InNanoseconds())),
        nullptr);
    if (status == ZX_ERR_TIMED_OUT) {
      // Timeout here indicates that Scenic is most likely broken. If it still
      // works, then in the worst case returning before |release_fence| is
      // signaled will cause screen tearing.
      LOG(WARNING) << "Release fence from previous frame timed out after 500ms";
    } else {
      ZX_CHECK(status == ZX_OK, status);
    }
  }

  return frames_[current_frame_].surface->getCanvas();
}

void FlatlandSurfaceCanvas::PresentCanvas(const gfx::Rect& damage) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Subtract |damage| from the dirty region in the current frame since it's
  // been repainted.
  SkIRect sk_damage = gfx::RectToSkIRect(damage);
  frames_[current_frame_].dirty_region.op(sk_damage, SkRegion::kDifference_Op);

  // Copy dirty region from the previous buffer to make sure the whole frame
  // is up to date.
  int prev_frame =
      current_frame_ == 0 ? (kNumBuffers - 1) : (current_frame_ - 1);
  frames_[current_frame_].CopyDirtyRegionFrom(frames_[prev_frame]);

  // |damage| rect was updated in the current frame. It means that the rect is
  // no longer valid in all other buffers. Add |damage| to |dirty_region| in all
  // buffers except the current one.
  for (size_t i = 0; i < kNumBuffers; ++i) {
    if (i != current_frame_) {
      frames_[i].dirty_region.op(sk_damage, SkRegion::kUnion_Op);
    }
  }

  flatland_.flatland()->SetContent(root_transform_id_,
                                   frames_[current_frame_].image_id);

  // Create release fence for the current buffer or reset it if it already
  // exists.
  if (!frames_[current_frame_].release_fence) {
    auto status = zx::event::create(
        /*options=*/0u, &(frames_[current_frame_].release_fence));
    ZX_CHECK(status == ZX_OK, status);
  } else {
    auto status = frames_[current_frame_].release_fence.signal(
        /*clear_mask=*/ZX_EVENT_SIGNALED, /*set_mask=*/0);
    ZX_CHECK(status == ZX_OK, status);
  }

  // Add release-fence for the Present() call below. The fence is used in
  // GetCanvas() to ensure that we reuse the buffer only after it's released
  // from scenic.
  zx::event release_fence_dup;
  auto status = frames_[current_frame_].release_fence.duplicate(
      ZX_RIGHT_SAME_RIGHTS, &release_fence_dup);
  ZX_CHECK(status == ZX_OK, status);

  fuchsia::ui::composition::PresentArgs present_args;
  present_args.set_requested_presentation_time(0);
  present_args.mutable_release_fences()->push_back(
      std::move(release_fence_dup));
  present_args.set_unsquashable(false);
  flatland_.Present(std::move(present_args),
                    base::BindOnce(&FlatlandSurfaceCanvas::OnFramePresented,
                                   base::Unretained(this)));

  // Move to the next buffer.
  current_frame_ = (current_frame_ + 1) % kNumBuffers;
}

std::unique_ptr<gfx::VSyncProvider>
FlatlandSurfaceCanvas::CreateVSyncProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto result = std::make_unique<VSyncProviderImpl>();
  vsync_provider_ = result->GetWeakPtr();

  return std::move(result);
}

void FlatlandSurfaceCanvas::OnFramePresented(
    base::TimeTicks actual_presentation_time,
    base::TimeDelta future_presentation_interval) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (vsync_provider_) {
    vsync_provider_->SetState(actual_presentation_time,
                              future_presentation_interval);
  }
}

void FlatlandSurfaceCanvas::OnFlatlandError(
    fuchsia::ui::composition::FlatlandError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(ERROR) << "Flatland error: " << static_cast<int>(error);
  base::LogFidlErrorAndExitProcess(FROM_HERE,
                                   "fuchsia::ui::composition::Flatland");
}

}  // namespace ui
