/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "experimental/graphite/src/mtl/MtlGraphicsPipeline.h"

#include "experimental/graphite/include/TextureInfo.h"
#include "experimental/graphite/src/GraphicsPipelineDesc.h"
#include "experimental/graphite/src/Log.h"
#include "experimental/graphite/src/Renderer.h"
#include "experimental/graphite/src/mtl/MtlGpu.h"
#include "experimental/graphite/src/mtl/MtlResourceProvider.h"
#include "experimental/graphite/src/mtl/MtlUtils.h"
#include "include/core/SkSpan.h"
#include "include/gpu/ShaderErrorHandler.h"
#include "include/private/SkSLString.h"
#include "src/core/SkShaderCodeDictionary.h"

namespace skgpu::mtl {

namespace {

std::string get_uniform_header(int bufferID, const char* name) {
    std::string result;

    SkSL::String::appendf(&result, "layout (binding=%d) uniform %sUniforms {\n", bufferID, name);

    return result;
}

std::string get_uniforms(SkSpan<const SkUniform> uniforms, int* offset, int manglingSuffix) {
    std::string result;

    for (auto u : uniforms) {
        int count = u.count() ? u.count() : 1;
        // TODO: this is sufficient for the sprint but should be changed to use SkSL's
        // machinery
        SkSL::String::appendf(&result, "    layout(offset=%d) ", *offset);
        switch (u.type()) {
            case SkSLType::kFloat4:
                result.append("float4");
                *offset += 16 * count;
                break;
            case SkSLType::kFloat2:
                result.append("float2");
                *offset += 8 * count;
                break;
            case SkSLType::kFloat:
                result.append("float");
                *offset += 4 * count;
                break;
            case SkSLType::kFloat4x4:
                result.append("float4x4");
                *offset += 64 * count;
                break;
            case SkSLType::kHalf4:
                result.append("half4");
                *offset += 8 * count;
                break;
            default:
                SkASSERT(0);
        }

        result.append(" ");
        result.append(u.name());
        if (manglingSuffix >= 0) {
            result.append("_");
            result.append(std::to_string(manglingSuffix));
        }
        if (u.count()) {
            result.append("[");
            result.append(std::to_string(u.count()));
            result.append("]");
        }
        result.append(";\n");
    }

    return result;
}

std::string emit_SKSL_uniforms(int bufferID, const char* name, SkSpan<const SkUniform> uniforms) {
    int offset = 0;

    std::string result = get_uniform_header(bufferID, name);
    result += get_uniforms(uniforms, &offset, -1);
    result.append("};\n\n");

    return result;
}

std::string emit_SkSL_attributes(SkSpan<const Attribute> vertexAttrs,
                                 SkSpan<const Attribute> instanceAttrs) {
    std::string result;

    int attr = 0;
    auto add_attrs = [&](SkSpan<const Attribute> attrs) {
        for (auto a : attrs) {
            // TODO: this is sufficient for the sprint but should be changed to use SkSL's
            // machinery
            SkSL::String::appendf(&result, "    layout(location=%d) in ", attr++);
            switch (a.gpuType()) {
                case SkSLType::kFloat4:
                    result.append("float4");
                    break;
                case SkSLType::kFloat2:
                    result.append("float2");
                    break;
                case SkSLType::kFloat3:
                    result.append("float3");
                    break;
                case SkSLType::kFloat:
                    result.append("float");
                    break;
                case SkSLType::kHalf4:
                    result.append("half4");
                    break;
                default:
                    SkASSERT(0);
            }

            SkSL::String::appendf(&result, " %s;\n", a.name());
        }
    };

    if (!vertexAttrs.empty()) {
        result.append("// vertex attrs\n");
        add_attrs(vertexAttrs);
    }
    if (!instanceAttrs.empty()) {
        result.append("// instance attrs\n");
        add_attrs(instanceAttrs);
    }

    return result;
}

std::string get_sksl_vs(const GraphicsPipelineDesc& desc) {
    const RenderStep* step = desc.renderStep();
    // TODO: To more completely support end-to-end rendering, this will need to be updated so that
    // the RenderStep shader snippet can produce a device coord, a local coord, and depth.
    // If the paint combination doesn't need the local coord it can be ignored, otherwise we need
    // a varying for it. The fragment function's output will need to be updated to have a color and
    // the depth, or when there's no combination, just the depth. Lastly, we also should add the
    // static/intrinsic uniform binding point so that we can handle normalizing the device position
    // produced by the RenderStep automatically.

    // Fixed program header
    std::string sksl =
        "layout (binding=0) uniform intrinsicUniforms {\n"
        "    layout(offset=0) float4 rtAdjust;\n"
        "};\n"
        "\n";

    if (step->numVertexAttributes() > 0 || step->numInstanceAttributes() > 0) {
        sksl += emit_SkSL_attributes(step->vertexAttributes(), step->instanceAttributes());
    }

    // Uniforms needed by RenderStep
    if (step->numUniforms() > 0) {
        sksl += emit_SKSL_uniforms(1, "Step", step->uniforms());
    }

    // Vertex shader function declaration
    sksl += "void main() {\n";
    // Vertex shader body
    sksl += step->vertexSkSL();
    sksl += "sk_Position = float4(devPosition.xy * rtAdjust.xy + rtAdjust.zw, devPosition.zw);\n"
            "}\n";

    return sksl;
}

std::string get_sksl_fs(SkShaderCodeDictionary* dict,
                        const GraphicsPipelineDesc& desc,
                        bool* writesColor) {
    if (!desc.paintParamsID().isValid()) {
        *writesColor = false;
        return {};
    }

    SkShaderInfo shaderInfo;

    dict->getShaderInfo(desc.paintParamsID(), &shaderInfo);

    *writesColor = shaderInfo.writesColor();
#if SK_SUPPORT_GPU
    return shaderInfo.toSkSL();
#else
    return {};
#endif
}

inline MTLVertexFormat attribute_type_to_mtlformat(VertexAttribType type) {
    switch (type) {
        case VertexAttribType::kFloat:
            return MTLVertexFormatFloat;
        case VertexAttribType::kFloat2:
            return MTLVertexFormatFloat2;
        case VertexAttribType::kFloat3:
            return MTLVertexFormatFloat3;
        case VertexAttribType::kFloat4:
            return MTLVertexFormatFloat4;
        case VertexAttribType::kHalf:
            if (@available(macOS 10.13, iOS 11.0, *)) {
                return MTLVertexFormatHalf;
            } else {
                return MTLVertexFormatInvalid;
            }
        case VertexAttribType::kHalf2:
            return MTLVertexFormatHalf2;
        case VertexAttribType::kHalf4:
            return MTLVertexFormatHalf4;
        case VertexAttribType::kInt2:
            return MTLVertexFormatInt2;
        case VertexAttribType::kInt3:
            return MTLVertexFormatInt3;
        case VertexAttribType::kInt4:
            return MTLVertexFormatInt4;
        case VertexAttribType::kByte:
            if (@available(macOS 10.13, iOS 11.0, *)) {
                return MTLVertexFormatChar;
            } else {
                return MTLVertexFormatInvalid;
            }
        case VertexAttribType::kByte2:
            return MTLVertexFormatChar2;
        case VertexAttribType::kByte4:
            return MTLVertexFormatChar4;
        case VertexAttribType::kUByte:
            if (@available(macOS 10.13, iOS 11.0, *)) {
                return MTLVertexFormatUChar;
            } else {
                return MTLVertexFormatInvalid;
            }
        case VertexAttribType::kUByte2:
            return MTLVertexFormatUChar2;
        case VertexAttribType::kUByte4:
            return MTLVertexFormatUChar4;
        case VertexAttribType::kUByte_norm:
            if (@available(macOS 10.13, iOS 11.0, *)) {
                return MTLVertexFormatUCharNormalized;
            } else {
                return MTLVertexFormatInvalid;
            }
        case VertexAttribType::kUByte4_norm:
            return MTLVertexFormatUChar4Normalized;
        case VertexAttribType::kShort2:
            return MTLVertexFormatShort2;
        case VertexAttribType::kShort4:
            return MTLVertexFormatShort4;
        case VertexAttribType::kUShort2:
            return MTLVertexFormatUShort2;
        case VertexAttribType::kUShort2_norm:
            return MTLVertexFormatUShort2Normalized;
        case VertexAttribType::kInt:
            return MTLVertexFormatInt;
        case VertexAttribType::kUInt:
            return MTLVertexFormatUInt;
        case VertexAttribType::kUShort_norm:
            if (@available(macOS 10.13, iOS 11.0, *)) {
                return MTLVertexFormatUShortNormalized;
            } else {
                return MTLVertexFormatInvalid;
            }
        case VertexAttribType::kUShort4_norm:
            return MTLVertexFormatUShort4Normalized;
    }
    SK_ABORT("Unknown vertex attribute type");
}

MTLVertexDescriptor* create_vertex_descriptor(const RenderStep* step) {
    auto vertexDescriptor = [[MTLVertexDescriptor alloc] init];
    int attributeIndex = 0;

    int vertexAttributeCount = step->numVertexAttributes();
    size_t vertexAttributeOffset = 0;
    for (const auto& attribute : step->vertexAttributes()) {
        MTLVertexAttributeDescriptor* mtlAttribute = vertexDescriptor.attributes[attributeIndex];
        MTLVertexFormat format = attribute_type_to_mtlformat(attribute.cpuType());
        SkASSERT(MTLVertexFormatInvalid != format);
        mtlAttribute.format = format;
        mtlAttribute.offset = vertexAttributeOffset;
        mtlAttribute.bufferIndex = GraphicsPipeline::kVertexBufferIndex;

        vertexAttributeOffset += attribute.sizeAlign4();
        attributeIndex++;
    }
    SkASSERT(vertexAttributeOffset == step->vertexStride());

    if (vertexAttributeCount) {
        MTLVertexBufferLayoutDescriptor* vertexBufferLayout =
                vertexDescriptor.layouts[GraphicsPipeline::kVertexBufferIndex];
        vertexBufferLayout.stepFunction = MTLVertexStepFunctionPerVertex;
        vertexBufferLayout.stepRate = 1;
        vertexBufferLayout.stride = vertexAttributeOffset;
    }

    int instanceAttributeCount = step->numInstanceAttributes();
    size_t instanceAttributeOffset = 0;
    for (const auto& attribute : step->instanceAttributes()) {
        MTLVertexAttributeDescriptor* mtlAttribute = vertexDescriptor.attributes[attributeIndex];
        MTLVertexFormat format = attribute_type_to_mtlformat(attribute.cpuType());
        SkASSERT(MTLVertexFormatInvalid != format);
        mtlAttribute.format = format;
        mtlAttribute.offset = instanceAttributeOffset;
        mtlAttribute.bufferIndex = GraphicsPipeline::kInstanceBufferIndex;

        instanceAttributeOffset += attribute.sizeAlign4();
        attributeIndex++;
    }
    SkASSERT(instanceAttributeOffset == step->instanceStride());

    if (instanceAttributeCount) {
        MTLVertexBufferLayoutDescriptor* instanceBufferLayout =
                vertexDescriptor.layouts[GraphicsPipeline::kInstanceBufferIndex];
        instanceBufferLayout.stepFunction = MTLVertexStepFunctionPerInstance;
        instanceBufferLayout.stepRate = 1;
        instanceBufferLayout.stride = instanceAttributeOffset;
    }
    return vertexDescriptor;
}

} // anonymous namespace

std::string GetMtlUniforms(int bufferID,
                           const char* name,
                           const std::vector<SkShaderInfo::SnippetEntry>& codeSnippets) {
    size_t numUniforms = 0;
    for (auto e : codeSnippets) {
        numUniforms += e.fUniforms.size();
    }

    if (!numUniforms) {
        return {};
    }

    int offset = 0;

    std::string result = get_uniform_header(bufferID, name);
    for (int i = 0; i < (int) codeSnippets.size(); ++i) {
        result += get_uniforms(codeSnippets[i].fUniforms, &offset, i);
    }
    result.append("};\n\n");

    return result;
}


enum ShaderType {
    kVertex_ShaderType = 0,
    kFragment_ShaderType = 1,

    kLast_ShaderType = kFragment_ShaderType
};
static const int kShaderTypeCount = kLast_ShaderType + 1;

sk_sp<GraphicsPipeline> GraphicsPipeline::Make(ResourceProvider* resourceProvider,
                                               const Gpu* gpu,
                                               const skgpu::GraphicsPipelineDesc& pipelineDesc,
                                               const skgpu::RenderPassDesc& renderPassDesc) {
    sk_cfp<MTLRenderPipelineDescriptor*> psoDescriptor([[MTLRenderPipelineDescriptor alloc] init]);

    std::string msl[kShaderTypeCount];
    SkSL::Program::Inputs inputs[kShaderTypeCount];
    SkSL::Program::Settings settings;

    ShaderErrorHandler* errorHandler = DefaultShaderErrorHandler();
    if (!SkSLToMSL(gpu,
                   get_sksl_vs(pipelineDesc),
                   SkSL::ProgramKind::kVertex,
                   settings,
                   &msl[kVertex_ShaderType],
                   &inputs[kVertex_ShaderType],
                   errorHandler)) {
        return nullptr;
    }

    bool writesColor;
    auto dict = resourceProvider->shaderCodeDictionary();
    if (!SkSLToMSL(gpu,
                   get_sksl_fs(dict, pipelineDesc, &writesColor),
                   SkSL::ProgramKind::kFragment,
                   settings,
                   &msl[kFragment_ShaderType],
                   &inputs[kFragment_ShaderType],
                   errorHandler)) {
        return nullptr;
    }

    sk_cfp<id<MTLLibrary>> shaderLibraries[kShaderTypeCount];

    shaderLibraries[kVertex_ShaderType] = CompileShaderLibrary(gpu,
                                                               msl[kVertex_ShaderType],
                                                               errorHandler);
    shaderLibraries[kFragment_ShaderType] = CompileShaderLibrary(gpu,
                                                                 msl[kFragment_ShaderType],
                                                                 errorHandler);
    if (!shaderLibraries[kVertex_ShaderType] || !shaderLibraries[kFragment_ShaderType]) {
        return nullptr;
    }

    (*psoDescriptor).label = @(pipelineDesc.renderStep()->name());

    (*psoDescriptor).vertexFunction =
            [shaderLibraries[kVertex_ShaderType].get() newFunctionWithName: @"vertexMain"];
    (*psoDescriptor).fragmentFunction =
            [shaderLibraries[kFragment_ShaderType].get() newFunctionWithName: @"fragmentMain"];

    // TODO: I *think* this gets cleaned up by the pipelineDescriptor?
    (*psoDescriptor).vertexDescriptor = create_vertex_descriptor(pipelineDesc.renderStep());

    // TODO: I *think* this gets cleaned up by the pipelineDescriptor as well?
    auto mtlColorAttachment = [[MTLRenderPipelineColorAttachmentDescriptor alloc] init];

    mtl::TextureInfo mtlTexInfo;
    renderPassDesc.fColorAttachment.fTextureInfo.getMtlTextureInfo(&mtlTexInfo);

    mtlColorAttachment.pixelFormat = (MTLPixelFormat)mtlTexInfo.fFormat;

    mtlColorAttachment.blendingEnabled = FALSE;

    mtlColorAttachment.writeMask = writesColor ? MTLColorWriteMaskAll : MTLColorWriteMaskNone;

    (*psoDescriptor).colorAttachments[0] = mtlColorAttachment;

    renderPassDesc.fDepthStencilAttachment.fTextureInfo.getMtlTextureInfo(&mtlTexInfo);
    MTLPixelFormat depthStencilFormat = (MTLPixelFormat)mtlTexInfo.fFormat;
    if (FormatIsStencil(depthStencilFormat)) {
        (*psoDescriptor).stencilAttachmentPixelFormat = depthStencilFormat;
    } else {
        (*psoDescriptor).stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
    }
    if (FormatIsDepth(depthStencilFormat)) {
        (*psoDescriptor).depthAttachmentPixelFormat = depthStencilFormat;
    } else {
        (*psoDescriptor).depthAttachmentPixelFormat = MTLPixelFormatInvalid;
    }

    NSError* error;
    sk_cfp<id<MTLRenderPipelineState>> pso(
            [gpu->device() newRenderPipelineStateWithDescriptor:psoDescriptor.get()
                                                          error:&error]);
    if (!pso) {
        SKGPU_LOG_E("Pipeline creation failure:\n%s", error.debugDescription.UTF8String);
        return nullptr;
    }

    const DepthStencilSettings& depthStencilSettings =
            pipelineDesc.renderStep()->depthStencilSettings();
    id<MTLDepthStencilState> dss = resourceProvider->findOrCreateCompatibleDepthStencilState(
            depthStencilSettings);

    return sk_sp<GraphicsPipeline>(
            new GraphicsPipeline(gpu,
                                 std::move(pso),
                                 dss,
                                 depthStencilSettings.fStencilReferenceValue,
                                 pipelineDesc.renderStep()->vertexStride(),
                                 pipelineDesc.renderStep()->instanceStride()));
}

void GraphicsPipeline::onFreeGpuData() {
    fPipelineState.reset();
}

} // namespace skgpu::mtl
