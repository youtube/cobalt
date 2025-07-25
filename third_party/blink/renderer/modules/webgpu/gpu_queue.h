// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"

namespace blink {

class ExceptionState;
class GPUBuffer;
class GPUCommandBuffer;
class GPUImageCopyExternalImage;
class GPUImageCopyTexture;
class GPUImageCopyTextureTagged;
class GPUImageDataLayout;
class ScriptPromiseResolver;
class ScriptState;
class StaticBitmapImage;
struct ExternalTextureSource;

class GPUQueue : public DawnObject<WGPUQueue> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUQueue(GPUDevice* device, WGPUQueue queue);

  GPUQueue(const GPUQueue&) = delete;
  GPUQueue& operator=(const GPUQueue&) = delete;

  // gpu_queue.idl
  void submit(ScriptState* script_state,
              const HeapVector<Member<GPUCommandBuffer>>& buffers);
  ScriptPromise onSubmittedWorkDone(ScriptState* script_state);
  void writeBuffer(ScriptState* script_state,
                   GPUBuffer* buffer,
                   uint64_t buffer_offset,
                   const MaybeShared<DOMArrayBufferView>& data,
                   uint64_t data_element_offset,
                   ExceptionState& exception_state);
  void writeBuffer(ScriptState* script_state,
                   GPUBuffer* buffer,
                   uint64_t buffer_offset,
                   const MaybeShared<DOMArrayBufferView>& data,
                   uint64_t data_element_offset,
                   uint64_t data_element_count,
                   ExceptionState& exception_state);
  void writeBuffer(ScriptState* script_state,
                   GPUBuffer* buffer,
                   uint64_t buffer_offset,
                   const DOMArrayBufferBase* data,
                   uint64_t data_byte_offset,
                   ExceptionState& exception_state);
  void writeBuffer(ScriptState* script_state,
                   GPUBuffer* buffer,
                   uint64_t buffer_offset,
                   const DOMArrayBufferBase* data,
                   uint64_t data_byte_offset,
                   uint64_t byte_size,
                   ExceptionState& exception_state);
  void writeTexture(ScriptState* script_state,
                    GPUImageCopyTexture* destination,
                    const MaybeShared<DOMArrayBufferView>& data,
                    GPUImageDataLayout* data_layout,
                    const V8GPUExtent3D* write_size,
                    ExceptionState& exception_state);
  void writeTexture(ScriptState* script_state,
                    GPUImageCopyTexture* destination,
                    const DOMArrayBufferBase* data,
                    GPUImageDataLayout* data_layout,
                    const V8GPUExtent3D* write_size,
                    ExceptionState& exception_state);
  void copyExternalImageToTexture(GPUImageCopyExternalImage* copyImage,
                                  GPUImageCopyTextureTagged* destination,
                                  const V8GPUExtent3D* copySize,
                                  ExceptionState& exception_state);

 private:
  void OnWorkDoneCallback(ScriptPromiseResolver* resolver,
                          WGPUQueueWorkDoneStatus status);
  void CopyFromVideoElement(const ExternalTextureSource source,
                            const WGPUExtent2D& video_frame_natural_size,
                            const WGPUOrigin2D& origin,
                            const WGPUExtent3D& copy_size,
                            const WGPUImageCopyTexture& destination,
                            bool dst_premultiplied_alpha,
                            PredefinedColorSpace dst_color_space,
                            bool flipY);
  bool CopyFromCanvasSourceImage(StaticBitmapImage* image,
                                 const WGPUOrigin2D& origin,
                                 const WGPUExtent3D& copy_size,
                                 const WGPUImageCopyTexture& destination,
                                 bool dst_premultiplied_alpha,
                                 PredefinedColorSpace dst_color_space,
                                 bool flipY);
  void WriteBufferImpl(ScriptState* script_state,
                       GPUBuffer* buffer,
                       uint64_t buffer_offset,
                       uint64_t data_byte_length,
                       const void* data_base_ptr,
                       unsigned data_bytes_per_element,
                       uint64_t data_byte_offset,
                       absl::optional<uint64_t> byte_size,
                       ExceptionState& exception_state);
  void WriteTextureImpl(ScriptState* script_state,
                        GPUImageCopyTexture* destination,
                        const void* data,
                        size_t dataSize,
                        GPUImageDataLayout* data_layout,
                        const V8GPUExtent3D* write_size,
                        ExceptionState& exception_state);

  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().queueSetLabel(GetHandle(), utf8_label.c_str());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_
