# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# This gypi file contains the Skia library.
#
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# variables and defines should go in skia_common.gypi so they can be seen
# by files listed here and in skia_library_opts.gypi.
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
{
  'dependencies': [
    '<(DEPTH)/cobalt/fonts/freetype2/freetype2.gyp:freetype2',
    '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
    '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
    'skia_library_opts.gyp:skia_opts',
  ],

  'includes': [
    '../../../../../third_party/skia/gyp/core.gypi',
    '../../../../../third_party/skia/gyp/effects.gypi',
    '../../../../../third_party/skia/gyp/record.gypi',
    '../../../../../third_party/skia/gyp/utils.gypi',
  ],

  'sources': [
    '<(DEPTH)/third_party/skia/src/core/SkFlate.cpp',
    '<(DEPTH)/third_party/skia/src/core/SkPath.cpp',
    '<(DEPTH)/third_party/skia/src/core/SkTypeface.cpp',

    '<(DEPTH)/third_party/skia/src/images/SkImageDecoder.cpp',
    '<(DEPTH)/third_party/skia/src/images/SkImageDecoder_FactoryDefault.cpp',
    '<(DEPTH)/third_party/skia/src/images/SkImageDecoder_FactoryRegistrar.cpp',

    '<(DEPTH)/third_party/skia/src/images/SkImageDecoder_libpng.cpp',
    '<(DEPTH)/third_party/skia/src/images/SkImageEncoder.cpp',
    '<(DEPTH)/third_party/skia/src/images/SkImageEncoder_Factory.cpp',

    '<(DEPTH)/third_party/skia/src/images/SkScaledBitmapSampler.cpp',
    '<(DEPTH)/third_party/skia/src/images/SkScaledBitmapSampler.h',

    '<(DEPTH)/third_party/skia/src/ports/SkDiscardableMemory_none.cpp',
    '<(DEPTH)/third_party/skia/src/ports/SkFontHost_FreeType.cpp',
    '<(DEPTH)/third_party/skia/src/ports/SkFontHost_FreeType_common.cpp',
    '<(DEPTH)/third_party/skia/src/ports/SkFontHost_FreeType_common.h',
    '<(DEPTH)/third_party/skia/src/ports/SkGlobalInitialization_chromium.cpp',
    '<(DEPTH)/third_party/skia/src/ports/SkMemory_malloc.cpp',

    '<(DEPTH)/third_party/skia/src/sfnt/SkOTTable_name.cpp',
    '<(DEPTH)/third_party/skia/src/sfnt/SkOTTable_name.h',
    '<(DEPTH)/third_party/skia/src/sfnt/SkOTUtils.cpp',
    '<(DEPTH)/third_party/skia/src/sfnt/SkOTUtils.h',

    '<(DEPTH)/third_party/skia/src/utils/debugger/SkDebugCanvas.cpp',
    '<(DEPTH)/third_party/skia/src/utils/debugger/SkDebugCanvas.h',
    '<(DEPTH)/third_party/skia/src/utils/debugger/SkDrawCommand.cpp',
    '<(DEPTH)/third_party/skia/src/utils/debugger/SkDrawCommand.h',
    '<(DEPTH)/third_party/skia/src/utils/debugger/SkObjectParser.cpp',
    '<(DEPTH)/third_party/skia/src/utils/debugger/SkObjectParser.h',

    '<(DEPTH)/third_party/skia/include/images/SkMovie.h',
    '<(DEPTH)/third_party/skia/include/images/SkPageFlipper.h',

    '<(DEPTH)/third_party/skia/include/ports/SkFontMgr.h',
    '<(DEPTH)/third_party/skia/include/ports/SkFontStyle.h',
  ],

  # Exclude all unused files in skia utils.gypi file
  'sources!': [
    '<(DEPTH)/third_party/skia/src/utils/SkCondVar.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkCondVar.h',
    '<(DEPTH)/third_party/skia/src/utils/SkRunnable.h',

    '<(DEPTH)/third_party/skia/include/utils/SkBoundaryPatch.h',
    '<(DEPTH)/third_party/skia/include/utils/SkCamera.h',
    '<(DEPTH)/third_party/skia/include/utils/SkCanvasStateUtils.h',
    '<(DEPTH)/third_party/skia/include/utils/SkCubicInterval.h',
    '<(DEPTH)/third_party/skia/include/utils/SkCullPoints.h',
    '<(DEPTH)/third_party/skia/include/utils/SkDebugUtils.h',
    '<(DEPTH)/third_party/skia/include/utils/SkDumpCanvas.h',
    '<(DEPTH)/third_party/skia/include/utils/SkEventTracer.h',
    '<(DEPTH)/third_party/skia/include/utils/SkFrontBufferedStream.h',
    '<(DEPTH)/third_party/skia/include/utils/SkInterpolator.h',
    '<(DEPTH)/third_party/skia/include/utils/SkLayer.h',
    '<(DEPTH)/third_party/skia/include/utils/SkMeshUtils.h',
    '<(DEPTH)/third_party/skia/include/utils/SkNinePatch.h',
    '<(DEPTH)/third_party/skia/include/utils/SkParse.h',
    '<(DEPTH)/third_party/skia/include/utils/SkParsePaint.h',
    '<(DEPTH)/third_party/skia/include/utils/SkParsePath.h',
    '<(DEPTH)/third_party/skia/include/utils/SkRandom.h',
    '<(DEPTH)/third_party/skia/include/utils/SkWGL.h',

    '<(DEPTH)/third_party/skia/src/utils/SkBitmapHasher.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkBitmapHasher.h',
    '<(DEPTH)/third_party/skia/src/utils/SkBoundaryPatch.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkCamera.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkCanvasStack.h',
    '<(DEPTH)/third_party/skia/src/utils/SkCubicInterval.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkCullPoints.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkDumpCanvas.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkFloatUtils.h',
    '<(DEPTH)/third_party/skia/src/utils/SkFrontBufferedStream.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkGatherPixelRefsAndRects.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkGatherPixelRefsAndRects.h',
    '<(DEPTH)/third_party/skia/src/utils/SkInterpolator.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkLayer.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkMD5.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkMD5.h',
    '<(DEPTH)/third_party/skia/src/utils/SkMeshUtils.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkNinePatch.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkParse.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkParseColor.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkParsePath.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkPathUtils.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkSHA1.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkSHA1.h',
    '<(DEPTH)/third_party/skia/src/utils/SkTFitsIn.h',
    '<(DEPTH)/third_party/skia/src/utils/SkTLogic.h',
    '<(DEPTH)/third_party/skia/src/utils/SkThreadUtils.h',
    '<(DEPTH)/third_party/skia/src/utils/SkThreadUtils_pthread.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkThreadUtils_pthread.h',
    '<(DEPTH)/third_party/skia/src/utils/SkThreadUtils_pthread_linux.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkThreadUtils_pthread_mach.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkThreadUtils_pthread_other.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkThreadUtils_win.cpp',
    '<(DEPTH)/third_party/skia/src/utils/SkThreadUtils_win.h',

    # mac
    '<(DEPTH)/third_party/skia/include/utils/mac/SkCGUtils.h',
    '<(DEPTH)/third_party/skia/src/utils/mac/SkCreateCGImageRef.cpp',

    # windows
    '<(DEPTH)/third_party/skia/include/utils/win/SkAutoCoInitialize.h',
    '<(DEPTH)/third_party/skia/include/utils/win/SkHRESULT.h',
    '<(DEPTH)/third_party/skia/include/utils/win/SkIStream.h',
    '<(DEPTH)/third_party/skia/include/utils/win/SkTScopedComPtr.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkAutoCoInitialize.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWrite.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWrite.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWriteFontFileStream.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWriteFontFileStream.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWriteGeometrySink.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWriteGeometrySink.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkHRESULT.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkIStream.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkWGL_win.cpp',

    # testing
    '<(DEPTH)/third_party/skia/src/fonts/SkGScalerContext.cpp',
    '<(DEPTH)/third_party/skia/src/fonts/SkGScalerContext.h',
    '<(DEPTH)/third_party/skia/src/fonts/SkTestScalerContext.cpp',
    '<(DEPTH)/third_party/skia/src/fonts/SkTestScalerContext.h',
  ],

  # Exclude Skia OpenGL backend source files.
  'sources/': [
    ['exclude', 'skia/src/gpu/GrGpuFactory.(h|cpp)$'],
  ],

  'direct_dependent_settings': {
    'include_dirs': [
      #temporary until we can hide SkFontHost
      '<(DEPTH)/third_party/skia/src/core',

      '<(DEPTH)/third_party/skia/include/core',
      '<(DEPTH)/third_party/skia/include/effects',
      '<(DEPTH)/third_party/skia/include/gpu',
      '<(DEPTH)/third_party/skia/include/lazy',
      '<(DEPTH)/third_party/skia/include/pathops',
      '<(DEPTH)/third_party/skia/include/pipe',
      '<(DEPTH)/third_party/skia/include/ports',
      '<(DEPTH)/third_party/skia/include/utils',
    ],
  },

  'conditions': [
    ['target_arch=="ps3"', {
      # Disable warnings within Skia's repository.
      'cflags': [
        '--diag_suppress=1067,1576,1229,1780',
      ],
    }],
    ['target_os=="linux"', {
      'cflags': [
        '-Wno-deprecated-declarations',
      ],
    }],
    ['OS=="starboard"', {
      'sources!': [
        '<(DEPTH)/third_party/skia/src/ports/SkMemory_malloc.cpp',
      ],
    }],
    ['gl_type != "none"', {
      'includes': [
        '../../../../../third_party/skia/gyp/gpu.gypi',
      ],
      'sources': [
        '<@(skgpu_sources)',
      ],
    }],
  ],
}
