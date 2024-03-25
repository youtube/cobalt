/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define SK_OPTS_NS skslc_standalone
#include "include/core/SkGraphics.h"
#include "include/core/SkStream.h"
#include "include/private/SkStringView.h"
#include "src/core/SkCpu.h"
#include "src/core/SkOpts.h"
#include "src/opts/SkChecksum_opts.h"
#include "src/opts/SkVM_opts.h"
#include "src/sksl/SkSLCompiler.h"
#include "src/sksl/SkSLDehydrator.h"
#include "src/sksl/SkSLFileOutputStream.h"
#include "src/sksl/SkSLStringStream.h"
#include "src/sksl/SkSLUtil.h"
#include "src/sksl/codegen/SkSLPipelineStageCodeGenerator.h"
#include "src/sksl/codegen/SkSLVMCodeGenerator.h"
#include "src/sksl/ir/SkSLUnresolvedFunction.h"
#include "src/sksl/ir/SkSLVarDeclarations.h"
#include "src/sksl/tracing/SkVMDebugTrace.h"
#include "src/utils/SkShaderUtils.h"
#include "src/utils/SkVMVisualizer.h"

#include "spirv-tools/libspirv.hpp"

#include <fstream>
#include <limits.h>
#include <optional>
#include <stdarg.h>
#include <stdio.h>

extern bool gSkVMAllowJIT;

void SkDebugf(const char format[], ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

namespace SkOpts {
    decltype(hash_fn) hash_fn = skslc_standalone::hash_fn;
    decltype(interpret_skvm) interpret_skvm = skslc_standalone::interpret_skvm;
}

enum class ResultCode {
    kSuccess = 0,
    kCompileError = 1,
    kInputError = 2,
    kOutputError = 3,
    kConfigurationError = 4,
};

static std::unique_ptr<SkWStream> as_SkWStream(SkSL::OutputStream& s) {
    struct Adapter : public SkWStream {
    public:
        Adapter(SkSL::OutputStream& out) : fOut(out), fBytesWritten(0) {}

        bool write(const void* buffer, size_t size) override {
            fOut.write(buffer, size);
            fBytesWritten += size;
            return true;
        }
        void flush() override {}
        size_t bytesWritten() const override { return fBytesWritten; }

    private:
        SkSL::OutputStream& fOut;
        size_t fBytesWritten;
    };

    return std::make_unique<Adapter>(s);
}

// Given the path to a file (e.g. src/gpu/effects/GrFooFragmentProcessor.fp) and the expected
// filename prefix and suffix (e.g. "Gr" and ".fp"), returns the "base name" of the
// file (in this case, 'FooFragmentProcessor'). If no match, returns the empty string.
static std::string base_name(const std::string& fpPath, const char* prefix, const char* suffix) {
    std::string result;
    const char* end = &*fpPath.end();
    const char* fileName = end;
    // back up until we find a slash
    while (fileName != fpPath && '/' != *(fileName - 1) && '\\' != *(fileName - 1)) {
        --fileName;
    }
    if (!strncmp(fileName, prefix, strlen(prefix)) &&
        !strncmp(end - strlen(suffix), suffix, strlen(suffix))) {
        result.append(fileName + strlen(prefix), end - fileName - strlen(prefix) - strlen(suffix));
    }
    return result;
}

static bool consume_suffix(std::string* str, const char suffix[]) {
    if (!skstd::ends_with(*str, suffix)) {
        return false;
    }
    str->resize(str->length() - strlen(suffix));
    return true;
}

// Given a string containing an SkSL program, searches for a #pragma settings comment, like so:
//    /*#pragma settings Default Sharpen*/
// The passed-in Settings object will be updated accordingly. Any number of options can be provided.
static bool detect_shader_settings(const std::string& text,
                                   SkSL::Program::Settings* settings,
                                   const SkSL::ShaderCaps** caps,
                                   std::unique_ptr<SkSL::SkVMDebugTrace>* debugTrace) {
    using Factory = SkSL::ShaderCapsFactory;

    // Find a matching comment and isolate the name portion.
    static constexpr char kPragmaSettings[] = "/*#pragma settings ";
    const char* settingsPtr = strstr(text.c_str(), kPragmaSettings);
    if (settingsPtr != nullptr) {
        // Subtract one here in order to preserve the leading space, which is necessary to allow
        // consumeSuffix to find the first item.
        settingsPtr += strlen(kPragmaSettings) - 1;

        const char* settingsEnd = strstr(settingsPtr, "*/");
        if (settingsEnd != nullptr) {
            std::string settingsText{settingsPtr, size_t(settingsEnd - settingsPtr)};

            // Apply settings as requested. Since they can come in any order, repeat until we've
            // consumed them all.
            for (;;) {
                const size_t startingLength = settingsText.length();

                if (consume_suffix(&settingsText, " AddAndTrueToLoopCondition")) {
                    static auto s_addAndTrueCaps = Factory::AddAndTrueToLoopCondition();
                    *caps = s_addAndTrueCaps.get();
                }
                if (consume_suffix(&settingsText, " CannotUseFractForNegativeValues")) {
                    static auto s_negativeFractCaps = Factory::CannotUseFractForNegativeValues();
                    *caps = s_negativeFractCaps.get();
                }
                if (consume_suffix(&settingsText, " CannotUseFragCoord")) {
                    static auto s_noFragCoordCaps = Factory::CannotUseFragCoord();
                    *caps = s_noFragCoordCaps.get();
                }
                if (consume_suffix(&settingsText, " CannotUseMinAndAbsTogether")) {
                    static auto s_minAbsCaps = Factory::CannotUseMinAndAbsTogether();
                    *caps = s_minAbsCaps.get();
                }
                if (consume_suffix(&settingsText, " Default")) {
                    static auto s_defaultCaps = Factory::Default();
                    *caps = s_defaultCaps.get();
                }
                if (consume_suffix(&settingsText, " EmulateAbsIntFunction")) {
                    static auto s_emulateAbsIntCaps = Factory::EmulateAbsIntFunction();
                    *caps = s_emulateAbsIntCaps.get();
                }
                if (consume_suffix(&settingsText, " FramebufferFetchSupport")) {
                    static auto s_fbFetchSupport = Factory::FramebufferFetchSupport();
                    *caps = s_fbFetchSupport.get();
                }
                if (consume_suffix(&settingsText, " IncompleteShortIntPrecision")) {
                    static auto s_incompleteShortIntCaps = Factory::IncompleteShortIntPrecision();
                    *caps = s_incompleteShortIntCaps.get();
                }
                if (consume_suffix(&settingsText, " MustGuardDivisionEvenAfterExplicitZeroCheck")) {
                    static auto s_div0Caps = Factory::MustGuardDivisionEvenAfterExplicitZeroCheck();
                    *caps = s_div0Caps.get();
                }
                if (consume_suffix(&settingsText, " MustForceNegatedAtanParamToFloat")) {
                    static auto s_negativeAtanCaps = Factory::MustForceNegatedAtanParamToFloat();
                    *caps = s_negativeAtanCaps.get();
                }
                if (consume_suffix(&settingsText, " MustForceNegatedLdexpParamToMultiply")) {
                    static auto s_negativeLdexpCaps =
                            Factory::MustForceNegatedLdexpParamToMultiply();
                    *caps = s_negativeLdexpCaps.get();
                }
                if (consume_suffix(&settingsText, " RemovePowWithConstantExponent")) {
                    static auto s_powCaps = Factory::RemovePowWithConstantExponent();
                    *caps = s_powCaps.get();
                }
                if (consume_suffix(&settingsText, " RewriteDoWhileLoops")) {
                    static auto s_rewriteLoopCaps = Factory::RewriteDoWhileLoops();
                    *caps = s_rewriteLoopCaps.get();
                }
                if (consume_suffix(&settingsText, " RewriteSwitchStatements")) {
                    static auto s_rewriteSwitchCaps = Factory::RewriteSwitchStatements();
                    *caps = s_rewriteSwitchCaps.get();
                }
                if (consume_suffix(&settingsText, " RewriteMatrixVectorMultiply")) {
                    static auto s_rewriteMatVecMulCaps = Factory::RewriteMatrixVectorMultiply();
                    *caps = s_rewriteMatVecMulCaps.get();
                }
                if (consume_suffix(&settingsText, " RewriteMatrixComparisons")) {
                    static auto s_rewriteMatrixComparisons = Factory::RewriteMatrixComparisons();
                    *caps = s_rewriteMatrixComparisons.get();
                }
                if (consume_suffix(&settingsText, " ShaderDerivativeExtensionString")) {
                    static auto s_derivativeCaps = Factory::ShaderDerivativeExtensionString();
                    *caps = s_derivativeCaps.get();
                }
                if (consume_suffix(&settingsText, " UnfoldShortCircuitAsTernary")) {
                    static auto s_ternaryCaps = Factory::UnfoldShortCircuitAsTernary();
                    *caps = s_ternaryCaps.get();
                }
                if (consume_suffix(&settingsText, " UsesPrecisionModifiers")) {
                    static auto s_precisionCaps = Factory::UsesPrecisionModifiers();
                    *caps = s_precisionCaps.get();
                }
                if (consume_suffix(&settingsText, " Version110")) {
                    static auto s_version110Caps = Factory::Version110();
                    *caps = s_version110Caps.get();
                }
                if (consume_suffix(&settingsText, " Version450Core")) {
                    static auto s_version450CoreCaps = Factory::Version450Core();
                    *caps = s_version450CoreCaps.get();
                }
                if (consume_suffix(&settingsText, " AllowNarrowingConversions")) {
                    settings->fAllowNarrowingConversions = true;
                }
                if (consume_suffix(&settingsText, " ForceHighPrecision")) {
                    settings->fForceHighPrecision = true;
                }
                if (consume_suffix(&settingsText, " NoES2Restrictions")) {
                    settings->fEnforceES2Restrictions = false;
                }
                if (consume_suffix(&settingsText, " NoInline")) {
                    settings->fInlineThreshold = 0;
                }
                if (consume_suffix(&settingsText, " NoTraceVarInSkVMDebugTrace")) {
                    settings->fAllowTraceVarInSkVMDebugTrace = false;
                }
                if (consume_suffix(&settingsText, " InlineThresholdMax")) {
                    settings->fInlineThreshold = INT_MAX;
                }
                if (consume_suffix(&settingsText, " Sharpen")) {
                    settings->fSharpenTextures = true;
                }
                if (consume_suffix(&settingsText, " SkVMDebugTrace")) {
                    settings->fOptimize = false;
                    *debugTrace = std::make_unique<SkSL::SkVMDebugTrace>();
                }

                if (settingsText.empty()) {
                    break;
                }
                if (settingsText.length() == startingLength) {
                    printf("Unrecognized #pragma settings: %s\n", settingsText.c_str());
                    return false;
                }
            }
        }
    }

    return true;
}

/**
 * Displays a usage banner; used when the command line arguments don't make sense.
 */
static void show_usage() {
    printf("usage: skslc <input> <output> <flags>\n"
           "       skslc <worklist>\n"
           "\n"
           "Allowed flags:\n"
           "--settings:   honor embedded /*#pragma settings*/ comments.\n"
           "--nosettings: ignore /*#pragma settings*/ comments\n");
}

static bool set_flag(std::optional<bool>* flag, const char* name, bool value) {
    if (flag->has_value()) {
        printf("%s flag was specified multiple times\n", name);
        return false;
    }
    *flag = value;
    return true;
}

/**
 * Handle a single input.
 */
ResultCode processCommand(const std::vector<std::string>& args) {
    std::optional<bool> honorSettings;
    std::vector<std::string> paths;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--settings") {
            if (!set_flag(&honorSettings, "settings", true)) {
                return ResultCode::kInputError;
            }
        } else if (arg == "--nosettings") {
            if (!set_flag(&honorSettings, "settings", false)) {
                return ResultCode::kInputError;
            }
        } else if (!skstd::starts_with(arg, "--")) {
            paths.push_back(arg);
        } else {
            show_usage();
            return ResultCode::kInputError;
        }
    }
    if (paths.size() != 2) {
        show_usage();
        return ResultCode::kInputError;
    }

    if (!honorSettings.has_value()) {
        honorSettings = true;
    }

    const std::string& inputPath = paths[0];
    const std::string& outputPath = paths[1];
    SkSL::ProgramKind kind;
    if (skstd::ends_with(inputPath, ".vert")) {
        kind = SkSL::ProgramKind::kVertex;
    } else if (skstd::ends_with(inputPath, ".frag") || skstd::ends_with(inputPath, ".sksl")) {
        kind = SkSL::ProgramKind::kFragment;
    } else if (skstd::ends_with(inputPath, ".rtb")) {
        kind = SkSL::ProgramKind::kRuntimeBlender;
    } else if (skstd::ends_with(inputPath, ".rtcf")) {
        kind = SkSL::ProgramKind::kRuntimeColorFilter;
    } else if (skstd::ends_with(inputPath, ".rts")) {
        kind = SkSL::ProgramKind::kRuntimeShader;
    } else {
        printf("input filename must end in '.vert', '.frag', '.rtb', '.rtcf', "
               "'.rts' or '.sksl'\n");
        return ResultCode::kInputError;
    }

    std::ifstream in(inputPath);
    std::string text((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
    if (in.rdstate()) {
        printf("error reading '%s'\n", inputPath.c_str());
        return ResultCode::kInputError;
    }

    SkSL::Program::Settings settings;
    auto standaloneCaps = SkSL::ShaderCapsFactory::Standalone();
    const SkSL::ShaderCaps* caps = standaloneCaps.get();
    std::unique_ptr<SkSL::SkVMDebugTrace> debugTrace;
    if (*honorSettings) {
        if (!detect_shader_settings(text, &settings, &caps, &debugTrace)) {
            return ResultCode::kInputError;
        }
    }

    // This tells the compiler where the rt-flip uniform will live should it be required. For
    // testing purposes we don't care where that is, but the compiler will report an error if we
    // leave them at their default invalid values, or if the offset overlaps another uniform.
    settings.fRTFlipOffset  = 16384;
    settings.fRTFlipSet     = 0;
    settings.fRTFlipBinding = 0;

    auto emitCompileError = [&](SkSL::FileOutputStream& out, const char* errorText) {
        // Overwrite the compiler output, if any, with an error message.
        out.close();
        SkSL::FileOutputStream errorStream(outputPath.c_str());
        errorStream.writeText("### Compilation failed:\n\n");
        errorStream.writeText(errorText);
        errorStream.close();
        // Also emit the error directly to stdout.
        puts(errorText);
    };

    auto compileProgram = [&](const auto& writeFn) -> ResultCode {
        SkSL::FileOutputStream out(outputPath.c_str());
        SkSL::Compiler compiler(caps);
        if (!out.isValid()) {
            printf("error writing '%s'\n", outputPath.c_str());
            return ResultCode::kOutputError;
        }
        std::unique_ptr<SkSL::Program> program = compiler.convertProgram(kind, text, settings);
        if (!program || !writeFn(compiler, *program, out)) {
            emitCompileError(out, compiler.errorText().c_str());
            return ResultCode::kCompileError;
        }
        if (!out.close()) {
            printf("error writing '%s'\n", outputPath.c_str());
            return ResultCode::kOutputError;
        }
        return ResultCode::kSuccess;
    };

    auto compileProgramForSkVM = [&](const auto& writeFn) -> ResultCode {
        if (kind == SkSL::ProgramKind::kVertex) {
            printf("%s: SkVM does not support vertex programs\n", outputPath.c_str());
            return ResultCode::kOutputError;
        }
        if (kind == SkSL::ProgramKind::kFragment) {
            // Handle .sksl and .frag programs as runtime shaders.
            kind = SkSL::ProgramKind::kRuntimeShader;
        }
        return compileProgram(writeFn);
    };

    if (skstd::ends_with(outputPath, ".spirv")) {
        return compileProgram(
                [](SkSL::Compiler& compiler, SkSL::Program& program, SkSL::OutputStream& out) {
                    return compiler.toSPIRV(program, out);
                });
    } else if (skstd::ends_with(outputPath, ".asm.frag") ||
               skstd::ends_with(outputPath, ".asm.vert")) {
        return compileProgram(
                [](SkSL::Compiler& compiler, SkSL::Program& program, SkSL::OutputStream& out) {
                    // Compile program to SPIR-V assembly in a string-stream.
                    SkSL::StringStream assembly;
                    if (!compiler.toSPIRV(program, assembly)) {
                        return false;
                    }
                    // Convert the string-stream to a SPIR-V disassembly.
                    spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_0);
                    const std::string& spirv(assembly.str());
                    std::string disassembly;
                    if (!tools.Disassemble((const uint32_t*)spirv.data(),
                                           spirv.size() / 4, &disassembly)) {
                        return false;
                    }
                    // Finally, write the disassembly to our output stream.
                    out.write(disassembly.data(), disassembly.size());
                    return true;
                });
    } else if (skstd::ends_with(outputPath, ".glsl")) {
        return compileProgram(
                [](SkSL::Compiler& compiler, SkSL::Program& program, SkSL::OutputStream& out) {
                    return compiler.toGLSL(program, out);
                });
    } else if (skstd::ends_with(outputPath, ".metal")) {
        return compileProgram(
                [](SkSL::Compiler& compiler, SkSL::Program& program, SkSL::OutputStream& out) {
                    return compiler.toMetal(program, out);
                });
    } else if (skstd::ends_with(outputPath, ".hlsl")) {
        return compileProgram(
                [](SkSL::Compiler& compiler, SkSL::Program& program, SkSL::OutputStream& out) {
                    return compiler.toHLSL(program, out);
                });
    } else if (skstd::ends_with(outputPath, ".skvm")) {
        return compileProgramForSkVM(
                [&](SkSL::Compiler&, SkSL::Program& program, SkSL::OutputStream& out) {
                    skvm::Builder builder{skvm::Features{}};
                    if (!SkSL::testingOnly_ProgramToSkVMShader(program, &builder,
                                                               debugTrace.get())) {
                        return false;
                    }

                    std::unique_ptr<SkWStream> redirect = as_SkWStream(out);
                    if (debugTrace) {
                        debugTrace->dump(redirect.get());
                    }
                    builder.done().dump(redirect.get());
                    return true;
                });
    } else if (skstd::ends_with(outputPath, ".stage")) {
        return compileProgram(
                [](SkSL::Compiler&, SkSL::Program& program, SkSL::OutputStream& out) {
                    class Callbacks : public SkSL::PipelineStage::Callbacks {
                    public:
                        std::string getMangledName(const char* name) override {
                            return std::string(name) + "_0";
                        }

                        std::string declareUniform(const SkSL::VarDeclaration* decl) override {
                            fOutput += decl->description();
                            return std::string(decl->var().name());
                        }

                        void defineFunction(const char* decl,
                                            const char* body,
                                            bool /*isMain*/) override {
                            fOutput += std::string(decl) + "{" + body + "}";
                        }

                        void declareFunction(const char* decl) override {
                            fOutput += std::string(decl) + ";";
                        }

                        void defineStruct(const char* definition) override {
                            fOutput += definition;
                        }

                        void declareGlobal(const char* declaration) override {
                            fOutput += declaration;
                        }

                        std::string sampleShader(int index, std::string coords) override {
                            return "child_" + std::to_string(index) + ".eval(" + coords + ")";
                        }

                        std::string sampleColorFilter(int index, std::string color) override {
                            return "child_" + std::to_string(index) + ".eval(" + color + ")";
                        }

                        std::string sampleBlender(int index,
                                                  std::string src,
                                                  std::string dst) override {
                            return "child_" + std::to_string(index) + ".eval(" + src + ", " +
                                   dst + ")";
                        }

                        std::string toLinearSrgb(std::string color) override {
                            return "toLinearSrgb(" + color + ")";
                        }
                        std::string fromLinearSrgb(std::string color) override {
                            return "fromLinearSrgb(" + color + ")";
                        }

                        std::string fOutput;
                    };
                    // The .stage output looks almost like valid SkSL, but not quite.
                    // The PipelineStageGenerator bridges the gap between the SkSL in `program`,
                    // and the C++ FP builder API (see GrSkSLFP). In that API, children don't need
                    // to be declared (so they don't emit declarations here). Children are sampled
                    // by index, not name - so all children here are just "child_N".
                    // The input color and coords have names in the original SkSL (as parameters to
                    // main), but those are ignored here. References to those variables become
                    // "_coords" and "_inColor". At runtime, those variable names are irrelevant
                    // when the new SkSL is emitted inside the FP - references to those variables
                    // are replaced with strings from EmitArgs, and might be varyings or differently
                    // named parameters.
                    Callbacks callbacks;
                    SkSL::PipelineStage::ConvertProgram(program, "_coords", "_inColor",
                                                        "_canvasColor", &callbacks);
                    out.writeString(SkShaderUtils::PrettyPrint(callbacks.fOutput));
                    return true;
                });
    } else if (skstd::ends_with(outputPath, ".dehydrated.sksl")) {
        SkSL::FileOutputStream out(outputPath.c_str());
        SkSL::Compiler compiler(caps);
        if (!out.isValid()) {
            printf("error writing '%s'\n", outputPath.c_str());
            return ResultCode::kOutputError;
        }
        SkSL::LoadedModule module =
                compiler.loadModule(kind, SkSL::Compiler::MakeModulePath(inputPath.c_str()),
                                    /*base=*/nullptr, /*dehydrate=*/true);
        SkSL::Dehydrator dehydrator;
        dehydrator.write(*module.fSymbols);
        dehydrator.write(module.fElements);
        std::string baseName = base_name(inputPath, "", ".sksl");
        SkSL::StringStream buffer;
        dehydrator.finish(buffer);
        const std::string& data = buffer.str();
        out.printf("static uint8_t SKSL_INCLUDE_%s[] = {", baseName.c_str());
        for (size_t i = 0; i < data.length(); ++i) {
            out.printf("%s%d,", dehydrator.prefixAtOffset(i), uint8_t(data[i]));
        }
        out.printf("};\n");
        out.printf("static constexpr size_t SKSL_INCLUDE_%s_LENGTH = sizeof(SKSL_INCLUDE_%s);\n",
                   baseName.c_str(), baseName.c_str());
        if (!out.close()) {
            printf("error writing '%s'\n", outputPath.c_str());
            return ResultCode::kOutputError;
        }
    } else if (skstd::ends_with(outputPath, ".html")) {
        settings.fAllowTraceVarInSkVMDebugTrace = false;

        SkCpu::CacheRuntimeFeatures();
        gSkVMAllowJIT = true;
        return compileProgramForSkVM(
            [&](SkSL::Compiler&, SkSL::Program& program, SkSL::OutputStream& out) {
                if (!debugTrace) {
                    debugTrace = std::make_unique<SkSL::SkVMDebugTrace>();
                    debugTrace->setSource(text.c_str());
                }
                auto visualizer = std::make_unique<skvm::viz::Visualizer>(debugTrace.get());
                skvm::Builder builder(skvm::Features{}, /*createDuplicates=*/true);
                if (!SkSL::testingOnly_ProgramToSkVMShader(program, &builder, debugTrace.get())) {
                    return false;
                }

                std::unique_ptr<SkWStream> redirect = as_SkWStream(out);
                skvm::Program p = builder.done(
                        /*debug_name=*/nullptr, /*allow_jit=*/true, std::move(visualizer));
#if defined(SKVM_JIT)
                SkDynamicMemoryWStream asmFile;
                p.disassemble(&asmFile);
                auto dumpData = asmFile.detachAsData();
                std::string dumpString(static_cast<const char*>(dumpData->data()),dumpData->size());
                p.visualize(redirect.get(), dumpString.c_str());
#else
                p.visualize(redirect.get(), nullptr);
#endif
                return true;
            });
    } else {
        printf("expected output path to end with one of: .glsl, .html, .metal, .hlsl, .spirv, "
               ".asm.frag, .skvm, .stage, .asm.vert, .dehydrated.sksl (got '%s')\n",
               outputPath.c_str());
        return ResultCode::kConfigurationError;
    }
    return ResultCode::kSuccess;
}

/**
 * Processes multiple inputs in a single invocation of skslc.
 */
ResultCode processWorklist(const char* worklistPath) {
    std::string inputPath(worklistPath);
    if (!skstd::ends_with(inputPath, ".worklist")) {
        printf("expected .worklist file, found: %s\n\n", worklistPath);
        show_usage();
        return ResultCode::kConfigurationError;
    }

    // The worklist contains one line per argument to pass to skslc. When a blank line is reached,
    // those arguments will be passed to `processCommand`.
    auto resultCode = ResultCode::kSuccess;
    std::vector<std::string> args = {"skslc"};
    std::ifstream in(worklistPath);
    for (std::string line; std::getline(in, line); ) {
        if (in.rdstate()) {
            printf("error reading '%s'\n", worklistPath);
            return ResultCode::kInputError;
        }

        if (!line.empty()) {
            // We found an argument. Remember it.
            args.push_back(std::move(line));
        } else {
            // We found a blank line. If we have any arguments stored up, process them as a command.
            if (!args.empty()) {
                ResultCode outcome = processCommand(args);
                resultCode = std::max(resultCode, outcome);

                // Clear every argument except the first ("skslc").
                args.resize(1);
            }
        }
    }

    // If the worklist ended with a list of arguments but no blank line, process those now.
    if (args.size() > 1) {
        ResultCode outcome = processCommand(args);
        resultCode = std::max(resultCode, outcome);
    }

    // Return the "worst" status we encountered. For our purposes, compilation errors are the least
    // serious, because they are expected to occur in unit tests. Other types of errors are not
    // expected at all during a build.
    return resultCode;
}

int main(int argc, const char** argv) {
    if (argc == 2) {
        // Worklists are the only two-argument case for skslc, and we don't intend to support
        // nested worklists, so we can process them here.
        return (int)processWorklist(argv[1]);
    } else {
        // Process non-worklist inputs.
        std::vector<std::string> args;
        for (int index=0; index<argc; ++index) {
            args.push_back(argv[index]);
        }

        return (int)processCommand(args);
    }
}
