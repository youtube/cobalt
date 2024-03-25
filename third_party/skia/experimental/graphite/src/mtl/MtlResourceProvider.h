/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_MtlResourceProvider_DEFINED
#define skgpu_MtlResourceProvider_DEFINED

#include "experimental/graphite/src/DrawTypes.h"
#include "experimental/graphite/src/ResourceProvider.h"
#include "include/private/SkTHash.h"

#import <Metal/Metal.h>

namespace skgpu {
class CommandBuffer;
}

namespace skgpu::mtl {

class Gpu;

class ResourceProvider final : public skgpu::ResourceProvider {
public:
    ResourceProvider(const skgpu::Gpu* gpu, sk_sp<GlobalCache>, SingleOwner*);
    ~ResourceProvider() override {}

    sk_sp<skgpu::Texture> createWrappedTexture(const BackendTexture&) override;

    // Finds or creates a compatible DepthStencilState based on the enum
    id<MTLDepthStencilState> findOrCreateCompatibleDepthStencilState(const DepthStencilSettings&);

private:
    const Gpu* mtlGpu();

    sk_sp<skgpu::CommandBuffer> createCommandBuffer() override;
    sk_sp<skgpu::GraphicsPipeline> onCreateGraphicsPipeline(const GraphicsPipelineDesc&,
                                                            const RenderPassDesc&) override;
    sk_sp<skgpu::Texture> createTexture(SkISize, const skgpu::TextureInfo&) override;
    sk_sp<skgpu::Buffer> createBuffer(size_t size, BufferType type, PrioritizeGpuReads) override;

    sk_sp<skgpu::Sampler> createSampler(const SkSamplingOptions&,
                                        SkTileMode xTileMode,
                                        SkTileMode yTileMode) override;

    SkTHashMap<DepthStencilSettings, sk_cfp<id<MTLDepthStencilState>>> fDepthStencilStates;
};

} // namespace skgpu::mtl

#endif // skgpu_MtlResourceProvider_DEFINED
