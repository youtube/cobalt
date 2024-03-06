/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "experimental/graphite/src/CommandBuffer.h"

#include "experimental/graphite/src/GraphicsPipeline.h"
#include "src/core/SkTraceEvent.h"

#include "experimental/graphite/src/Buffer.h"
#include "experimental/graphite/src/Sampler.h"
#include "experimental/graphite/src/Texture.h"
#include "experimental/graphite/src/TextureProxy.h"

namespace skgpu {

CommandBuffer::CommandBuffer() {}

CommandBuffer::~CommandBuffer() {
    this->releaseResources();
}

void CommandBuffer::releaseResources() {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);

    fTrackedResources.reset();
}

void CommandBuffer::trackResource(sk_sp<Resource> resource) {
    fTrackedResources.push_back(std::move(resource));
}

bool CommandBuffer::beginRenderPass(const RenderPassDesc& renderPassDesc,
                                    sk_sp<Texture> colorTexture,
                                    sk_sp<Texture> resolveTexture,
                                    sk_sp<Texture> depthStencilTexture) {
    if (!this->onBeginRenderPass(renderPassDesc, colorTexture.get(), resolveTexture.get(),
                                 depthStencilTexture.get())) {
        return false;
    }

    if (colorTexture) {
        this->trackResource(std::move(colorTexture));
    }
    if (resolveTexture) {
        this->trackResource(std::move(resolveTexture));
    }
    if (depthStencilTexture) {
        this->trackResource(std::move(depthStencilTexture));
    }
#ifdef SK_DEBUG
    if (renderPassDesc.fColorAttachment.fLoadOp == LoadOp::kClear &&
        (renderPassDesc.fColorAttachment.fStoreOp == StoreOp::kStore ||
         renderPassDesc.fColorResolveAttachment.fStoreOp == StoreOp::kStore)) {
        fHasWork = true;
    }
#endif

    return true;
}

void CommandBuffer::bindGraphicsPipeline(sk_sp<GraphicsPipeline> graphicsPipeline) {
    this->onBindGraphicsPipeline(graphicsPipeline.get());
    this->trackResource(std::move(graphicsPipeline));
}

void CommandBuffer::bindUniformBuffer(UniformSlot slot,
                                      sk_sp<Buffer> uniformBuffer,
                                      size_t offset) {
    this->onBindUniformBuffer(slot, uniformBuffer.get(), offset);
    this->trackResource(std::move(uniformBuffer));
}

void CommandBuffer::bindVertexBuffers(sk_sp<Buffer> vertexBuffer, size_t vertexOffset,
                                      sk_sp<Buffer> instanceBuffer, size_t instanceOffset) {
    this->onBindVertexBuffers(vertexBuffer.get(), vertexOffset,
                              instanceBuffer.get(), instanceOffset);
    if (vertexBuffer) {
        this->trackResource(std::move(vertexBuffer));
    }
    if (instanceBuffer) {
        this->trackResource(std::move(instanceBuffer));
    }
}

void CommandBuffer::bindIndexBuffer(sk_sp<Buffer> indexBuffer, size_t bufferOffset) {
    this->onBindIndexBuffer(indexBuffer.get(), bufferOffset);
    if (indexBuffer) {
        this->trackResource(std::move(indexBuffer));
    }
}

void CommandBuffer::bindDrawBuffers(BindBufferInfo vertices,
                                    BindBufferInfo instances,
                                    BindBufferInfo indices) {
    this->bindVertexBuffers(sk_ref_sp(vertices.fBuffer), vertices.fOffset,
                            sk_ref_sp(instances.fBuffer), instances.fOffset);
    this->bindIndexBuffer(sk_ref_sp(indices.fBuffer), indices.fOffset);
}

void CommandBuffer::bindTextures(const TextureBindEntry* entries, int count) {
    this->onBindTextures(entries, count);
    for (int i = 0; i < count; ++i) {
        SkASSERT(entries[i].fTexture);
        this->trackResource(entries[i].fTexture);
    }
}

void CommandBuffer::bindSamplers(const SamplerBindEntry* entries, int count) {
    this->onBindSamplers(entries, count);
    for (int i = 0; i < count; ++i) {
        SkASSERT(entries[i].fSampler);
        this->trackResource(entries[i].fSampler);
    }
}

bool CommandBuffer::copyTextureToBuffer(sk_sp<skgpu::Texture> texture,
                                        SkIRect srcRect,
                                        sk_sp<skgpu::Buffer> buffer,
                                        size_t bufferOffset,
                                        size_t bufferRowBytes) {
    SkASSERT(texture);
    SkASSERT(buffer);

    if (!this->onCopyTextureToBuffer(texture.get(), srcRect, buffer.get(), bufferOffset,
                                     bufferRowBytes)) {
        return false;
    }

    this->trackResource(std::move(texture));
    this->trackResource(std::move(buffer));

    SkDEBUGCODE(fHasWork = true;)

    return true;
}

bool CommandBuffer::copyBufferToTexture(sk_sp<skgpu::Buffer> buffer,
                                        sk_sp<skgpu::Texture> texture,
                                        const BufferTextureCopyData* copyData,
                                        int count) {
    SkASSERT(buffer);
    SkASSERT(texture);
    SkASSERT(count > 0 && copyData);

    if (!this->onCopyBufferToTexture(buffer.get(), texture.get(), copyData, count)) {
        return false;
    }

    this->trackResource(std::move(buffer));
    this->trackResource(std::move(texture));

    SkDEBUGCODE(fHasWork = true;)

    return true;
}

} // namespace skgpu
