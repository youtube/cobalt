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
    '<(DEPTH)/third_party/freetype2/freetype2_cobalt.gyp:freetype2',
    '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
    '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
    'skia_library_opts.gyp:skia_opts',
  ],

  'includes': [
    '../../../../../third_party/skia/gyp/core.gypi',
    '../../../../../third_party/skia/gyp/effects.gypi',
    '../../../../../third_party/skia/gyp/utils.gypi',
  ],

  'sources': [
    '<(DEPTH)/third_party/skia/src/codec/SkBmpBaseCodec.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkBmpCodec.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkBmpMaskCodec.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkBmpRLECodec.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkBmpStandardCodec.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkCodec.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkCodecImageGenerator.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkGifCodec.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkMaskSwizzler.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkMasks.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkSampledCodec.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkSampler.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkStreamBuffer.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkSwizzler.cpp',
    '<(DEPTH)/third_party/skia/src/codec/SkWbmpCodec.cpp',
    '<(DEPTH)/third_party/skia/src/images/SkImageEncoder.cpp',
    '<(DEPTH)/third_party/skia/src/ports/SkDiscardableMemory_none.cpp',
    '<(DEPTH)/third_party/skia/src/ports/SkImageGenerator_skia.cpp',
    '<(DEPTH)/third_party/skia/src/sfnt/SkOTTable_name.cpp',
    '<(DEPTH)/third_party/skia/src/sfnt/SkOTUtils.cpp',
    '<(DEPTH)/third_party/skia/third_party/gif/SkGifImageReader.cpp',

    '<(DEPTH)/third_party/skia/src/ports/SkFontHost_FreeType.cpp',
    '<(DEPTH)/third_party/skia/src/ports/SkFontHost_FreeType_common.cpp',
    '<(DEPTH)/third_party/skia/src/ports/SkFontHost_FreeType_common.h',
    '<(DEPTH)/third_party/skia/src/ports/SkGlobalInitialization_none.cpp',
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
    '<(DEPTH)/third_party/skia/src/utils/win/SkAutoCoInitialize.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkAutoCoInitialize.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWrite.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWrite.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWriteFontFileStream.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWriteFontFileStream.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWriteGeometrySink.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkDWriteGeometrySink.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkHRESULT.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkHRESULT.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkIStream.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkIStream.cpp',
    '<(DEPTH)/third_party/skia/src/utils/win/SkTScopedComPtr.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkWGL.h',
    '<(DEPTH)/third_party/skia/src/utils/win/SkWGL_win.cpp',

    # testing
    '<(DEPTH)/third_party/skia/src/fonts/SkRandomScalerContext.cpp',
    '<(DEPTH)/third_party/skia/src/fonts/SkRandomScalerContext.h',
    '<(DEPTH)/third_party/skia/src/fonts/SkTestScalerContext.cpp',
    '<(DEPTH)/third_party/skia/src/fonts/SkTestScalerContext.h',
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
      '<(DEPTH)/third_party/skia/include/private',
      '<(DEPTH)/third_party/skia/include/utils',
    ],
  },

  'conditions': [
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
        '<@(skia_gpu_sources)',
        '<@(skia_native_gpu_sources)',
      ],
    }],
  ],
}
