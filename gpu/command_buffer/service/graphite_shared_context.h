// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_SHARED_CONTEXT_H_
#define GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_SHARED_CONTEXT_H_

#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/graphite/GraphiteTypes.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"

class SkTraceMemoryDump;

namespace skgpu ::graphite {
class BackendTexture;
struct InsertRecordingInfo;
class PrecompileContext;
class Recorder;
struct RecorderOptions;
}  // namespace skgpu::graphite

namespace gpu {

// This is a thread safe wrapper class to skgpu::graphite::Context. In order to
// support multi-threading, locks are used to ensure thread safety in
// skgpu::graphite::Context. All clients need to call the wrapper functions in
// GraphiteSharedContext. Only GraphiteSharedContext can communicate with
// skgpu::graphite::Context directly. If |is_thread_safe| is false, the locks
// are equivalent to no-op.
class GPU_GLES2_EXPORT GraphiteSharedContext {
 public:
  using SkImageReadPixelsCallback = base::OnceCallback<
      void(void* ctx, std::unique_ptr<const SkSurface::AsyncReadResult>)>;

  GraphiteSharedContext(
      std::unique_ptr<skgpu::graphite::Context> graphite_context,
      bool is_thread_safe);

  GraphiteSharedContext(const GraphiteSharedContext&) = delete;
  GraphiteSharedContext(GraphiteSharedContext&&) = delete;
  GraphiteSharedContext& operator=(const GraphiteSharedContext&) = delete;
  GraphiteSharedContext& operator=(GraphiteSharedContext&&) = delete;

  ~GraphiteSharedContext();

  bool IsThreadSafe() const { return !!lock_; }

  // Wrapper function implementations for skgpu::graphite:Context
  skgpu::BackendApi backend() const;

  std::unique_ptr<skgpu::graphite::Recorder> makeRecorder(
      const skgpu::graphite::RecorderOptions& = {});

  std::unique_ptr<skgpu::graphite::PrecompileContext> makePrecompileContext();

  bool insertRecording(const skgpu::graphite::InsertRecordingInfo&);
  bool submit(skgpu::graphite::SyncToCpu = skgpu::graphite::SyncToCpu::kNo);

  bool hasUnfinishedGpuWork() const;

  void asyncRescaleAndReadPixels(const SkImage* src,
                                 const SkImageInfo& dstImageInfo,
                                 const SkIRect& srcRect,
                                 SkImage::RescaleGamma rescaleGamma,
                                 SkImage::RescaleMode rescaleMode,
                                 SkImageReadPixelsCallback callback,
                                 SkImage::ReadPixelsContext context);
  void asyncRescaleAndReadPixels(const SkSurface* src,
                                 const SkImageInfo& dstImageInfo,
                                 const SkIRect& srcRect,
                                 SkImage::RescaleGamma rescaleGamma,
                                 SkImage::RescaleMode rescaleMode,
                                 SkImageReadPixelsCallback callback,
                                 SkImage::ReadPixelsContext context);

  void asyncRescaleAndReadPixelsYUV420(const SkImage* src,
                                       SkYUVColorSpace yuvColorSpace,
                                       sk_sp<SkColorSpace> dstColorSpace,
                                       const SkIRect& srcRect,
                                       const SkISize& dstSize,
                                       SkImage::RescaleGamma rescaleGamma,
                                       SkImage::RescaleMode rescaleMode,
                                       SkImageReadPixelsCallback callback,
                                       SkImage::ReadPixelsContext context);
  void asyncRescaleAndReadPixelsYUV420(const SkSurface* src,
                                       SkYUVColorSpace yuvColorSpace,
                                       sk_sp<SkColorSpace> dstColorSpace,
                                       const SkIRect& srcRect,
                                       const SkISize& dstSize,
                                       SkImage::RescaleGamma rescaleGamma,
                                       SkImage::RescaleMode rescaleMode,
                                       SkImageReadPixelsCallback callback,
                                       SkImage::ReadPixelsContext context);

  void asyncRescaleAndReadPixelsYUVA420(const SkImage* src,
                                        SkYUVColorSpace yuvColorSpace,
                                        sk_sp<SkColorSpace> dstColorSpace,
                                        const SkIRect& srcRect,
                                        const SkISize& dstSize,
                                        SkImage::RescaleGamma rescaleGamma,
                                        SkImage::RescaleMode rescaleMode,
                                        SkImageReadPixelsCallback callback,
                                        SkImage::ReadPixelsContext context);
  void asyncRescaleAndReadPixelsYUVA420(const SkSurface* src,
                                        SkYUVColorSpace yuvColorSpace,
                                        sk_sp<SkColorSpace> dstColorSpace,
                                        const SkIRect& srcRect,
                                        const SkISize& dstSize,
                                        SkImage::RescaleGamma rescaleGamma,
                                        SkImage::RescaleMode rescaleMode,
                                        SkImageReadPixelsCallback callback,
                                        SkImage::ReadPixelsContext context);

  void checkAsyncWorkCompletion();

  void deleteBackendTexture(const skgpu::graphite::BackendTexture&);

  void freeGpuResources();

  void performDeferredCleanup(std::chrono::milliseconds msNotUsed);

  // TODO(crbug.com/407874799): Some of the methods below (maybe above as well)
  // may be safe to use without locks. Review and delete unneeded locks in
  // those methods once we start to run with |is_thread_safe| set to true.
  size_t currentBudgetedBytes() const;

  size_t currentPurgeableBytes() const;

  size_t maxBudgetedBytes() const;

  void setMaxBudgetedBytes(size_t bytes);

  void dumpMemoryStatistics(SkTraceMemoryDump* traceMemoryDump) const;

  bool isDeviceLost() const;

  int maxTextureSize() const;

  bool supportsProtectedContent() const;

  skgpu::GpuStatsFlags supportedGpuStats() const;

 private:
  class AutoLock;

  // The lock for protecting skgpu::graphite::Context.
  // Valid only when |is_thread_safe| is set to true in Ctor.
  mutable std::optional<base::Lock> lock_;

  const std::unique_ptr<skgpu::graphite::Context> graphite_context_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GRAPHITE_SHARED_CONTEXT_H_
