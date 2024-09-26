// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/flexible_array_buffer_view.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class DOMArrayBuffer;
class GPUBufferDescriptor;
class GPUMappedDOMArrayBuffer;
struct BoxedMappableWGPUBufferHandles;
class ScriptPromiseResolver;
class ScriptState;

class GPUBuffer : public DawnObject<WGPUBuffer> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUBuffer* Create(GPUDevice* device,
                           const GPUBufferDescriptor* webgpu_desc,
                           ExceptionState& exception_state);
  GPUBuffer(GPUDevice* device, uint64_t size, WGPUBuffer buffer);
  ~GPUBuffer() override;

  GPUBuffer(const GPUBuffer&) = delete;
  GPUBuffer& operator=(const GPUBuffer&) = delete;

  void Trace(Visitor* visitor) const override;

  // gpu_buffer.idl
  ScriptPromise mapAsync(ScriptState* script_state,
                         uint32_t mode,
                         uint64_t offset,
                         ExceptionState& exception_state);
  ScriptPromise mapAsync(ScriptState* script_state,
                         uint32_t mode,
                         uint64_t offset,
                         uint64_t size,
                         ExceptionState& exception_state);
  DOMArrayBuffer* getMappedRange(v8::Isolate* isolate,
                                 uint64_t offset,
                                 ExceptionState& exception_state);
  DOMArrayBuffer* getMappedRange(v8::Isolate* isolate,
                                 uint64_t offset,
                                 uint64_t size,
                                 ExceptionState& exception_state);
  void unmap(v8::Isolate* isolate);
  void destroy(v8::Isolate* isolate);
  uint64_t size() const;
  uint32_t usage() const;
  String mapState() const;

  void DetachMappedArrayBuffers(v8::Isolate* isolate);

 private:
  ScriptPromise MapAsyncImpl(ScriptState* script_state,
                             uint32_t mode,
                             uint64_t offset,
                             absl::optional<uint64_t> size,
                             ExceptionState& exception_state);
  DOMArrayBuffer* GetMappedRangeImpl(v8::Isolate* isolate,
                                     uint64_t offset,
                                     absl::optional<uint64_t> size,
                                     ExceptionState& exception_state);

  void OnMapAsyncCallback(ScriptPromiseResolver* resolver,
                          WGPUBufferMapAsyncStatus status);

  DOMArrayBuffer* CreateArrayBufferForMappedData(v8::Isolate* isolate,
                                                 void* data,
                                                 size_t data_length);
  void ResetMappingState(v8::Isolate* isolate);

  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().bufferSetLabel(GetHandle(), utf8_label.c_str());
  }

  uint64_t size_;

  // Holds onto any ArrayBuffers returned by getMappedRange, mapReadAsync, or
  // mapWriteAsync.
  HeapVector<Member<GPUMappedDOMArrayBuffer>> mapped_array_buffers_;

  // Mappable buffers remove themselves from this set on destruction.
  // It tracks the set of buffers that need to be destroyed in the
  // GPU::ContextDestroyed notification.
  scoped_refptr<BoxedMappableWGPUBufferHandles> mappable_buffer_handles_;

  // List of ranges currently returned by getMappedRange, to avoid overlaps.
  Vector<std::pair<size_t, size_t>> mapped_ranges_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_H_
