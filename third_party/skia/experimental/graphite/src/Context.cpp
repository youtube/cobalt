/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "experimental/graphite/include/Context.h"

#include "experimental/graphite/include/BackendTexture.h"
#include "experimental/graphite/include/Recorder.h"
#include "experimental/graphite/include/Recording.h"
#include "experimental/graphite/include/TextureInfo.h"
#include "experimental/graphite/src/Caps.h"
#include "experimental/graphite/src/CommandBuffer.h"
#include "experimental/graphite/src/GlobalCache.h"
#include "experimental/graphite/src/Gpu.h"
#include "experimental/graphite/src/GraphicsPipelineDesc.h"
#include "experimental/graphite/src/Renderer.h"
#include "experimental/graphite/src/ResourceProvider.h"
#include "include/core/SkPathTypes.h"
#include "src/core/SkKeyHelpers.h"
#include "src/core/SkShaderCodeDictionary.h"

#ifdef SK_METAL
#include "experimental/graphite/src/mtl/MtlTrampoline.h"
#endif

namespace skgpu {

Context::Context(sk_sp<Gpu> gpu, BackendApi backend)
        : fGpu(std::move(gpu))
        , fGlobalCache(sk_make_sp<GlobalCache>())
        , fBackend(backend) {
}
Context::~Context() {}

#ifdef SK_METAL
std::unique_ptr<Context> Context::MakeMetal(const mtl::BackendContext& backendContext) {
    sk_sp<Gpu> gpu = mtl::Trampoline::MakeGpu(backendContext);
    if (!gpu) {
        return nullptr;
    }

    return std::unique_ptr<Context>(new Context(std::move(gpu), BackendApi::kMetal));
}
#endif

std::unique_ptr<Recorder> Context::makeRecorder() {
    return std::unique_ptr<Recorder>(new Recorder(fGpu, fGlobalCache));
}

void Context::insertRecording(std::unique_ptr<Recording> recording) {
    fRecordings.emplace_back(std::move(recording));
}

void Context::submit(SyncToCpu syncToCpu) {
    // TODO: we want Gpu::submit to take an array of command buffers but, for now, it just takes
    // one. Once we have more than one recording queued up we will need to extract the
    // command buffers and submit them as a block.
    SkASSERT(fRecordings.size() == 1);
    fGpu->submit(fRecordings[0]->fCommandBuffer);

    fGpu->checkForFinishedWork(syncToCpu);
    fRecordings.clear();
}

void Context::preCompile(const PaintCombo& paintCombo) {
    static const Renderer* kRenderers[] = {
            &Renderer::StencilAndFillPath(SkPathFillType::kWinding),
            &Renderer::StencilAndFillPath(SkPathFillType::kEvenOdd),
            &Renderer::StencilAndFillPath(SkPathFillType::kInverseWinding),
            &Renderer::StencilAndFillPath(SkPathFillType::kInverseEvenOdd)
    };

    SkShaderCodeDictionary* dict = fGlobalCache->shaderCodeDictionary();

    for (auto bm: paintCombo.fBlendModes) {
        for (auto& shaderCombo: paintCombo.fShaders) {
            for (auto shaderType: shaderCombo.fTypes) {
                for (auto tm: shaderCombo.fTileModes) {
                    std::unique_ptr<SkPaintParamsKey> key = CreateKey(dict, SkBackend::kGraphite,
                                                                      shaderType, tm, bm);
                    auto entry = dict->findOrCreate(std::move(key));

                    GraphicsPipelineDesc desc;

                    for (const Renderer* r : kRenderers) {
                        for (auto&& s : r->steps()) {
                            if (s->performsShading()) {
                                desc.setProgram(s, entry->uniqueID());
                            }
                            // TODO: Combine with renderpass description set to generate full
                            // GraphicsPipeline and MSL program. Cache that compiled pipeline on
                            // the resource provider in a map from desc -> pipeline so that any
                            // later desc created from equivalent RenderStep + Combination get it.
                        }
                    }
                }
            }
        }
    }
    // TODO: Iterate over the renderers and make descriptions for the steps that don't perform
    // shading, and just use ShaderType::kNone.
}

BackendTexture Context::createBackendTexture(SkISize dimensions, const TextureInfo& info) {
    if (!info.isValid() || info.backend() != this->backend()) {
        return {};
    }
    return fGpu->createBackendTexture(dimensions, info);
}

void Context::deleteBackendTexture(BackendTexture& texture) {
    if (!texture.isValid() || texture.backend() != this->backend()) {
        return;
    }
    fGpu->deleteBackendTexture(texture);
}

} // namespace skgpu
