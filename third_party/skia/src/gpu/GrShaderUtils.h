/*
 * Copyright 2019 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrShaderUtils_DEFINED
#define GrShaderUtils_DEFINED

#include "include/core/SkTypes.h"
#include "include/gpu/GrContextOptions.h"
#include "include/private/SkSLProgramKind.h"
#include "include/private/SkSLString.h"

namespace GrShaderUtils {

SkSL::String PrettyPrint(const SkSL::String& string);

void VisitLineByLine(const SkSL::String& text,
                     const std::function<void(int lineNumber, const char* lineText)>&);

// Prints shaders one line at the time. This ensures they don't get truncated by the adb log.
inline void PrintLineByLine(const SkSL::String& text) {
    VisitLineByLine(text, [](int lineNumber, const char* lineText) {
        SkDebugf("%4i\t%s\n", lineNumber, lineText);
    });
}

// Combines raw shader and error text into an easier-to-read error message with line numbers.
SkSL::String BuildShaderErrorMessage(const char* shader, const char* errors);

GrContextOptions::ShaderErrorHandler* DefaultShaderErrorHandler();

void PrintShaderBanner(SkSL::ProgramKind programKind);

}  // namespace GrShaderUtils

#endif
