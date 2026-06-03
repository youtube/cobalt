#include "base/logging.h"
#include "dawn/dawn_proc.h"  // nogncheck

// The sanitizer is disabled for calls to procs.* since those functions may be
// dynamically loaded.
#ifndef DAWN_NO_SANITIZE
#define DAWN_NO_SANITIZE(x)
#endif

// A fake wgpuCreateInstance that prints a warning so folks know that they are using dawn_procs and
// should either use a different target to link against, or call dawnProcSetProcs.
__attribute__((weak)) WGPUInstance  CreateInstanceThatWarns(const WGPUInstanceDescriptor* desc) {
    LOG(WARNING) << "wgpuCreateInstance called on stubbed dawn_procs!";
    return nullptr;
}

constexpr DawnProcTable MakeNullProcTable() {
    DawnProcTable procs = {};
    procs.createInstance = CreateInstanceThatWarns;
    return procs;
}

static DawnProcTable kNullProcs = MakeNullProcTable();
static DawnProcTable procs = MakeNullProcTable();

__attribute__((weak)) void  dawnProcSetProcs(const DawnProcTable* procs_) {
    if (procs_) {
        procs = *procs_;
    } else {
        procs = kNullProcs;
    }
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuAdapterInfoFreeMembers(WGPUAdapterInfo value) {
    procs.adapterInfoFreeMembers(value);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuAdapterPropertiesMemoryHeapsFreeMembers(WGPUAdapterPropertiesMemoryHeaps value) {
    procs.adapterPropertiesMemoryHeapsFreeMembers(value);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuAdapterPropertiesSubgroupMatrixConfigsFreeMembers(WGPUAdapterPropertiesSubgroupMatrixConfigs value) {
    procs.adapterPropertiesSubgroupMatrixConfigsFreeMembers(value);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUInstance  wgpuCreateInstance(WGPUInstanceDescriptor const * descriptor) {
return     procs.createInstance(descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDawnDrmFormatCapabilitiesFreeMembers(WGPUDawnDrmFormatCapabilities value) {
    procs.dawnDrmFormatCapabilitiesFreeMembers(value);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuGetInstanceCapabilities(WGPUInstanceCapabilities * capabilities) {
return     procs.getInstanceCapabilities(capabilities);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUProc  wgpuGetProcAddress(WGPUStringView procName) {
return     procs.getProcAddress(procName);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSharedBufferMemoryEndAccessStateFreeMembers(WGPUSharedBufferMemoryEndAccessState value) {
    procs.sharedBufferMemoryEndAccessStateFreeMembers(value);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSharedTextureMemoryEndAccessStateFreeMembers(WGPUSharedTextureMemoryEndAccessState value) {
    procs.sharedTextureMemoryEndAccessStateFreeMembers(value);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSupportedFeaturesFreeMembers(WGPUSupportedFeatures value) {
    procs.supportedFeaturesFreeMembers(value);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSupportedWGSLLanguageFeaturesFreeMembers(WGPUSupportedWGSLLanguageFeatures value) {
    procs.supportedWGSLLanguageFeaturesFreeMembers(value);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities value) {
    procs.surfaceCapabilitiesFreeMembers(value);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUDevice  wgpuAdapterCreateDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor) {
return     procs.adapterCreateDevice(adapter, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuAdapterGetFeatures(WGPUAdapter adapter, WGPUSupportedFeatures * features) {
    procs.adapterGetFeatures(adapter, features);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuAdapterGetFormatCapabilities(WGPUAdapter adapter, WGPUTextureFormat format, WGPUDawnFormatCapabilities * capabilities) {
return     procs.adapterGetFormatCapabilities(adapter, format, capabilities);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuAdapterGetInfo(WGPUAdapter adapter, WGPUAdapterInfo * info) {
return     procs.adapterGetInfo(adapter, info);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUInstance  wgpuAdapterGetInstance(WGPUAdapter adapter) {
return     procs.adapterGetInstance(adapter);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuAdapterGetLimits(WGPUAdapter adapter, WGPULimits * limits) {
return     procs.adapterGetLimits(adapter, limits);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBool  wgpuAdapterHasFeature(WGPUAdapter adapter, WGPUFeatureName feature) {
return     procs.adapterHasFeature(adapter, feature);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUFuture  wgpuAdapterRequestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const * options, WGPURequestDeviceCallbackInfo callbackInfo) {
return     procs.adapterRequestDevice(adapter, options, callbackInfo);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuAdapterAddRef(WGPUAdapter adapter) {
    procs.adapterAddRef(adapter);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuAdapterRelease(WGPUAdapter adapter) {
    procs.adapterRelease(adapter);
}

__attribute__((weak)) void  wgpuBindGroupSetLabel(WGPUBindGroup bindGroup, WGPUStringView label) {
    procs.bindGroupSetLabel(bindGroup, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuBindGroupAddRef(WGPUBindGroup bindGroup) {
    procs.bindGroupAddRef(bindGroup);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuBindGroupRelease(WGPUBindGroup bindGroup) {
    procs.bindGroupRelease(bindGroup);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuBindGroupLayoutSetLabel(WGPUBindGroupLayout bindGroupLayout, WGPUStringView label) {
    procs.bindGroupLayoutSetLabel(bindGroupLayout, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuBindGroupLayoutAddRef(WGPUBindGroupLayout bindGroupLayout) {
    procs.bindGroupLayoutAddRef(bindGroupLayout);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuBindGroupLayoutRelease(WGPUBindGroupLayout bindGroupLayout) {
    procs.bindGroupLayoutRelease(bindGroupLayout);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuBufferDestroy(WGPUBuffer buffer) {
    procs.bufferDestroy(buffer);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void const *  wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset, size_t size) {
return     procs.bufferGetConstMappedRange(buffer, offset, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void *  wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size) {
return     procs.bufferGetMappedRange(buffer, offset, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBufferMapState  wgpuBufferGetMapState(WGPUBuffer buffer) {
return     procs.bufferGetMapState(buffer);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) uint64_t  wgpuBufferGetSize(WGPUBuffer buffer) {
return     procs.bufferGetSize(buffer);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBufferUsage  wgpuBufferGetUsage(WGPUBuffer buffer) {
return     procs.bufferGetUsage(buffer);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUFuture  wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapMode mode, size_t offset, size_t size, WGPUBufferMapCallbackInfo callbackInfo) {
return     procs.bufferMapAsync(buffer, mode, offset, size, callbackInfo);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuBufferReadMappedRange(WGPUBuffer buffer, size_t offset, void * data, size_t size) {
return     procs.bufferReadMappedRange(buffer, offset, data, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuBufferSetLabel(WGPUBuffer buffer, WGPUStringView label) {
    procs.bufferSetLabel(buffer, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuBufferUnmap(WGPUBuffer buffer) {
    procs.bufferUnmap(buffer);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuBufferWriteMappedRange(WGPUBuffer buffer, size_t offset, void const * data, size_t size) {
return     procs.bufferWriteMappedRange(buffer, offset, data, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuBufferAddRef(WGPUBuffer buffer) {
    procs.bufferAddRef(buffer);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuBufferRelease(WGPUBuffer buffer) {
    procs.bufferRelease(buffer);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandBufferSetLabel(WGPUCommandBuffer commandBuffer, WGPUStringView label) {
    procs.commandBufferSetLabel(commandBuffer, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandBufferAddRef(WGPUCommandBuffer commandBuffer) {
    procs.commandBufferAddRef(commandBuffer);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandBufferRelease(WGPUCommandBuffer commandBuffer) {
    procs.commandBufferRelease(commandBuffer);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUComputePassEncoder  wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder commandEncoder, WGPUComputePassDescriptor const * descriptor) {
return     procs.commandEncoderBeginComputePass(commandEncoder, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPURenderPassEncoder  wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder, WGPURenderPassDescriptor const * descriptor) {
return     procs.commandEncoderBeginRenderPass(commandEncoder, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderClearBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
    procs.commandEncoderClearBuffer(commandEncoder, buffer, offset, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size) {
    procs.commandEncoderCopyBufferToBuffer(commandEncoder, source, sourceOffset, destination, destinationOffset, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderCopyBufferToTexture(WGPUCommandEncoder commandEncoder, WGPUTexelCopyBufferInfo const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize) {
    procs.commandEncoderCopyBufferToTexture(commandEncoder, source, destination, copySize);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder commandEncoder, WGPUTexelCopyTextureInfo const * source, WGPUTexelCopyBufferInfo const * destination, WGPUExtent3D const * copySize) {
    procs.commandEncoderCopyTextureToBuffer(commandEncoder, source, destination, copySize);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderCopyTextureToTexture(WGPUCommandEncoder commandEncoder, WGPUTexelCopyTextureInfo const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize) {
    procs.commandEncoderCopyTextureToTexture(commandEncoder, source, destination, copySize);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUCommandBuffer  wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder, WGPUCommandBufferDescriptor const * descriptor) {
return     procs.commandEncoderFinish(commandEncoder, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderInjectValidationError(WGPUCommandEncoder commandEncoder, WGPUStringView message) {
    procs.commandEncoderInjectValidationError(commandEncoder, message);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderInsertDebugMarker(WGPUCommandEncoder commandEncoder, WGPUStringView markerLabel) {
    procs.commandEncoderInsertDebugMarker(commandEncoder, markerLabel);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderPopDebugGroup(WGPUCommandEncoder commandEncoder) {
    procs.commandEncoderPopDebugGroup(commandEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderPushDebugGroup(WGPUCommandEncoder commandEncoder, WGPUStringView groupLabel) {
    procs.commandEncoderPushDebugGroup(commandEncoder, groupLabel);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderResolveQuerySet(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset) {
    procs.commandEncoderResolveQuerySet(commandEncoder, querySet, firstQuery, queryCount, destination, destinationOffset);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderSetLabel(WGPUCommandEncoder commandEncoder, WGPUStringView label) {
    procs.commandEncoderSetLabel(commandEncoder, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderWriteBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer buffer, uint64_t bufferOffset, uint8_t const * data, uint64_t size) {
    procs.commandEncoderWriteBuffer(commandEncoder, buffer, bufferOffset, data, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderWriteTimestamp(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t queryIndex) {
    procs.commandEncoderWriteTimestamp(commandEncoder, querySet, queryIndex);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderAddRef(WGPUCommandEncoder commandEncoder) {
    procs.commandEncoderAddRef(commandEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuCommandEncoderRelease(WGPUCommandEncoder commandEncoder) {
    procs.commandEncoderRelease(commandEncoder);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder computePassEncoder, uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ) {
    procs.computePassEncoderDispatchWorkgroups(computePassEncoder, workgroupCountX, workgroupCountY, workgroupCountZ);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderDispatchWorkgroupsIndirect(WGPUComputePassEncoder computePassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
    procs.computePassEncoderDispatchWorkgroupsIndirect(computePassEncoder, indirectBuffer, indirectOffset);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderEnd(WGPUComputePassEncoder computePassEncoder) {
    procs.computePassEncoderEnd(computePassEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderInsertDebugMarker(WGPUComputePassEncoder computePassEncoder, WGPUStringView markerLabel) {
    procs.computePassEncoderInsertDebugMarker(computePassEncoder, markerLabel);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderPopDebugGroup(WGPUComputePassEncoder computePassEncoder) {
    procs.computePassEncoderPopDebugGroup(computePassEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderPushDebugGroup(WGPUComputePassEncoder computePassEncoder, WGPUStringView groupLabel) {
    procs.computePassEncoderPushDebugGroup(computePassEncoder, groupLabel);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder computePassEncoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) {
    procs.computePassEncoderSetBindGroup(computePassEncoder, groupIndex, group, dynamicOffsetCount, dynamicOffsets);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderSetImmediateData(WGPUComputePassEncoder computePassEncoder, uint32_t offset, void const * data, size_t size) {
    procs.computePassEncoderSetImmediateData(computePassEncoder, offset, data, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderSetLabel(WGPUComputePassEncoder computePassEncoder, WGPUStringView label) {
    procs.computePassEncoderSetLabel(computePassEncoder, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderSetPipeline(WGPUComputePassEncoder computePassEncoder, WGPUComputePipeline pipeline) {
    procs.computePassEncoderSetPipeline(computePassEncoder, pipeline);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderWriteTimestamp(WGPUComputePassEncoder computePassEncoder, WGPUQuerySet querySet, uint32_t queryIndex) {
    procs.computePassEncoderWriteTimestamp(computePassEncoder, querySet, queryIndex);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderAddRef(WGPUComputePassEncoder computePassEncoder) {
    procs.computePassEncoderAddRef(computePassEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePassEncoderRelease(WGPUComputePassEncoder computePassEncoder) {
    procs.computePassEncoderRelease(computePassEncoder);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBindGroupLayout  wgpuComputePipelineGetBindGroupLayout(WGPUComputePipeline computePipeline, uint32_t groupIndex) {
return     procs.computePipelineGetBindGroupLayout(computePipeline, groupIndex);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePipelineSetLabel(WGPUComputePipeline computePipeline, WGPUStringView label) {
    procs.computePipelineSetLabel(computePipeline, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePipelineAddRef(WGPUComputePipeline computePipeline) {
    procs.computePipelineAddRef(computePipeline);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuComputePipelineRelease(WGPUComputePipeline computePipeline) {
    procs.computePipelineRelease(computePipeline);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBindGroup  wgpuDeviceCreateBindGroup(WGPUDevice device, WGPUBindGroupDescriptor const * descriptor) {
return     procs.deviceCreateBindGroup(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBindGroupLayout  wgpuDeviceCreateBindGroupLayout(WGPUDevice device, WGPUBindGroupLayoutDescriptor const * descriptor) {
return     procs.deviceCreateBindGroupLayout(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBuffer  wgpuDeviceCreateBuffer(WGPUDevice device, WGPUBufferDescriptor const * descriptor) {
return     procs.deviceCreateBuffer(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUCommandEncoder  wgpuDeviceCreateCommandEncoder(WGPUDevice device, WGPUCommandEncoderDescriptor const * descriptor) {
return     procs.deviceCreateCommandEncoder(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUComputePipeline  wgpuDeviceCreateComputePipeline(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor) {
return     procs.deviceCreateComputePipeline(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUFuture  wgpuDeviceCreateComputePipelineAsync(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncCallbackInfo callbackInfo) {
return     procs.deviceCreateComputePipelineAsync(device, descriptor, callbackInfo);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBuffer  wgpuDeviceCreateErrorBuffer(WGPUDevice device, WGPUBufferDescriptor const * descriptor) {
return     procs.deviceCreateErrorBuffer(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUExternalTexture  wgpuDeviceCreateErrorExternalTexture(WGPUDevice device) {
return     procs.deviceCreateErrorExternalTexture(device);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUShaderModule  wgpuDeviceCreateErrorShaderModule(WGPUDevice device, WGPUShaderModuleDescriptor const * descriptor, WGPUStringView errorMessage) {
return     procs.deviceCreateErrorShaderModule(device, descriptor, errorMessage);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUTexture  wgpuDeviceCreateErrorTexture(WGPUDevice device, WGPUTextureDescriptor const * descriptor) {
return     procs.deviceCreateErrorTexture(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUExternalTexture  wgpuDeviceCreateExternalTexture(WGPUDevice device, WGPUExternalTextureDescriptor const * externalTextureDescriptor) {
return     procs.deviceCreateExternalTexture(device, externalTextureDescriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUPipelineLayout  wgpuDeviceCreatePipelineLayout(WGPUDevice device, WGPUPipelineLayoutDescriptor const * descriptor) {
return     procs.deviceCreatePipelineLayout(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUQuerySet  wgpuDeviceCreateQuerySet(WGPUDevice device, WGPUQuerySetDescriptor const * descriptor) {
return     procs.deviceCreateQuerySet(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPURenderBundleEncoder  wgpuDeviceCreateRenderBundleEncoder(WGPUDevice device, WGPURenderBundleEncoderDescriptor const * descriptor) {
return     procs.deviceCreateRenderBundleEncoder(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPURenderPipeline  wgpuDeviceCreateRenderPipeline(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor) {
return     procs.deviceCreateRenderPipeline(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUFuture  wgpuDeviceCreateRenderPipelineAsync(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo) {
return     procs.deviceCreateRenderPipelineAsync(device, descriptor, callbackInfo);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUSampler  wgpuDeviceCreateSampler(WGPUDevice device, WGPUSamplerDescriptor const * descriptor) {
return     procs.deviceCreateSampler(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUShaderModule  wgpuDeviceCreateShaderModule(WGPUDevice device, WGPUShaderModuleDescriptor const * descriptor) {
return     procs.deviceCreateShaderModule(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUTexture  wgpuDeviceCreateTexture(WGPUDevice device, WGPUTextureDescriptor const * descriptor) {
return     procs.deviceCreateTexture(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDeviceDestroy(WGPUDevice device) {
    procs.deviceDestroy(device);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDeviceForceLoss(WGPUDevice device, WGPUDeviceLostReason type, WGPUStringView message) {
    procs.deviceForceLoss(device, type, message);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUAdapter  wgpuDeviceGetAdapter(WGPUDevice device) {
return     procs.deviceGetAdapter(device);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuDeviceGetAdapterInfo(WGPUDevice device, WGPUAdapterInfo * adapterInfo) {
return     procs.deviceGetAdapterInfo(device, adapterInfo);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuDeviceGetAHardwareBufferProperties(WGPUDevice device, void * handle, WGPUAHardwareBufferProperties * properties) {
return     procs.deviceGetAHardwareBufferProperties(device, handle, properties);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDeviceGetFeatures(WGPUDevice device, WGPUSupportedFeatures * features) {
    procs.deviceGetFeatures(device, features);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuDeviceGetLimits(WGPUDevice device, WGPULimits * limits) {
return     procs.deviceGetLimits(device, limits);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUFuture  wgpuDeviceGetLostFuture(WGPUDevice device) {
return     procs.deviceGetLostFuture(device);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUQueue  wgpuDeviceGetQueue(WGPUDevice device) {
return     procs.deviceGetQueue(device);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBool  wgpuDeviceHasFeature(WGPUDevice device, WGPUFeatureName feature) {
return     procs.deviceHasFeature(device, feature);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUSharedBufferMemory  wgpuDeviceImportSharedBufferMemory(WGPUDevice device, WGPUSharedBufferMemoryDescriptor const * descriptor) {
return     procs.deviceImportSharedBufferMemory(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUSharedFence  wgpuDeviceImportSharedFence(WGPUDevice device, WGPUSharedFenceDescriptor const * descriptor) {
return     procs.deviceImportSharedFence(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUSharedTextureMemory  wgpuDeviceImportSharedTextureMemory(WGPUDevice device, WGPUSharedTextureMemoryDescriptor const * descriptor) {
return     procs.deviceImportSharedTextureMemory(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDeviceInjectError(WGPUDevice device, WGPUErrorType type, WGPUStringView message) {
    procs.deviceInjectError(device, type, message);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUFuture  wgpuDevicePopErrorScope(WGPUDevice device, WGPUPopErrorScopeCallbackInfo callbackInfo) {
return     procs.devicePopErrorScope(device, callbackInfo);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDevicePushErrorScope(WGPUDevice device, WGPUErrorFilter filter) {
    procs.devicePushErrorScope(device, filter);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDeviceSetLabel(WGPUDevice device, WGPUStringView label) {
    procs.deviceSetLabel(device, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDeviceSetLoggingCallback(WGPUDevice device, WGPULoggingCallbackInfo callbackInfo) {
    procs.deviceSetLoggingCallback(device, callbackInfo);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDeviceTick(WGPUDevice device) {
    procs.deviceTick(device);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDeviceValidateTextureDescriptor(WGPUDevice device, WGPUTextureDescriptor const * descriptor) {
    procs.deviceValidateTextureDescriptor(device, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDeviceAddRef(WGPUDevice device) {
    procs.deviceAddRef(device);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuDeviceRelease(WGPUDevice device) {
    procs.deviceRelease(device);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuExternalTextureDestroy(WGPUExternalTexture externalTexture) {
    procs.externalTextureDestroy(externalTexture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuExternalTextureExpire(WGPUExternalTexture externalTexture) {
    procs.externalTextureExpire(externalTexture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuExternalTextureRefresh(WGPUExternalTexture externalTexture) {
    procs.externalTextureRefresh(externalTexture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuExternalTextureSetLabel(WGPUExternalTexture externalTexture, WGPUStringView label) {
    procs.externalTextureSetLabel(externalTexture, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuExternalTextureAddRef(WGPUExternalTexture externalTexture) {
    procs.externalTextureAddRef(externalTexture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuExternalTextureRelease(WGPUExternalTexture externalTexture) {
    procs.externalTextureRelease(externalTexture);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUSurface  wgpuInstanceCreateSurface(WGPUInstance instance, WGPUSurfaceDescriptor const * descriptor) {
return     procs.instanceCreateSurface(instance, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuInstanceGetWGSLLanguageFeatures(WGPUInstance instance, WGPUSupportedWGSLLanguageFeatures * features) {
return     procs.instanceGetWGSLLanguageFeatures(instance, features);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBool  wgpuInstanceHasWGSLLanguageFeature(WGPUInstance instance, WGPUWGSLLanguageFeatureName feature) {
return     procs.instanceHasWGSLLanguageFeature(instance, feature);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuInstanceProcessEvents(WGPUInstance instance) {
    procs.instanceProcessEvents(instance);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUFuture  wgpuInstanceRequestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterCallbackInfo callbackInfo) {
return     procs.instanceRequestAdapter(instance, options, callbackInfo);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUWaitStatus  wgpuInstanceWaitAny(WGPUInstance instance, size_t futureCount, WGPUFutureWaitInfo * futures, uint64_t timeoutNS) {
return     procs.instanceWaitAny(instance, futureCount, futures, timeoutNS);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuInstanceAddRef(WGPUInstance instance) {
    procs.instanceAddRef(instance);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuInstanceRelease(WGPUInstance instance) {
    procs.instanceRelease(instance);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuPipelineLayoutSetLabel(WGPUPipelineLayout pipelineLayout, WGPUStringView label) {
    procs.pipelineLayoutSetLabel(pipelineLayout, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuPipelineLayoutAddRef(WGPUPipelineLayout pipelineLayout) {
    procs.pipelineLayoutAddRef(pipelineLayout);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuPipelineLayoutRelease(WGPUPipelineLayout pipelineLayout) {
    procs.pipelineLayoutRelease(pipelineLayout);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQuerySetDestroy(WGPUQuerySet querySet) {
    procs.querySetDestroy(querySet);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) uint32_t  wgpuQuerySetGetCount(WGPUQuerySet querySet) {
return     procs.querySetGetCount(querySet);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUQueryType  wgpuQuerySetGetType(WGPUQuerySet querySet) {
return     procs.querySetGetType(querySet);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQuerySetSetLabel(WGPUQuerySet querySet, WGPUStringView label) {
    procs.querySetSetLabel(querySet, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQuerySetAddRef(WGPUQuerySet querySet) {
    procs.querySetAddRef(querySet);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQuerySetRelease(WGPUQuerySet querySet) {
    procs.querySetRelease(querySet);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQueueCopyExternalTextureForBrowser(WGPUQueue queue, WGPUImageCopyExternalTexture const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize, WGPUCopyTextureForBrowserOptions const * options) {
    procs.queueCopyExternalTextureForBrowser(queue, source, destination, copySize, options);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQueueCopyTextureForBrowser(WGPUQueue queue, WGPUTexelCopyTextureInfo const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize, WGPUCopyTextureForBrowserOptions const * options) {
    procs.queueCopyTextureForBrowser(queue, source, destination, copySize, options);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUFuture  wgpuQueueOnSubmittedWorkDone(WGPUQueue queue, WGPUQueueWorkDoneCallbackInfo callbackInfo) {
return     procs.queueOnSubmittedWorkDone(queue, callbackInfo);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQueueSetLabel(WGPUQueue queue, WGPUStringView label) {
    procs.queueSetLabel(queue, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQueueSubmit(WGPUQueue queue, size_t commandCount, WGPUCommandBuffer const * commands) {
    procs.queueSubmit(queue, commandCount, commands);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, void const * data, size_t size) {
    procs.queueWriteBuffer(queue, buffer, bufferOffset, data, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQueueWriteTexture(WGPUQueue queue, WGPUTexelCopyTextureInfo const * destination, void const * data, size_t dataSize, WGPUTexelCopyBufferLayout const * dataLayout, WGPUExtent3D const * writeSize) {
    procs.queueWriteTexture(queue, destination, data, dataSize, dataLayout, writeSize);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQueueAddRef(WGPUQueue queue) {
    procs.queueAddRef(queue);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuQueueRelease(WGPUQueue queue) {
    procs.queueRelease(queue);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleSetLabel(WGPURenderBundle renderBundle, WGPUStringView label) {
    procs.renderBundleSetLabel(renderBundle, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleAddRef(WGPURenderBundle renderBundle) {
    procs.renderBundleAddRef(renderBundle);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleRelease(WGPURenderBundle renderBundle) {
    procs.renderBundleRelease(renderBundle);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderDraw(WGPURenderBundleEncoder renderBundleEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    procs.renderBundleEncoderDraw(renderBundleEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderDrawIndexed(WGPURenderBundleEncoder renderBundleEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) {
    procs.renderBundleEncoderDrawIndexed(renderBundleEncoder, indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderDrawIndexedIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
    procs.renderBundleEncoderDrawIndexedIndirect(renderBundleEncoder, indirectBuffer, indirectOffset);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
    procs.renderBundleEncoderDrawIndirect(renderBundleEncoder, indirectBuffer, indirectOffset);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPURenderBundle  wgpuRenderBundleEncoderFinish(WGPURenderBundleEncoder renderBundleEncoder, WGPURenderBundleDescriptor const * descriptor) {
return     procs.renderBundleEncoderFinish(renderBundleEncoder, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderInsertDebugMarker(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView markerLabel) {
    procs.renderBundleEncoderInsertDebugMarker(renderBundleEncoder, markerLabel);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderPopDebugGroup(WGPURenderBundleEncoder renderBundleEncoder) {
    procs.renderBundleEncoderPopDebugGroup(renderBundleEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderPushDebugGroup(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView groupLabel) {
    procs.renderBundleEncoderPushDebugGroup(renderBundleEncoder, groupLabel);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder renderBundleEncoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) {
    procs.renderBundleEncoderSetBindGroup(renderBundleEncoder, groupIndex, group, dynamicOffsetCount, dynamicOffsets);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderSetImmediateData(WGPURenderBundleEncoder renderBundleEncoder, uint32_t offset, void const * data, size_t size) {
    procs.renderBundleEncoderSetImmediateData(renderBundleEncoder, offset, data, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderSetIndexBuffer(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
    procs.renderBundleEncoderSetIndexBuffer(renderBundleEncoder, buffer, format, offset, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView label) {
    procs.renderBundleEncoderSetLabel(renderBundleEncoder, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderSetPipeline(WGPURenderBundleEncoder renderBundleEncoder, WGPURenderPipeline pipeline) {
    procs.renderBundleEncoderSetPipeline(renderBundleEncoder, pipeline);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderSetVertexBuffer(WGPURenderBundleEncoder renderBundleEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
    procs.renderBundleEncoderSetVertexBuffer(renderBundleEncoder, slot, buffer, offset, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderAddRef(WGPURenderBundleEncoder renderBundleEncoder) {
    procs.renderBundleEncoderAddRef(renderBundleEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderBundleEncoderRelease(WGPURenderBundleEncoder renderBundleEncoder) {
    procs.renderBundleEncoderRelease(renderBundleEncoder);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderBeginOcclusionQuery(WGPURenderPassEncoder renderPassEncoder, uint32_t queryIndex) {
    procs.renderPassEncoderBeginOcclusionQuery(renderPassEncoder, queryIndex);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderDraw(WGPURenderPassEncoder renderPassEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    procs.renderPassEncoderDraw(renderPassEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder renderPassEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) {
    procs.renderPassEncoderDrawIndexed(renderPassEncoder, indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderDrawIndexedIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
    procs.renderPassEncoderDrawIndexedIndirect(renderPassEncoder, indirectBuffer, indirectOffset);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderDrawIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
    procs.renderPassEncoderDrawIndirect(renderPassEncoder, indirectBuffer, indirectOffset);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderEnd(WGPURenderPassEncoder renderPassEncoder) {
    procs.renderPassEncoderEnd(renderPassEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderEndOcclusionQuery(WGPURenderPassEncoder renderPassEncoder) {
    procs.renderPassEncoderEndOcclusionQuery(renderPassEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderExecuteBundles(WGPURenderPassEncoder renderPassEncoder, size_t bundleCount, WGPURenderBundle const * bundles) {
    procs.renderPassEncoderExecuteBundles(renderPassEncoder, bundleCount, bundles);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderInsertDebugMarker(WGPURenderPassEncoder renderPassEncoder, WGPUStringView markerLabel) {
    procs.renderPassEncoderInsertDebugMarker(renderPassEncoder, markerLabel);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderMultiDrawIndexedIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, WGPUBuffer drawCountBuffer, uint64_t drawCountBufferOffset) {
    procs.renderPassEncoderMultiDrawIndexedIndirect(renderPassEncoder, indirectBuffer, indirectOffset, maxDrawCount, drawCountBuffer, drawCountBufferOffset);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderMultiDrawIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, WGPUBuffer drawCountBuffer, uint64_t drawCountBufferOffset) {
    procs.renderPassEncoderMultiDrawIndirect(renderPassEncoder, indirectBuffer, indirectOffset, maxDrawCount, drawCountBuffer, drawCountBufferOffset);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderPixelLocalStorageBarrier(WGPURenderPassEncoder renderPassEncoder) {
    procs.renderPassEncoderPixelLocalStorageBarrier(renderPassEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderPopDebugGroup(WGPURenderPassEncoder renderPassEncoder) {
    procs.renderPassEncoderPopDebugGroup(renderPassEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderPushDebugGroup(WGPURenderPassEncoder renderPassEncoder, WGPUStringView groupLabel) {
    procs.renderPassEncoderPushDebugGroup(renderPassEncoder, groupLabel);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) {
    procs.renderPassEncoderSetBindGroup(renderPassEncoder, groupIndex, group, dynamicOffsetCount, dynamicOffsets);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderSetBlendConstant(WGPURenderPassEncoder renderPassEncoder, WGPUColor const * color) {
    procs.renderPassEncoderSetBlendConstant(renderPassEncoder, color);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderSetImmediateData(WGPURenderPassEncoder renderPassEncoder, uint32_t offset, void const * data, size_t size) {
    procs.renderPassEncoderSetImmediateData(renderPassEncoder, offset, data, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
    procs.renderPassEncoderSetIndexBuffer(renderPassEncoder, buffer, format, offset, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderSetLabel(WGPURenderPassEncoder renderPassEncoder, WGPUStringView label) {
    procs.renderPassEncoderSetLabel(renderPassEncoder, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder renderPassEncoder, WGPURenderPipeline pipeline) {
    procs.renderPassEncoderSetPipeline(renderPassEncoder, pipeline);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderSetScissorRect(WGPURenderPassEncoder renderPassEncoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    procs.renderPassEncoderSetScissorRect(renderPassEncoder, x, y, width, height);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder renderPassEncoder, uint32_t reference) {
    procs.renderPassEncoderSetStencilReference(renderPassEncoder, reference);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder renderPassEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
    procs.renderPassEncoderSetVertexBuffer(renderPassEncoder, slot, buffer, offset, size);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderSetViewport(WGPURenderPassEncoder renderPassEncoder, float x, float y, float width, float height, float minDepth, float maxDepth) {
    procs.renderPassEncoderSetViewport(renderPassEncoder, x, y, width, height, minDepth, maxDepth);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderWriteTimestamp(WGPURenderPassEncoder renderPassEncoder, WGPUQuerySet querySet, uint32_t queryIndex) {
    procs.renderPassEncoderWriteTimestamp(renderPassEncoder, querySet, queryIndex);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderAddRef(WGPURenderPassEncoder renderPassEncoder) {
    procs.renderPassEncoderAddRef(renderPassEncoder);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPassEncoderRelease(WGPURenderPassEncoder renderPassEncoder) {
    procs.renderPassEncoderRelease(renderPassEncoder);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBindGroupLayout  wgpuRenderPipelineGetBindGroupLayout(WGPURenderPipeline renderPipeline, uint32_t groupIndex) {
return     procs.renderPipelineGetBindGroupLayout(renderPipeline, groupIndex);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPipelineSetLabel(WGPURenderPipeline renderPipeline, WGPUStringView label) {
    procs.renderPipelineSetLabel(renderPipeline, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPipelineAddRef(WGPURenderPipeline renderPipeline) {
    procs.renderPipelineAddRef(renderPipeline);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuRenderPipelineRelease(WGPURenderPipeline renderPipeline) {
    procs.renderPipelineRelease(renderPipeline);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSamplerSetLabel(WGPUSampler sampler, WGPUStringView label) {
    procs.samplerSetLabel(sampler, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSamplerAddRef(WGPUSampler sampler) {
    procs.samplerAddRef(sampler);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSamplerRelease(WGPUSampler sampler) {
    procs.samplerRelease(sampler);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUFuture  wgpuShaderModuleGetCompilationInfo(WGPUShaderModule shaderModule, WGPUCompilationInfoCallbackInfo callbackInfo) {
return     procs.shaderModuleGetCompilationInfo(shaderModule, callbackInfo);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuShaderModuleSetLabel(WGPUShaderModule shaderModule, WGPUStringView label) {
    procs.shaderModuleSetLabel(shaderModule, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuShaderModuleAddRef(WGPUShaderModule shaderModule) {
    procs.shaderModuleAddRef(shaderModule);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuShaderModuleRelease(WGPUShaderModule shaderModule) {
    procs.shaderModuleRelease(shaderModule);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuSharedBufferMemoryBeginAccess(WGPUSharedBufferMemory sharedBufferMemory, WGPUBuffer buffer, WGPUSharedBufferMemoryBeginAccessDescriptor const * descriptor) {
return     procs.sharedBufferMemoryBeginAccess(sharedBufferMemory, buffer, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBuffer  wgpuSharedBufferMemoryCreateBuffer(WGPUSharedBufferMemory sharedBufferMemory, WGPUBufferDescriptor const * descriptor) {
return     procs.sharedBufferMemoryCreateBuffer(sharedBufferMemory, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuSharedBufferMemoryEndAccess(WGPUSharedBufferMemory sharedBufferMemory, WGPUBuffer buffer, WGPUSharedBufferMemoryEndAccessState * descriptor) {
return     procs.sharedBufferMemoryEndAccess(sharedBufferMemory, buffer, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuSharedBufferMemoryGetProperties(WGPUSharedBufferMemory sharedBufferMemory, WGPUSharedBufferMemoryProperties * properties) {
return     procs.sharedBufferMemoryGetProperties(sharedBufferMemory, properties);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBool  wgpuSharedBufferMemoryIsDeviceLost(WGPUSharedBufferMemory sharedBufferMemory) {
return     procs.sharedBufferMemoryIsDeviceLost(sharedBufferMemory);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSharedBufferMemorySetLabel(WGPUSharedBufferMemory sharedBufferMemory, WGPUStringView label) {
    procs.sharedBufferMemorySetLabel(sharedBufferMemory, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSharedBufferMemoryAddRef(WGPUSharedBufferMemory sharedBufferMemory) {
    procs.sharedBufferMemoryAddRef(sharedBufferMemory);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSharedBufferMemoryRelease(WGPUSharedBufferMemory sharedBufferMemory) {
    procs.sharedBufferMemoryRelease(sharedBufferMemory);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSharedFenceExportInfo(WGPUSharedFence sharedFence, WGPUSharedFenceExportInfo * info) {
    procs.sharedFenceExportInfo(sharedFence, info);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSharedFenceAddRef(WGPUSharedFence sharedFence) {
    procs.sharedFenceAddRef(sharedFence);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSharedFenceRelease(WGPUSharedFence sharedFence) {
    procs.sharedFenceRelease(sharedFence);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuSharedTextureMemoryBeginAccess(WGPUSharedTextureMemory sharedTextureMemory, WGPUTexture texture, WGPUSharedTextureMemoryBeginAccessDescriptor const * descriptor) {
return     procs.sharedTextureMemoryBeginAccess(sharedTextureMemory, texture, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUTexture  wgpuSharedTextureMemoryCreateTexture(WGPUSharedTextureMemory sharedTextureMemory, WGPUTextureDescriptor const * descriptor) {
return     procs.sharedTextureMemoryCreateTexture(sharedTextureMemory, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuSharedTextureMemoryEndAccess(WGPUSharedTextureMemory sharedTextureMemory, WGPUTexture texture, WGPUSharedTextureMemoryEndAccessState * descriptor) {
return     procs.sharedTextureMemoryEndAccess(sharedTextureMemory, texture, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuSharedTextureMemoryGetProperties(WGPUSharedTextureMemory sharedTextureMemory, WGPUSharedTextureMemoryProperties * properties) {
return     procs.sharedTextureMemoryGetProperties(sharedTextureMemory, properties);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUBool  wgpuSharedTextureMemoryIsDeviceLost(WGPUSharedTextureMemory sharedTextureMemory) {
return     procs.sharedTextureMemoryIsDeviceLost(sharedTextureMemory);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSharedTextureMemorySetLabel(WGPUSharedTextureMemory sharedTextureMemory, WGPUStringView label) {
    procs.sharedTextureMemorySetLabel(sharedTextureMemory, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSharedTextureMemoryAddRef(WGPUSharedTextureMemory sharedTextureMemory) {
    procs.sharedTextureMemoryAddRef(sharedTextureMemory);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSharedTextureMemoryRelease(WGPUSharedTextureMemory sharedTextureMemory) {
    procs.sharedTextureMemoryRelease(sharedTextureMemory);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSurfaceConfigure(WGPUSurface surface, WGPUSurfaceConfiguration const * config) {
    procs.surfaceConfigure(surface, config);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUStatus  wgpuSurfaceGetCapabilities(WGPUSurface surface, WGPUAdapter adapter, WGPUSurfaceCapabilities * capabilities) {
return     procs.surfaceGetCapabilities(surface, adapter, capabilities);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSurfaceGetCurrentTexture(WGPUSurface surface, WGPUSurfaceTexture * surfaceTexture) {
    procs.surfaceGetCurrentTexture(surface, surfaceTexture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSurfacePresent(WGPUSurface surface) {
    procs.surfacePresent(surface);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSurfaceSetLabel(WGPUSurface surface, WGPUStringView label) {
    procs.surfaceSetLabel(surface, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSurfaceUnconfigure(WGPUSurface surface) {
    procs.surfaceUnconfigure(surface);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSurfaceAddRef(WGPUSurface surface) {
    procs.surfaceAddRef(surface);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuSurfaceRelease(WGPUSurface surface) {
    procs.surfaceRelease(surface);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUTextureView  wgpuTextureCreateErrorView(WGPUTexture texture, WGPUTextureViewDescriptor const * descriptor) {
return     procs.textureCreateErrorView(texture, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUTextureView  wgpuTextureCreateView(WGPUTexture texture, WGPUTextureViewDescriptor const * descriptor) {
return     procs.textureCreateView(texture, descriptor);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuTextureDestroy(WGPUTexture texture) {
    procs.textureDestroy(texture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) uint32_t  wgpuTextureGetDepthOrArrayLayers(WGPUTexture texture) {
return     procs.textureGetDepthOrArrayLayers(texture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUTextureDimension  wgpuTextureGetDimension(WGPUTexture texture) {
return     procs.textureGetDimension(texture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUTextureFormat  wgpuTextureGetFormat(WGPUTexture texture) {
return     procs.textureGetFormat(texture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) uint32_t  wgpuTextureGetHeight(WGPUTexture texture) {
return     procs.textureGetHeight(texture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) uint32_t  wgpuTextureGetMipLevelCount(WGPUTexture texture) {
return     procs.textureGetMipLevelCount(texture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) uint32_t  wgpuTextureGetSampleCount(WGPUTexture texture) {
return     procs.textureGetSampleCount(texture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) WGPUTextureUsage  wgpuTextureGetUsage(WGPUTexture texture) {
return     procs.textureGetUsage(texture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) uint32_t  wgpuTextureGetWidth(WGPUTexture texture) {
return     procs.textureGetWidth(texture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuTextureSetLabel(WGPUTexture texture, WGPUStringView label) {
    procs.textureSetLabel(texture, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuTextureAddRef(WGPUTexture texture) {
    procs.textureAddRef(texture);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuTextureRelease(WGPUTexture texture) {
    procs.textureRelease(texture);
}

DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuTextureViewSetLabel(WGPUTextureView textureView, WGPUStringView label) {
    procs.textureViewSetLabel(textureView, label);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuTextureViewAddRef(WGPUTextureView textureView) {
    procs.textureViewAddRef(textureView);
}
DAWN_NO_SANITIZE("cfi-icall")
__attribute__((weak)) void  wgpuTextureViewRelease(WGPUTextureView textureView) {
    procs.textureViewRelease(textureView);
}


