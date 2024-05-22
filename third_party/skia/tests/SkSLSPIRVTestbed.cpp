/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/SkSLCompiler.h"

#include "tests/Test.h"

static void test(skiatest::Reporter* r,
                 const SkSL::ShaderCaps& caps,
                 const char* src,
                 SkSL::ProgramKind kind = SkSL::ProgramKind::kFragment) {
    SkSL::Compiler compiler(&caps);
    SkSL::Program::Settings settings;
    std::unique_ptr<SkSL::Program> program = compiler.convertProgram(kind, std::string(src),
                                                                     settings);
    if (!program) {
        SkDebugf("Unexpected error compiling %s\n%s", src, compiler.errorText().c_str());
        REPORTER_ASSERT(r, program);
    } else {
        std::string output;
        REPORTER_ASSERT(r, compiler.toSPIRV(*program, &output));
    }
}

DEF_TEST(SkSLSPIRVTestbed, r) {
    // Add in your SkSL here.
    test(r,
         *SkSL::ShaderCapsFactory::Default(),
         R"__SkSL__(
             void main() {
                 sk_FragColor = half4(0);
             }
         )__SkSL__");
}
