/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkKeyHelpers.h"

#include "src/core/SkDebugUtils.h"
#include "src/core/SkPaintParamsKey.h"
#include "src/core/SkShaderCodeDictionary.h"
#include "src/core/SkUniform.h"
#include "src/core/SkUniformData.h"
#include "src/shaders/SkShaderBase.h"

#ifdef SK_GRAPHITE_ENABLED
#include "experimental/graphite/src/UniformManager.h"
#endif

namespace {

#if defined(SK_DEBUG) && defined(SK_GRAPHITE_ENABLED)
SkBuiltInCodeSnippetID read_code_snippet_id(const SkPaintParamsKey& key, int headerOffset) {
    uint8_t byte = key.byte(headerOffset);

    SkASSERT(byte <= static_cast<int>(SkBuiltInCodeSnippetID::kLast));

    return static_cast<SkBuiltInCodeSnippetID>(byte);
}

void validate_block_header_key(const SkPaintParamsKey& key,
                               int headerOffset,
                               SkBuiltInCodeSnippetID codeSnippetID,
                               int blockDataSize) {
    SkASSERT(key.byte(headerOffset) == static_cast<int>(codeSnippetID));
    SkASSERT(key.byte(headerOffset+SkPaintParamsKey::kBlockSizeOffsetInBytes) ==
             SkPaintParamsKey::kBlockHeaderSizeInBytes + blockDataSize);
}
#endif

// This can be used to catch errors in blocks that have a fixed, known block data size
void validate_block_header(const SkPaintParamsKeyBuilder* builder,
                           SkBuiltInCodeSnippetID codeSnippetID,
                           int blockDataSize) {
    SkDEBUGCODE(int fullBlockSize = SkPaintParamsKey::kBlockHeaderSizeInBytes + blockDataSize;)
    SkDEBUGCODE(int headerOffset = builder->sizeInBytes() - fullBlockSize;)
    SkASSERT(builder->byte(headerOffset) == static_cast<int>(codeSnippetID));
    SkASSERT(builder->byte(headerOffset+SkPaintParamsKey::kBlockSizeOffsetInBytes) ==
             fullBlockSize);
}

#ifdef SK_GRAPHITE_ENABLED
void add_blendmode_to_key(SkPaintParamsKeyBuilder* builder, SkBlendMode bm) {
    SkASSERT(static_cast<int>(bm) <= std::numeric_limits<uint8_t>::max());
    builder->addByte(static_cast<uint8_t>(bm));
}

#ifdef SK_DEBUG
SkBlendMode to_blendmode(uint8_t data) {
    SkASSERT(data <= static_cast<int>(SkBlendMode::kLastMode));
    return static_cast<SkBlendMode>(data);
}

SkTileMode to_tilemode(uint8_t data) {
    SkASSERT(data <= static_cast<int>(SkTileMode::kLastTileMode));
    return static_cast<SkTileMode>(data);
}
#endif // SK_DEBUG

#endif // SK_GRAPHITE_ENABLED

} // anonymous namespace

//--------------------------------------------------------------------------------------------------
namespace DepthStencilOnlyBlock {

static const int kBlockDataSize = 0;

void AddToKey(SkShaderCodeDictionary* /* dict */,
              SkBackend /* backend */,
              SkPaintParamsKeyBuilder* builder,
              SkUniformBlock* /* uniformBlock */) {
    builder->beginBlock(SkBuiltInCodeSnippetID::kDepthStencilOnlyDraw);
    builder->endBlock();

    validate_block_header(builder,
                          SkBuiltInCodeSnippetID::kDepthStencilOnlyDraw,
                          kBlockDataSize);
}

#ifdef SK_DEBUG
void Dump(const SkPaintParamsKey& key, int headerOffset) {
#ifdef SK_GRAPHITE_ENABLED
    validate_block_header_key(key,
                              headerOffset,
                              SkBuiltInCodeSnippetID::kDepthStencilOnlyDraw,
                              kBlockDataSize);

    SkDebugf("kDepthStencilOnlyDraw\n");
#endif // SK_GRAPHITE_ENABLED
}
#endif // SK_DEBUG

} // namespace DepthStencilOnlyBlock

//--------------------------------------------------------------------------------------------------
namespace SolidColorShaderBlock {

namespace {

#ifdef SK_GRAPHITE_ENABLED
static const int kBlockDataSize = 0;

sk_sp<SkUniformData> make_solid_uniform_data(SkShaderCodeDictionary* dict, SkColor4f color) {
    static constexpr size_t kExpectedNumUniforms = 1;

    SkSpan<const SkUniform> uniforms = dict->getUniforms(SkBuiltInCodeSnippetID::kSolidColorShader);
    SkASSERT(uniforms.size() == kExpectedNumUniforms);

    skgpu::UniformManager mgr(skgpu::Layout::kMetal);

    size_t dataSize = mgr.writeUniforms(uniforms, nullptr, nullptr, nullptr);

    sk_sp<SkUniformData> result = SkUniformData::Make(uniforms, dataSize);

    const void* srcs[kExpectedNumUniforms] = { &color };

    mgr.writeUniforms(result->uniforms(), srcs, result->offsets(), result->data());
    return result;
}
#endif // SK_GRAPHITE_ENABLED

} // anonymous namespace

void AddToKey(SkShaderCodeDictionary* dict,
              SkBackend backend,
              SkPaintParamsKeyBuilder* builder,
              SkUniformBlock* uniformBlock,
              const SkColor4f& color) {

#ifdef SK_GRAPHITE_ENABLED
    if (backend == SkBackend::kGraphite) {
        builder->beginBlock(SkBuiltInCodeSnippetID::kSolidColorShader);
        builder->endBlock();

        validate_block_header(builder,
                              SkBuiltInCodeSnippetID::kSolidColorShader,
                              kBlockDataSize);

        if (uniformBlock) {
            uniformBlock->add(make_solid_uniform_data(dict, color));
        }
        return;
    }
#endif // SK_GRAPHITE_ENABLED

    if (backend == SkBackend::kSkVM || backend == SkBackend::kGanesh) {
        // TODO: add implementation of other backends
    }

}

#ifdef SK_DEBUG
void Dump(const SkPaintParamsKey& key, int headerOffset) {

#ifdef SK_GRAPHITE_ENABLED
    validate_block_header_key(key,
                              headerOffset,
                              SkBuiltInCodeSnippetID::kSolidColorShader,
                              kBlockDataSize);

    SkDebugf("kSolidColorShader\n");
#endif

}
#endif

} // namespace SolidColorShaderBlock

//--------------------------------------------------------------------------------------------------
namespace GradientShaderBlocks {

namespace {

#ifdef SK_GRAPHITE_ENABLED
static const int kBlockDataSize = 1;
static const int kExpectedNumGradientUniforms = 7;

sk_sp<SkUniformData> make_gradient_uniform_data_common(
        SkSpan<const SkUniform> uniforms,
        const void* srcs[kExpectedNumGradientUniforms]) {
    skgpu::UniformManager mgr(skgpu::Layout::kMetal);

    // TODO: Given that, for the sprint, we always know the uniforms we could cache 'dataSize'
    // for each layout and skip the first call.
    size_t dataSize = mgr.writeUniforms(uniforms, nullptr, nullptr, nullptr);

    sk_sp<SkUniformData> result = SkUniformData::Make(uniforms, dataSize);

    mgr.writeUniforms(result->uniforms(), srcs, result->offsets(), result->data());
    return result;
}

sk_sp<SkUniformData> make_linear_gradient_uniform_data(SkShaderCodeDictionary* dict,
                                                       const GradientData& gradData) {

    auto uniforms = dict->getUniforms(SkBuiltInCodeSnippetID::kLinearGradientShader);
    SkASSERT(uniforms.size() == kExpectedNumGradientUniforms);

    SkPoint padding{0, 0};
    const void* srcs[kExpectedNumGradientUniforms] = {
        gradData.fColor4fs,
        gradData.fOffsets,
        &gradData.fPoints[0],
        &gradData.fPoints[1],
        &gradData.fRadii[0], // unused
        &gradData.fRadii[1], // unused
        &padding
    };

    return make_gradient_uniform_data_common(uniforms, srcs);
};

sk_sp<SkUniformData> make_radial_gradient_uniform_data(SkShaderCodeDictionary* dict,
                                                       const GradientData& gradData) {

    auto uniforms = dict->getUniforms(SkBuiltInCodeSnippetID::kRadialGradientShader);
    SkASSERT(uniforms.size() == kExpectedNumGradientUniforms);

    SkPoint padding{0, 0};
    const void* srcs[kExpectedNumGradientUniforms] = {
        gradData.fColor4fs,
        gradData.fOffsets,
        &gradData.fPoints[0],
        &gradData.fPoints[1], // unused
        &gradData.fRadii[0],
        &gradData.fRadii[1],  // unused
        &padding
    };

    return make_gradient_uniform_data_common(uniforms, srcs);
};

sk_sp<SkUniformData> make_sweep_gradient_uniform_data(SkShaderCodeDictionary* dict,
                                                      const GradientData& gradData) {

    auto uniforms = dict->getUniforms(SkBuiltInCodeSnippetID::kSweepGradientShader);
    SkASSERT(uniforms.size() == kExpectedNumGradientUniforms);

    SkPoint padding{0, 0};
    const void* srcs[kExpectedNumGradientUniforms] = {
        gradData.fColor4fs,
        gradData.fOffsets,
        &gradData.fPoints[0],
        &gradData.fPoints[1], // unused
        &gradData.fRadii[0],  // unused
        &gradData.fRadii[1],  // unused
        &padding
    };

    return make_gradient_uniform_data_common(uniforms, srcs);
};

sk_sp<SkUniformData> make_conical_gradient_uniform_data(SkShaderCodeDictionary* dict,
                                                        const GradientData& gradData) {

    auto uniforms = dict->getUniforms(SkBuiltInCodeSnippetID::kConicalGradientShader);
    SkASSERT(uniforms.size() == kExpectedNumGradientUniforms);

    SkPoint padding{0, 0};
    const void* srcs[kExpectedNumGradientUniforms] = {
        gradData.fColor4fs,
        gradData.fOffsets,
        &gradData.fPoints[0],
        &gradData.fPoints[1],
        &gradData.fRadii[0],
        &gradData.fRadii[1],
        &padding,
    };

    return make_gradient_uniform_data_common(uniforms, srcs);
};

#endif // SK_GRAPHITE_ENABLED

} // anonymous namespace

GradientData::GradientData(SkShader::GradientType type,
                           SkTileMode tm,
                           int numStops)
        : fType(type)
        , fPoints{{0.0f, 0.0f}, {0.0f, 0.0f}}
        , fRadii{0.0f, 0.0f}
        , fTM(tm)
        , fNumStops(numStops) {
    sk_bzero(fColor4fs, sizeof(fColor4fs));
    sk_bzero(fOffsets, sizeof(fOffsets));
}

GradientData::GradientData(SkShader::GradientType type,
                           SkPoint point0, SkPoint point1,
                           float radius0, float radius1,
                           SkTileMode tm,
                           int numStops,
                           SkColor4f* color4fs,
                           float* offsets)
        : fType(type)
        , fTM(tm)
        , fNumStops(std::min(numStops, kMaxStops)) {
    SkASSERT(fNumStops >= 1);

    fPoints[0] = point0;
    fPoints[1] = point1;
    fRadii[0] = radius0;
    fRadii[1] = radius1;
    memcpy(fColor4fs, color4fs, fNumStops * sizeof(SkColor4f));
    if (offsets) {
        memcpy(fOffsets, offsets, fNumStops * sizeof(float));
    } else {
        for (int i = 0; i < fNumStops; ++i) {
            fOffsets[i] = SkIntToFloat(i) / (fNumStops-1);
        }
    }

    // Extend the colors and offset, if necessary, to fill out the arrays
    // TODO: this should be done later when the actual code snippet has been selected!!
    for (int i = fNumStops ; i < kMaxStops; ++i) {
        fColor4fs[i] = fColor4fs[fNumStops-1];
        fOffsets[i] = fOffsets[fNumStops-1];
    }
}

void AddToKey(SkShaderCodeDictionary* dict,
              SkBackend backend,
              SkPaintParamsKeyBuilder *builder,
              SkUniformBlock* uniformBlock,
              const GradientData& gradData) {

#ifdef SK_GRAPHITE_ENABLED
    if (backend == SkBackend::kGraphite) {
        SkBuiltInCodeSnippetID codeSnippetID = SkBuiltInCodeSnippetID::kSolidColorShader;
        switch (gradData.fType) {
            case SkShader::kLinear_GradientType:
                codeSnippetID = SkBuiltInCodeSnippetID::kLinearGradientShader;
                if (uniformBlock) {
                    uniformBlock->add(make_linear_gradient_uniform_data(dict, gradData));
                }
                break;
            case SkShader::kRadial_GradientType:
                codeSnippetID = SkBuiltInCodeSnippetID::kRadialGradientShader;
                if (uniformBlock) {
                    uniformBlock->add(make_radial_gradient_uniform_data(dict, gradData));
                }
                break;
            case SkShader::kSweep_GradientType:
                codeSnippetID = SkBuiltInCodeSnippetID::kSweepGradientShader;
                if (uniformBlock) {
                    uniformBlock->add(make_sweep_gradient_uniform_data(dict, gradData));
                }
                break;
            case SkShader::GradientType::kConical_GradientType:
                codeSnippetID = SkBuiltInCodeSnippetID::kConicalGradientShader;
                if (uniformBlock) {
                    uniformBlock->add(make_conical_gradient_uniform_data(dict, gradData));
                }
                break;
            case SkShader::GradientType::kColor_GradientType:
            case SkShader::GradientType::kNone_GradientType:
            default:
                SkASSERT(0);
                break;
        }

        builder->beginBlock(codeSnippetID);

        SkASSERT(static_cast<int>(gradData.fTM) <= std::numeric_limits<uint8_t>::max());
        builder->addByte(static_cast<uint8_t>(gradData.fTM));

        builder->endBlock();

        validate_block_header(builder, codeSnippetID, kBlockDataSize);
        return;
    }
#endif // SK_GRAPHITE_ENABLED

    if (backend == SkBackend::kSkVM || backend == SkBackend::kGanesh) {
        // TODO: add implementation of other backends
        SolidColorShaderBlock::AddToKey(dict, backend, builder, uniformBlock, SkColors::kRed);
    }
}

#ifdef SK_DEBUG

#ifdef SK_GRAPHITE_ENABLED

std::pair<SkBuiltInCodeSnippetID, SkTileMode> ExtractFromKey(const SkPaintParamsKey& key,
                                                             uint32_t headerOffset) {
    SkBuiltInCodeSnippetID id = read_code_snippet_id(key, headerOffset);

    SkASSERT(id == SkBuiltInCodeSnippetID::kLinearGradientShader ||
             id == SkBuiltInCodeSnippetID::kRadialGradientShader ||
             id == SkBuiltInCodeSnippetID::kSweepGradientShader ||
             id == SkBuiltInCodeSnippetID::kConicalGradientShader);
    SkASSERT(key.byte(headerOffset+SkPaintParamsKey::kBlockSizeOffsetInBytes) ==
             SkPaintParamsKey::kBlockHeaderSizeInBytes+kBlockDataSize);

    uint8_t data = key.byte(headerOffset + SkPaintParamsKey::kBlockHeaderSizeInBytes);
    SkTileMode tm = to_tilemode(data);

    return { id, tm };
}

#endif // SK_GRAPHITE_ENABLED

void Dump(const SkPaintParamsKey& key, int headerOffset) {

#ifdef SK_GRAPHITE_ENABLED
    auto [id, tm] =  ExtractFromKey(key, headerOffset);

    switch (id) {
        case SkBuiltInCodeSnippetID::kLinearGradientShader:
            SkDebugf("kLinearGradientShader: %s\n", SkTileModeToStr(tm));
            break;
        case SkBuiltInCodeSnippetID::kRadialGradientShader:
            SkDebugf("kRadialGradientShader: %s\n", SkTileModeToStr(tm));
            break;
        case SkBuiltInCodeSnippetID::kSweepGradientShader:
            SkDebugf("kSweepGradientShader: %s\n", SkTileModeToStr(tm));
            break;
        case SkBuiltInCodeSnippetID::kConicalGradientShader:
            SkDebugf("kConicalGradientShader: %s\n", SkTileModeToStr(tm));
            break;
        default:
            SkDebugf("Unknown!!\n");
            break;
    }
#endif // SK_GRAPHITE_ENABLED

}
#endif

} // namespace GradientShaderBlocks

//--------------------------------------------------------------------------------------------------
namespace ImageShaderBlock {

namespace {

#ifdef SK_GRAPHITE_ENABLED

inline static constexpr int kTileModeBits = 2;

static const int kXTileModeShift = 0;
static const int kYTileModeShift = kTileModeBits;

#ifdef SK_DEBUG
static const int kBlockDataSize = 1;

inline static constexpr int kTileModeMask = 0x3;

ImageData ExtractFromKey(const SkPaintParamsKey& key, uint32_t headerOffset) {
    validate_block_header_key(key,
                              headerOffset,
                              SkBuiltInCodeSnippetID::kImageShader,
                              kBlockDataSize);

    uint8_t data = key.byte(headerOffset+SkPaintParamsKey::kBlockHeaderSizeInBytes);

    SkTileMode tmX = to_tilemode(((data) >> kXTileModeShift) & kTileModeMask);
    SkTileMode tmY = to_tilemode(((data) >> kYTileModeShift) & kTileModeMask);

    return { tmX, tmY };
}
#endif // SK_DEBUG

sk_sp<SkUniformData> make_image_uniform_data(SkShaderCodeDictionary* dict,
                                             const ImageData& imgData) {
    SkDEBUGCODE(static constexpr size_t kExpectedNumUniforms = 0;)

    SkSpan<const SkUniform> uniforms = dict->getUniforms(SkBuiltInCodeSnippetID::kImageShader);
    SkASSERT(uniforms.size() == kExpectedNumUniforms);

    skgpu::UniformManager mgr(skgpu::Layout::kMetal);

    size_t dataSize = mgr.writeUniforms(uniforms, nullptr, nullptr, nullptr);

    sk_sp<SkUniformData> result = SkUniformData::Make(uniforms, dataSize);

    // TODO: add the required data to ImageData and assemble the uniforms here

    mgr.writeUniforms(result->uniforms(), nullptr, result->offsets(), result->data());
    return result;
}

#endif // SK_GRAPHITE_ENABLED

} // anonymous namespace

void AddToKey(SkShaderCodeDictionary* dict,
              SkBackend backend,
              SkPaintParamsKeyBuilder* builder,
              SkUniformBlock* uniformBlock,
              const ImageData& imgData) {

#ifdef SK_GRAPHITE_ENABLED
    if (backend == SkBackend::kGraphite) {

        uint8_t data = (static_cast<uint8_t>(imgData.fTileModes[0]) << kXTileModeShift) |
                       (static_cast<uint8_t>(imgData.fTileModes[1]) << kYTileModeShift);

        builder->beginBlock(SkBuiltInCodeSnippetID::kImageShader);

        builder->addByte(data);

        builder->endBlock();

        if (uniformBlock) {
            uniformBlock->add(make_image_uniform_data(dict, imgData));
        }
        return;
    }
#endif // SK_GRAPHITE_ENABLED

    if (backend == SkBackend::kSkVM || backend == SkBackend::kGanesh) {
        // TODO: add implementation for other backends
        SolidColorShaderBlock::AddToKey(dict, backend, builder, uniformBlock, SkColors::kRed);
    }
}

#ifdef SK_DEBUG
void Dump(const SkPaintParamsKey& key, int headerOffset) {

#ifdef SK_GRAPHITE_ENABLED
    ImageData imgData = ExtractFromKey(key, headerOffset);

    SkDebugf("kImageShader: tileModes(%s, %s) ",
             SkTileModeToStr(imgData.fTileModes[0]),
             SkTileModeToStr(imgData.fTileModes[1]));
#endif // SK_GRAPHITE_ENABLED

}
#endif // SK_DEBUG

} // namespace ImageShaderBlock

//--------------------------------------------------------------------------------------------------
namespace BlendShaderBlock {

void AddToKey(SkShaderCodeDictionary* dict,
              SkBackend backend,
              SkPaintParamsKeyBuilder *builder,
              SkUniformBlock* uniformBlock,
              const BlendData& blendData) {

#ifdef SK_GRAPHITE_ENABLED
    if (backend == SkBackend::kGraphite) {
        builder->beginBlock(SkBuiltInCodeSnippetID::kBlendShader);

        // Child blocks always go right after the parent block's header
        // TODO: add startChild/endChild entry points to SkPaintParamsKeyBuilder. They could be
        // used to compute and store the number of children w/in a block's header.
        int start = builder->sizeInBytes();
        as_SB(blendData.fDst)->addToKey(dict, backend, builder, uniformBlock);
        int firstShaderSize = builder->sizeInBytes() - start;

        start = builder->sizeInBytes();
        as_SB(blendData.fSrc)->addToKey(dict, backend, builder, uniformBlock);
        int secondShaderSize = builder->sizeInBytes() - start;

        add_blendmode_to_key(builder, blendData.fBM);

        builder->endBlock();

        int expectedBlockSize = 1 + firstShaderSize + secondShaderSize;
        validate_block_header(builder,
                              SkBuiltInCodeSnippetID::kBlendShader,
                              expectedBlockSize);
        return;
    }
#endif // SK_GRAPHITE_ENABLED

    if (backend == SkBackend::kSkVM || backend == SkBackend::kGanesh) {
        // TODO: add implementation for other backends
        SolidColorShaderBlock::AddToKey(dict, backend, builder, uniformBlock, SkColors::kRed);
    }
}

#ifdef SK_DEBUG
void Dump(const SkPaintParamsKey& key, int headerOffset) {
#ifdef SK_GRAPHITE_ENABLED
    auto [id, storedBlockSize] = key.readCodeSnippetID(headerOffset);
    SkASSERT(id == SkBuiltInCodeSnippetID::kBlendShader);

    int runningOffset = headerOffset + SkPaintParamsKey::kBlockHeaderSizeInBytes;

    SkDebugf("\nDst:  ");
    int firstBlockSize = SkPaintParamsKey::DumpBlock(key, runningOffset);
    runningOffset += firstBlockSize;

    SkDebugf("Src: ");
    int secondBlockSize = SkPaintParamsKey::DumpBlock(key, runningOffset);
    runningOffset += secondBlockSize;

    uint8_t data = key.byte(runningOffset);
    SkBlendMode bm = to_blendmode(data);

    SkDebugf("BlendMode: %s\n", SkBlendMode_Name(bm));
    runningOffset += 1; // 1 byte for blendmode

    int calculatedBlockSize = SkPaintParamsKey::kBlockHeaderSizeInBytes +
                              firstBlockSize + secondBlockSize + 1;
    SkASSERT(calculatedBlockSize == storedBlockSize);
#endif// SK_GRAPHITE_ENABLED
}
#endif

} // namespace BlendShaderBlock

//--------------------------------------------------------------------------------------------------
namespace BlendModeBlock {

#ifdef SK_GRAPHITE_ENABLED
static const int kBlockDataSize = 1;
#endif

void AddToKey(SkShaderCodeDictionary* dict,
              SkBackend backend,
              SkPaintParamsKeyBuilder *builder,
              SkUniformBlock* uniformBlock,
              SkBlendMode bm) {

#ifdef SK_GRAPHITE_ENABLED
    if (backend == SkBackend::kGraphite) {
        builder->beginBlock(SkBuiltInCodeSnippetID::kSimpleBlendMode);
        add_blendmode_to_key(builder, bm);
        builder->endBlock();

        validate_block_header(builder,
                              SkBuiltInCodeSnippetID::kSimpleBlendMode,
                              kBlockDataSize);
        return;
    }
#endif// SK_GRAPHITE_ENABLED

    if (backend == SkBackend::kSkVM || backend == SkBackend::kGanesh) {
        // TODO: add implementation for other backends
        SolidColorShaderBlock::AddToKey(dict, backend, builder, uniformBlock, SkColors::kRed);
    }
}

#ifdef SK_DEBUG

#ifdef SK_GRAPHITE_ENABLED
SkBlendMode ExtractFromKey(const SkPaintParamsKey& key, uint32_t headerOffset) {
    validate_block_header_key(key,
                              headerOffset,
                              SkBuiltInCodeSnippetID::kSimpleBlendMode,
                              kBlockDataSize);

    uint8_t data = key.byte(headerOffset + SkPaintParamsKey::kBlockHeaderSizeInBytes);
    return to_blendmode(data);
}
#endif // SK_GRAPHITE_ENABLED

void Dump(const SkPaintParamsKey& key, int headerOffset) {

#ifdef SK_GRAPHITE_ENABLED
    SkBlendMode bm = ExtractFromKey(key, headerOffset);

    SkDebugf("kSimpleBlendMode: %s\n", SkBlendMode_Name(bm));
#endif

}
#endif

} // namespace BlendModeBlock

//--------------------------------------------------------------------------------------------------
#ifdef SK_GRAPHITE_ENABLED
std::unique_ptr<SkPaintParamsKey> CreateKey(SkShaderCodeDictionary* dict,
                                            SkBackend backend,
                                            skgpu::ShaderCombo::ShaderType s,
                                            SkTileMode tm,
                                            SkBlendMode bm) {
    SkPaintParamsKeyBuilder builder(dict);

    switch (s) {
        case skgpu::ShaderCombo::ShaderType::kNone:
            DepthStencilOnlyBlock::AddToKey(dict, backend, &builder, nullptr);
            break;
        case skgpu::ShaderCombo::ShaderType::kSolidColor:
            SolidColorShaderBlock::AddToKey(dict, backend, &builder, nullptr, SkColors::kRed);
            break;
        case skgpu::ShaderCombo::ShaderType::kLinearGradient:
            GradientShaderBlocks::AddToKey(dict, backend, &builder, nullptr,
                                           { SkShader::kLinear_GradientType, tm, 0 });
            break;
        case skgpu::ShaderCombo::ShaderType::kRadialGradient:
            GradientShaderBlocks::AddToKey(dict, backend, &builder, nullptr,
                                           { SkShader::kRadial_GradientType, tm, 0 });
            break;
        case skgpu::ShaderCombo::ShaderType::kSweepGradient:
            GradientShaderBlocks::AddToKey(dict, backend, &builder, nullptr,
                                           { SkShader::kSweep_GradientType, tm, 0 });
            break;
        case skgpu::ShaderCombo::ShaderType::kConicalGradient:
            GradientShaderBlocks::AddToKey(dict, backend, &builder, nullptr,
                                           { SkShader::kConical_GradientType, tm, 0 });
            break;
    }

    BlendModeBlock::AddToKey(dict, backend, &builder, nullptr, bm);
    return builder.snap();
}
#endif
