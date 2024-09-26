// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

#include "base/debug/dump_without_crashing.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSurfaceMutableState.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "ui/gl/gl_fence.h"

namespace gpu {

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentation

SharedImageRepresentation::SharedImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* owning_tracker)
    : manager_(manager), backing_(backing), tracker_(owning_tracker) {
  DCHECK(tracker_);
  // TODO(hitawala): Rewrite the reference counting so that
  // SharedImageRepresentation does not need manager and manager attaches to
  // backing in Register().
  if (manager_ && backing_->is_ref_counted()) {
    backing_->AddRef(this);
  }
}

SharedImageRepresentation::~SharedImageRepresentation() {
  // CHECK here as we'll crash later anyway, and this makes it clearer what the
  // error is.
  CHECK(!has_scoped_access_) << "Destroying a SharedImageRepresentation with "
                                "outstanding Scoped*Access objects.";
  if (manager_ && backing_->is_ref_counted()) {
    manager_->OnRepresentationDestroyed(backing_.ExtractAsDangling()->mailbox(),
                                        this);
  }
}

size_t SharedImageRepresentation::NumPlanesExpected() const {
  if (format().PrefersExternalSampler()) {
    return 1;
  }

  return static_cast<size_t>(format().NumberOfPlanes());
}

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageRepresentationBase

std::unique_ptr<GLTextureImageRepresentation::ScopedAccess>
GLTextureImageRepresentationBase::BeginScopedAccess(
    GLenum mode,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to access an uninitialized SharedImage";
    return nullptr;
  }

  if (!BeginAccess(mode))
    return nullptr;

  UpdateClearedStateOnBeginAccess();

  if (mode == kReadAccessMode)
    backing()->OnReadSucceeded();
  else
    backing()->OnWriteSucceeded();

  return std::make_unique<ScopedAccess>(
      base::PassKey<GLTextureImageRepresentationBase>(), this);
}

gpu::TextureBase* GLTextureImageRepresentationBase::GetTextureBase() {
  DCHECK(format().is_single_plane());
  return GetTextureBase(0);
}

bool GLTextureImageRepresentationBase::SupportsMultipleConcurrentReadAccess() {
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// GLTextureImageRepresentation

gpu::TextureBase* GLTextureImageRepresentation::GetTextureBase(
    int plane_index) {
  return GetTexture(plane_index);
}

gles2::Texture* GLTextureImageRepresentation::GetTexture() {
  DCHECK(format().is_single_plane());
  return GetTexture(0);
}

void GLTextureImageRepresentation::UpdateClearedStateOnEndAccess() {
  auto* texture = GetTexture();
  // Operations on the gles2::Texture may have cleared or uncleared it. Make
  // sure this state is reflected back in the SharedImage.
  gfx::Rect cleared_rect = texture->GetLevelClearedRect(texture->target(), 0);
  if (cleared_rect != ClearedRect())
    SetClearedRect(cleared_rect);
}

void GLTextureImageRepresentation::UpdateClearedStateOnBeginAccess() {
  auto* texture = GetTexture();
  // Operations outside of the gles2::Texture may have cleared or uncleared it.
  // Make sure this state is reflected back in gles2::Texture.
  gfx::Rect cleared_rect = ClearedRect();
  if (cleared_rect != texture->GetLevelClearedRect(texture->target(), 0))
    texture->SetLevelClearedRect(texture->target(), 0, cleared_rect);
}

///////////////////////////////////////////////////////////////////////////////
// GLTexturePassthroughImageRepresentation

gpu::TextureBase* GLTexturePassthroughImageRepresentation::GetTextureBase(
    int plane_index) {
  return GetTexturePassthrough(plane_index).get();
}

const scoped_refptr<gles2::TexturePassthrough>&
GLTexturePassthroughImageRepresentation::GetTexturePassthrough() {
  DCHECK(format().is_single_plane());
  return GetTexturePassthrough(0);
}

bool GLTexturePassthroughImageRepresentation::
    NeedsSuspendAccessForDXGIKeyedMutex() const {
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// SkiaImageRepresentation

SkiaImageRepresentation::SkiaImageRepresentation(SharedImageManager* manager,
                                                 SharedImageBacking* backing,
                                                 MemoryTypeTracker* tracker)
    : SharedImageRepresentation(manager, backing, tracker) {}

SkiaImageRepresentation::~SkiaImageRepresentation() = default;

bool SkiaImageRepresentation::SupportsMultipleConcurrentReadAccess() {
  return false;
}

SkiaImageRepresentation::ScopedWriteAccess::ScopedWriteAccess(
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<SkSurface>> surfaces)
    : ScopedAccessBase(representation), surfaces_(std::move(surfaces)) {
  DCHECK(!surfaces_.empty());
}

SkiaImageRepresentation::ScopedWriteAccess::ScopedWriteAccess(
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<SkPromiseImageTexture>> promise_image_textures)
    : ScopedAccessBase(representation),
      promise_image_textures_(std::move(promise_image_textures)) {
  DCHECK(!promise_image_textures_.empty());
#if BUILDFLAG(ENABLE_SKIA_GRAPHITE)
  CHECK(graphite_textures_.empty());
#endif
}

#if BUILDFLAG(ENABLE_SKIA_GRAPHITE)
SkiaImageRepresentation::ScopedWriteAccess::ScopedWriteAccess(
    SkiaImageRepresentation* representation,
    std::vector<skgpu::graphite::BackendTexture> graphite_textures)
    : ScopedAccessBase(representation), graphite_textures_(graphite_textures) {
  CHECK(!graphite_textures_.empty());
  CHECK(promise_image_textures_.empty());
}
#endif

SkiaImageRepresentation::ScopedWriteAccess::~ScopedWriteAccess() {
  // Ensure no one uses `surfaces_` by dropping the reference before calling
  // EndWriteAccess.
  surfaces_.clear();
  representation()->EndWriteAccess();
}

SkiaImageRepresentation::ScopedReadAccess::ScopedReadAccess(
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<SkPromiseImageTexture>> promise_image_textures)
    : ScopedAccessBase(representation),
      promise_image_textures_(std::move(promise_image_textures)) {
  DCHECK(!promise_image_textures_.empty());
#if BUILDFLAG(ENABLE_SKIA_GRAPHITE)
  CHECK(graphite_textures_.empty());
#endif
}

#if BUILDFLAG(ENABLE_SKIA_GRAPHITE)
SkiaImageRepresentation::ScopedReadAccess::ScopedReadAccess(
    SkiaImageRepresentation* representation,
    std::vector<skgpu::graphite::BackendTexture> graphite_textures)
    : ScopedAccessBase(representation), graphite_textures_(graphite_textures) {
  CHECK(!graphite_textures_.empty());
  CHECK(promise_image_textures_.empty());
}
#endif

SkiaImageRepresentation::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess();
}

sk_sp<SkImage> SkiaImageRepresentation::ScopedReadAccess::CreateSkImage(
    GrDirectContext* context,
    SkImage::TextureReleaseProc texture_release_proc,
    SkImage::ReleaseContext release_context) const {
  auto format = representation()->format();
  auto surface_origin = representation()->surface_origin();
  auto sk_color_space =
      representation()->color_space().GetAsFullRangeRGB().ToSkColorSpace();
  if (format.is_single_plane() || format.PrefersExternalSampler()) {
    DCHECK_EQ(static_cast<int>(promise_image_textures_.size()), 1);
    auto alpha_type = representation()->alpha_type();
    auto color_type =
        viz::ToClosestSkColorType(/*gpu_compositing=*/true, format);
    return SkImages::BorrowTextureFrom(
        context, promise_image_texture()->backendTexture(), surface_origin,
        color_type, alpha_type, sk_color_space, texture_release_proc,
        release_context);
  } else {
    DCHECK_EQ(static_cast<int>(promise_image_textures_.size()),
              format.NumberOfPlanes());
    std::array<GrBackendTexture, SkYUVAInfo::kMaxPlanes> yuva_textures;
    // Get the texture per plane.
    for (int plane_index = 0; plane_index < format.NumberOfPlanes();
         plane_index++) {
      yuva_textures[plane_index] =
          promise_image_texture(plane_index)->backendTexture();
    }

    SkISize sk_size = gfx::SizeToSkISize(representation()->size());
    // TODO(crbug.com/828599): This should really default to rec709.
    SkYUVColorSpace yuv_color_space = kRec601_SkYUVColorSpace;
    representation()->color_space().ToSkYUVColorSpace(
        format.MultiplanarBitDepth(), &yuv_color_space);
    SkYUVAInfo yuva_info(sk_size, ToSkYUVAPlaneConfig(format),
                         ToSkYUVASubsampling(format), yuv_color_space);
    GrYUVABackendTextures yuva_backend_textures(yuva_info, yuva_textures.data(),
                                                surface_origin);
    return SkImages::TextureFromYUVATextures(
        context, yuva_backend_textures, sk_color_space, texture_release_proc,
        release_context);
  }
}

sk_sp<SkImage> SkiaImageRepresentation::ScopedReadAccess::CreateSkImageForPlane(
    int plane_index,
    GrDirectContext* context) const {
  auto format = representation()->format();
  DCHECK(format.is_multi_plane());
  DCHECK_EQ(static_cast<int>(promise_image_textures_.size()),
            format.NumberOfPlanes());

  auto surface_origin = representation()->surface_origin();
  auto alpha_type = SkAlphaType::kOpaque_SkAlphaType;
  auto color_type =
      viz::ToClosestSkColorType(/*gpu_compositing=*/true, format, plane_index);
  return SkImages::BorrowTextureFrom(
      context, promise_image_texture(plane_index)->backendTexture(),
      surface_origin, color_type, alpha_type, /*sk_color_space=*/nullptr);
}

///////////////////////////////////////////////////////////////////////////////
// SkiaGaneshImageRepresentation

SkiaGaneshImageRepresentation::SkiaGaneshImageRepresentation(
    GrDirectContext* gr_context,
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker),
      gr_context_(gr_context) {}

SkiaGaneshImageRepresentation::ScopedGaneshWriteAccess::ScopedGaneshWriteAccess(
    base::PassKey<SkiaGaneshImageRepresentation> /* pass_key */,
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<SkSurface>> surfaces,
    std::unique_ptr<GrBackendSurfaceMutableState> end_state)
    : ScopedWriteAccess(representation, std::move(surfaces)),
      end_state_(std::move(end_state)) {
  DCHECK(!surfaces_.empty());
}

SkiaGaneshImageRepresentation::ScopedGaneshWriteAccess::ScopedGaneshWriteAccess(
    base::PassKey<SkiaGaneshImageRepresentation> /* pass_key */,
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<SkPromiseImageTexture>> promise_image_textures,
    std::unique_ptr<GrBackendSurfaceMutableState> end_state)
    : ScopedWriteAccess(representation, std::move(promise_image_textures)),
      end_state_(std::move(end_state)) {
  DCHECK(!promise_image_textures_.empty());
}

SkiaGaneshImageRepresentation::ScopedGaneshWriteAccess::
    ~ScopedGaneshWriteAccess() {
  if (end_state_) {
    NOTREACHED() << "Before ending write access TakeEndState() must be called "
                    "and the result passed to skia to make sure all layout and "
                    "ownership transitions are done.";
  }
}

void SkiaGaneshImageRepresentation::ScopedGaneshWriteAccess::
    ApplyBackendSurfaceEndState() {
  if (!end_state_) {
    return;
  }
  DCHECK(promise_image_textures_.empty() || surfaces_.empty());

  int num_planes = representation()->format().NumberOfPlanes();
  if (!surfaces_.empty()) {
    for (int plane = 0; plane < num_planes; plane++) {
      surface(plane)->flush(/*info=*/{}, end_state_.get());
    }
  }
  if (!promise_image_textures_.empty()) {
    for (int plane = 0; plane < num_planes; plane++) {
      if (!ganesh_representation()->gr_context()->setBackendTextureState(
              promise_image_texture(plane)->backendTexture(), *end_state_)) {
        LOG(ERROR) << "setBackendTextureState() failed for plane: " << plane;
      }
    }
  }
  end_state_ = nullptr;
}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGaneshImageRepresentation::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to write to an uninitialized SharedImage";
    return nullptr;
  }

  std::unique_ptr<GrBackendSurfaceMutableState> end_state;
  if (use_sk_surface) {
    std::vector<sk_sp<SkSurface>> surfaces =
        BeginWriteAccess(final_msaa_count, surface_props, update_rect,
                         begin_semaphores, end_semaphores, &end_state);
    if (surfaces.empty()) {
      LOG(ERROR) << "Unable to initialize SkSurface";
      return nullptr;
    }

    backing()->OnWriteSucceeded();

    return std::make_unique<ScopedGaneshWriteAccess>(
        base::PassKey<SkiaGaneshImageRepresentation>(), this,
        std::move(surfaces), std::move(end_state));
  }
  std::vector<sk_sp<SkPromiseImageTexture>> promise_image_textures =
      BeginWriteAccess(begin_semaphores, end_semaphores, &end_state);
  if (promise_image_textures.empty()) {
    LOG(ERROR) << "Unable to initialize SkPromiseImageTexture";
    return nullptr;
  }

  backing()->OnWriteSucceeded();

  return std::make_unique<ScopedGaneshWriteAccess>(
      base::PassKey<SkiaGaneshImageRepresentation>(), this,
      std::move(promise_image_textures), std::move(end_state));
}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGaneshImageRepresentation::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  return BeginScopedWriteAccess(
      final_msaa_count, surface_props, gfx::Rect(size()), begin_semaphores,
      end_semaphores, allow_uncleared, use_sk_surface);
}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGaneshImageRepresentation::BeginScopedWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  return BeginScopedWriteAccess(
      /*final_msaa_count=*/1,
      SkSurfaceProps(/*flags=*/0, kUnknown_SkPixelGeometry), begin_semaphores,
      end_semaphores, allow_uncleared, use_sk_surface);
}

SkiaGaneshImageRepresentation::ScopedGaneshReadAccess::ScopedGaneshReadAccess(
    base::PassKey<SkiaGaneshImageRepresentation> /* pass_key */,
    SkiaImageRepresentation* representation,
    std::vector<sk_sp<SkPromiseImageTexture>> promise_image_textures,
    std::unique_ptr<GrBackendSurfaceMutableState> end_state)
    : ScopedReadAccess(representation, std::move(promise_image_textures)),
      end_state_(std::move(end_state)) {
  DCHECK(!promise_image_textures_.empty());
}

SkiaGaneshImageRepresentation::ScopedGaneshReadAccess::
    ~ScopedGaneshReadAccess() {
  if (end_state_) {
    NOTREACHED() << "Before ending read access TakeEndState() must be called "
                    "and the result passed to skia to make sure all layout and "
                    "ownership transitions are done.";
  }
}

bool SkiaGaneshImageRepresentation::ScopedGaneshReadAccess::
    HasBackendSurfaceEndState() {
  return end_state_.get();
}

void SkiaGaneshImageRepresentation::ScopedGaneshReadAccess::
    ApplyBackendSurfaceEndState() {
  if (!end_state_) {
    return;
  }
  for (int plane = 0; plane < representation()->format().NumberOfPlanes();
       plane++) {
    if (!ganesh_representation()->gr_context()->setBackendTextureState(
            promise_image_texture(plane)->backendTexture(), *end_state_)) {
      LOG(ERROR) << "setBackendTextureState() failed for plane: " << plane;
    }
  }
  end_state_ = nullptr;
}

std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
SkiaGaneshImageRepresentation::BeginScopedReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (!IsCleared()) {
    auto cr = ClearedRect();
    LOG(ERROR) << base::StringPrintf(
        "Attempt to read from an uninitialized SharedImage. "
        "Initialized region: (%d, %d, %d, %d) Size: (%d, %d)",
        cr.x(), cr.y(), cr.width(), cr.height(), size().width(),
        size().height());
    return nullptr;
  }

  std::unique_ptr<GrBackendSurfaceMutableState> end_state;
  std::vector<sk_sp<SkPromiseImageTexture>> promise_image_textures =
      BeginReadAccess(begin_semaphores, end_semaphores, &end_state);
  if (promise_image_textures.empty()) {
    LOG(ERROR) << "Unable to initialize SkPromiseImageTexture";
    return nullptr;
  }

  backing()->OnReadSucceeded();

  return std::make_unique<ScopedGaneshReadAccess>(
      base::PassKey<SkiaGaneshImageRepresentation>(), this,
      std::move(promise_image_textures), std::move(end_state));
}

///////////////////////////////////////////////////////////////////////////////
// SkiaGraphiteImageRepresentation

#if BUILDFLAG(ENABLE_SKIA_GRAPHITE)
SkiaGraphiteImageRepresentation::SkiaGraphiteImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SkiaImageRepresentation(manager, backing, tracker) {}

SkiaGraphiteImageRepresentation::ScopedGraphiteWriteAccess::
    ScopedGraphiteWriteAccess(
        base::PassKey<SkiaGraphiteImageRepresentation> /* pass_key */,
        SkiaImageRepresentation* representation,
        std::vector<sk_sp<SkSurface>> surfaces)
    : ScopedWriteAccess(representation, std::move(surfaces)) {
  CHECK(!surfaces_.empty());
}

SkiaGraphiteImageRepresentation::ScopedGraphiteWriteAccess::
    ScopedGraphiteWriteAccess(
        base::PassKey<SkiaGraphiteImageRepresentation> /* pass_key */,
        SkiaImageRepresentation* representation,
        std::vector<skgpu::graphite::BackendTexture> backend_textures)
    : ScopedWriteAccess(representation, backend_textures) {
  CHECK(!graphite_textures_.empty());
}

SkiaGraphiteImageRepresentation::ScopedGraphiteWriteAccess::
    ~ScopedGraphiteWriteAccess() = default;

// Graphite-Dawn backend handles Vulkan transitions by itself, so nothing to do
// here.
void SkiaGraphiteImageRepresentation::ScopedGraphiteWriteAccess::
    ApplyBackendSurfaceEndState() {}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGraphiteImageRepresentation::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const gfx::Rect& update_rect,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to write to an uninitialized SharedImage";
    return nullptr;
  }

  if (use_sk_surface) {
    std::vector<sk_sp<SkSurface>> surfaces =
        BeginWriteAccess(surface_props, update_rect);
    if (surfaces.empty()) {
      LOG(ERROR) << "Unable to initialize SkSurface";
      return nullptr;
    }

    backing()->OnWriteSucceeded();

    return std::make_unique<ScopedGraphiteWriteAccess>(
        base::PassKey<SkiaGraphiteImageRepresentation>(), this,
        std::move(surfaces));
  }
  std::vector<skgpu::graphite::BackendTexture> graphite_textures =
      BeginWriteAccess();
  if (graphite_textures.empty()) {
    LOG(ERROR) << "Unable to initialize graphite::BackendTextures";
    return nullptr;
  }

  backing()->OnWriteSucceeded();

  return std::make_unique<ScopedGraphiteWriteAccess>(
      base::PassKey<SkiaGraphiteImageRepresentation>(), this,
      graphite_textures);
}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGraphiteImageRepresentation::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  return BeginScopedWriteAccess(
      final_msaa_count, surface_props, gfx::Rect(size()), begin_semaphores,
      end_semaphores, allow_uncleared, use_sk_surface);
}

std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
SkiaGraphiteImageRepresentation::BeginScopedWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared,
    bool use_sk_surface) {
  return BeginScopedWriteAccess(
      /*final_msaa_count=*/1,
      SkSurfaceProps(/*flags=*/0, kUnknown_SkPixelGeometry), begin_semaphores,
      end_semaphores, allow_uncleared, use_sk_surface);
}

SkiaGraphiteImageRepresentation::ScopedGraphiteReadAccess::
    ScopedGraphiteReadAccess(
        base::PassKey<SkiaGraphiteImageRepresentation> /* pass_key */,
        SkiaImageRepresentation* representation,
        std::vector<skgpu::graphite::BackendTexture> graphite_textures)
    : ScopedReadAccess(representation, graphite_textures) {
  CHECK(!graphite_textures_.empty());
}

SkiaGraphiteImageRepresentation::ScopedGraphiteReadAccess::
    ~ScopedGraphiteReadAccess() = default;

bool SkiaGraphiteImageRepresentation::ScopedGraphiteReadAccess::
    HasBackendSurfaceEndState() {
  return false;
}

// Graphite-Dawn backend handles Vulkan transitions by itself, so nothing to do
// here.
void SkiaGraphiteImageRepresentation::ScopedGraphiteReadAccess::
    ApplyBackendSurfaceEndState() {}

std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess>
SkiaGraphiteImageRepresentation::BeginScopedReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (!IsCleared()) {
    auto cr = ClearedRect();
    LOG(ERROR) << base::StringPrintf(
        "Attempt to read from an uninitialized SharedImage. "
        "Initialized region: (%d, %d, %d, %d) Size: (%d, %d)",
        cr.x(), cr.y(), cr.width(), cr.height(), size().width(),
        size().height());
    return nullptr;
  }

  std::vector<skgpu::graphite::BackendTexture> graphite_textures =
      BeginReadAccess();
  if (graphite_textures.empty()) {
    LOG(ERROR) << "Unable to initialize graphite::BackendTextures";
    return nullptr;
  }

  backing()->OnReadSucceeded();

  return std::make_unique<ScopedGraphiteReadAccess>(
      base::PassKey<SkiaGraphiteImageRepresentation>(), this,
      graphite_textures);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// OverlayImageRepresentation

#if BUILDFLAG(IS_ANDROID)
AHardwareBuffer* OverlayImageRepresentation::GetAHardwareBuffer() {
  NOTREACHED();
  return nullptr;
}
std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
OverlayImageRepresentation::GetAHardwareBufferFenceSync() {
  NOTREACHED();
  return nullptr;
}
#elif BUILDFLAG(IS_OZONE)
scoped_refptr<gfx::NativePixmap> OverlayImageRepresentation::GetNativePixmap() {
  return backing()->GetNativePixmap();
}
#elif BUILDFLAG(IS_WIN)
absl::optional<gl::DCLayerOverlayImage>
OverlayImageRepresentation::GetDCLayerOverlayImage() {
  NOTREACHED();
  return absl::nullopt;
}
#elif BUILDFLAG(IS_APPLE)
gfx::ScopedIOSurface OverlayImageRepresentation::GetIOSurface() const {
  return gfx::ScopedIOSurface();
}
bool OverlayImageRepresentation::IsInUseByWindowServer() const {
  return false;
}
#endif

OverlayImageRepresentation::ScopedReadAccess::ScopedReadAccess(
    base::PassKey<OverlayImageRepresentation> pass_key,
    OverlayImageRepresentation* representation,
    gfx::GpuFenceHandle acquire_fence)
    : ScopedAccessBase(representation),
      acquire_fence_(std::move(acquire_fence)) {}

OverlayImageRepresentation::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess(std::move(release_fence_));
}

std::unique_ptr<OverlayImageRepresentation::ScopedReadAccess>
OverlayImageRepresentation::BeginScopedReadAccess() {
  if (!IsCleared()) {
    LOG(ERROR) << "Attempt to read from an uninitialized SharedImage";
    return nullptr;
  }

  gfx::GpuFenceHandle acquire_fence;
  if (!BeginReadAccess(acquire_fence))
    return nullptr;

  backing()->OnReadSucceeded();

  return std::make_unique<ScopedReadAccess>(
      base::PassKey<OverlayImageRepresentation>(), this,
      std::move(acquire_fence));
}

///////////////////////////////////////////////////////////////////////////////
// DawnImageRepresentation

DawnImageRepresentation::ScopedAccess::ScopedAccess(
    base::PassKey<DawnImageRepresentation> /* pass_key */,
    DawnImageRepresentation* representation,
    WGPUTexture texture)
    : ScopedAccessBase(representation), texture_(texture) {}

DawnImageRepresentation::ScopedAccess::~ScopedAccess() {
  representation()->EndAccess();
}

std::unique_ptr<DawnImageRepresentation::ScopedAccess>
DawnImageRepresentation::BeginScopedAccess(
    WGPUTextureUsage usage,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to access an uninitialized SharedImage";
    return nullptr;
  }

  WGPUTexture texture = BeginAccess(usage);
  if (!texture) {
    LOG(ERROR) << "Error creating WGPUTexture";
    return nullptr;
  }

  if (usage & kWriteUsage) {
    backing()->OnWriteSucceeded();
  } else {
    backing()->OnReadSucceeded();
  }

  return std::make_unique<ScopedAccess>(
      base::PassKey<DawnImageRepresentation>(), this, texture);
}

///////////////////////////////////////////////////////////////////////////////
// SharedImageRepresentationFactoryRef

SharedImageRepresentationFactoryRef::SharedImageRepresentationFactoryRef(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    bool is_primary)
    : SharedImageRepresentation(manager, backing, tracker),
      is_primary_(is_primary) {
  // If this is secondary reference, we need to notify SharedImageBacking that
  // it can significantly outlive its owning SharedImageFactory and can't rely
  // on it for operation.
  if (!is_primary) {
    backing->OnAddSecondaryReference();
  }
}

SharedImageRepresentationFactoryRef::~SharedImageRepresentationFactoryRef() {
  // Only primary refs provide link to the owning SharedImageFactory.
  if (is_primary_) {
    backing()->UnregisterImageFactory();
    backing()->MarkForDestruction();
  }
}

///////////////////////////////////////////////////////////////////////////////
// VaapiImageRepresentation

VaapiImageRepresentation::VaapiImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    VaapiDependencies* vaapi_deps)
    : SharedImageRepresentation(manager, backing, tracker),
      vaapi_deps_(vaapi_deps) {}

VaapiImageRepresentation::~VaapiImageRepresentation() = default;

VaapiImageRepresentation::ScopedWriteAccess::ScopedWriteAccess(
    base::PassKey<VaapiImageRepresentation> /* pass_key */,
    VaapiImageRepresentation* representation)
    : ScopedAccessBase(representation) {}

VaapiImageRepresentation::ScopedWriteAccess::~ScopedWriteAccess() {
  representation()->EndAccess();
}

const media::VASurface*
VaapiImageRepresentation::ScopedWriteAccess::va_surface() {
  return representation()->vaapi_deps_->GetVaSurface();
}

std::unique_ptr<VaapiImageRepresentation::ScopedWriteAccess>
VaapiImageRepresentation::BeginScopedWriteAccess() {
  return std::make_unique<ScopedWriteAccess>(
      base::PassKey<VaapiImageRepresentation>(), this);
}

///////////////////////////////////////////////////////////////////////////////
// MemoryImageRepresentation

MemoryImageRepresentation::ScopedReadAccess::ScopedReadAccess(
    base::PassKey<MemoryImageRepresentation> pass_key,
    MemoryImageRepresentation* representation,
    SkPixmap pixmap)
    : ScopedAccessBase(representation), pixmap_(pixmap) {}

MemoryImageRepresentation::ScopedReadAccess::~ScopedReadAccess() = default;

std::unique_ptr<MemoryImageRepresentation::ScopedReadAccess>
MemoryImageRepresentation::BeginScopedReadAccess() {
  return std::make_unique<ScopedReadAccess>(
      base::PassKey<MemoryImageRepresentation>(), this, BeginReadAccess());
}

///////////////////////////////////////////////////////////////////////////////
// RasterImageRepresentation

RasterImageRepresentation::ScopedReadAccess::ScopedReadAccess(
    base::PassKey<RasterImageRepresentation> pass_key,
    RasterImageRepresentation* representation,
    const cc::PaintOpBuffer* paint_op_buffer,
    const absl::optional<SkColor4f>& clear_color)
    : ScopedAccessBase(representation),
      paint_op_buffer_(paint_op_buffer),
      clear_color_(clear_color) {}

RasterImageRepresentation::ScopedReadAccess::~ScopedReadAccess() {
  representation()->EndReadAccess();
}

RasterImageRepresentation::ScopedWriteAccess::ScopedWriteAccess(
    base::PassKey<RasterImageRepresentation> pass_key,
    RasterImageRepresentation* representation,
    cc::PaintOpBuffer* paint_op_buffer)
    : ScopedAccessBase(representation), paint_op_buffer_(paint_op_buffer) {}

RasterImageRepresentation::ScopedWriteAccess::~ScopedWriteAccess() {
  representation()->EndWriteAccess(std::move(callback_));
}

std::unique_ptr<RasterImageRepresentation::ScopedReadAccess>
RasterImageRepresentation::BeginScopedReadAccess() {
  absl::optional<SkColor4f> clear_color;
  auto* paint_op_buffer = BeginReadAccess(clear_color);
  if (!paint_op_buffer)
    return nullptr;
  return std::make_unique<ScopedReadAccess>(
      base::PassKey<RasterImageRepresentation>(), this, paint_op_buffer,
      clear_color);
}

std::unique_ptr<RasterImageRepresentation::ScopedWriteAccess>
RasterImageRepresentation::BeginScopedWriteAccess(
    scoped_refptr<SharedContextState> context_state,
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const absl::optional<SkColor4f>& clear_color,
    bool visible) {
  return std::make_unique<ScopedWriteAccess>(
      base::PassKey<RasterImageRepresentation>(), this,
      BeginWriteAccess(std::move(context_state), final_msaa_count,
                       surface_props, clear_color, visible));
}

///////////////////////////////////////////////////////////////////////////////
// VideoDecodeImageRepresentation

VideoDecodeImageRepresentation::VideoDecodeImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SharedImageRepresentation(manager, backing, tracker) {}

VideoDecodeImageRepresentation::~VideoDecodeImageRepresentation() = default;

VideoDecodeImageRepresentation::ScopedWriteAccess::ScopedWriteAccess(
    base::PassKey<VideoDecodeImageRepresentation> /* pass_key */,
    VideoDecodeImageRepresentation* representation)
    : ScopedAccessBase(representation) {}

VideoDecodeImageRepresentation::ScopedWriteAccess::~ScopedWriteAccess() {
  representation()->EndWriteAccess();
}

std::unique_ptr<VideoDecodeImageRepresentation::ScopedWriteAccess>
VideoDecodeImageRepresentation::BeginScopedWriteAccess() {
  if (!BeginWriteAccess())
    return nullptr;

  return std::make_unique<ScopedWriteAccess>(
      base::PassKey<VideoDecodeImageRepresentation>(), this);
}

}  // namespace gpu
