/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_Caps_DEFINED
#define skgpu_Caps_DEFINED

#include "experimental/graphite/src/ResourceTypes.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkRefCnt.h"
#include "src/gpu/ResourceKey.h"

namespace SkSL {
struct ShaderCaps;
}

namespace skgpu {

class GraphicsPipelineDesc;
class GraphiteResourceKey;
struct RenderPassDesc;
class TextureInfo;

class Caps : public SkRefCnt {
public:
    ~Caps() override;

    const SkSL::ShaderCaps* shaderCaps() const { return fShaderCaps.get(); }

    virtual TextureInfo getDefaultSampledTextureInfo(SkColorType,
                                                     uint32_t levelCount,
                                                     Protected,
                                                     Renderable) const = 0;

    virtual TextureInfo getDefaultMSAATextureInfo(SkColorType,
                                                  uint32_t sampleCount,
                                                  Protected) const = 0;

    virtual TextureInfo getDefaultDepthStencilTextureInfo(Mask<DepthStencilFlags>,
                                                          uint32_t sampleCount,
                                                          Protected) const = 0;

    virtual UniqueKey makeGraphicsPipelineKey(const GraphicsPipelineDesc&,
                                              const RenderPassDesc&) const = 0;

    bool areColorTypeAndTextureInfoCompatible(SkColorType, const TextureInfo&) const;

    bool isTexturable(const TextureInfo&) const;
    virtual bool isRenderable(const TextureInfo&) const = 0;

    int maxTextureSize() const { return fMaxTextureSize; }

    virtual void buildKeyForTexture(SkISize dimensions,
                                    const TextureInfo&,
                                    ResourceType,
                                    Shareable,
                                    GraphiteResourceKey*) const = 0;

    // Returns the required alignment in bytes for the offset into a uniform buffer when binding it
    // to a draw.
    size_t requiredUniformBufferAlignment() const { return fRequiredUniformBufferAlignment; }

    // Returns the alignment in bytes for the offset into a Buffer when using it
    // to transfer to or from a Texture with the given bytes per pixel.
    virtual size_t getTransferBufferAlignment(size_t bytesPerPixel) const = 0;

    bool clampToBorderSupport() const { return fClampToBorderSupport; }

protected:
    Caps();

    int fMaxTextureSize = 0;
    size_t fRequiredUniformBufferAlignment = 0;

    std::unique_ptr<SkSL::ShaderCaps> fShaderCaps;

    bool fClampToBorderSupport = true;

private:
    virtual bool onIsTexturable(const TextureInfo&) const = 0;
    virtual bool onAreColorTypeAndTextureInfoCompatible(SkColorType, const TextureInfo&) const = 0;
};

} // namespace skgpu

#endif // skgpu_Caps_DEFINED
