# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# This gypi file contains all the Cobalt-specific enhancements to Skia.
# In component mode (shared_lib) it is folded into a single shared library with
# the Skia files but in all other cases it is a separate library.
{
  'sources': [
    '<(DEPTH)/third_party/skia/src/sksl/lex.layout.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/SkSLCFGGenerator.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/SkSLCompiler.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/SkSLCPPCodeGenerator.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/SkSLGLSLCodeGenerator.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/SkSLHCodeGenerator.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/SkSLIRGenerator.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/SkSLParser.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/SkSLSPIRVCodeGenerator.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/SkSLString.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/SkSLUtil.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/ir/SkSLSetting.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/ir/SkSLSymbolTable.cpp',
    '<(DEPTH)/third_party/skia/src/sksl/ir/SkSLType.cpp',
  ],
}
