# Copyright 2020 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Translated from: gn/utils.gni

{
  'variables': {
    'skia_utils_public': [
      "<(skia_include_path)/utils/SkAnimCodecPlayer.h",
      "<(skia_include_path)/utils/SkBase64.h",
      "<(skia_include_path)/utils/SkCamera.h",
      "<(skia_include_path)/utils/SkCanvasStateUtils.h",
      "<(skia_include_path)/utils/SkEventTracer.h",
      "<(skia_include_path)/utils/SkFrontBufferedStream.h",
      "<(skia_include_path)/utils/SkInterpolator.h",
      "<(skia_include_path)/utils/SkNWayCanvas.h",
      "<(skia_include_path)/utils/SkNoDrawCanvas.h",
      "<(skia_include_path)/utils/SkNullCanvas.h",
      "<(skia_include_path)/utils/SkPaintFilterCanvas.h",
      "<(skia_include_path)/utils/SkParse.h",
      "<(skia_include_path)/utils/SkParsePath.h",
      "<(skia_include_path)/utils/SkRandom.h",
      "<(skia_include_path)/utils/SkShadowUtils.h",

      #mac
      "<(skia_include_path)/utils/mac/SkCGUtils.h",
    ],

    'skia_utils_sources': [
      "<(skia_src_path)/utils/Sk3D.cpp",
      "<(skia_src_path)/utils/SkAnimCodecPlayer.cpp",
      "<(skia_src_path)/utils/SkBase64.cpp",
      "<(skia_src_path)/utils/SkBitSet.h",
      "<(skia_src_path)/utils/SkCallableTraits.h",
      "<(skia_src_path)/utils/SkCamera.cpp",
      "<(skia_src_path)/utils/SkCanvasStack.h",
      "<(skia_src_path)/utils/SkCanvasStack.cpp",
      "<(skia_src_path)/utils/SkCanvasStateUtils.cpp",
      "<(skia_src_path)/utils/SkCharToGlyphCache.cpp",
      "<(skia_src_path)/utils/SkCharToGlyphCache.h",
      "<(skia_src_path)/utils/SkDashPath.cpp",
      "<(skia_src_path)/utils/SkDashPathPriv.h",
      "<(skia_src_path)/utils/SkEventTracer.cpp",
      "<(skia_src_path)/utils/SkFloatToDecimal.cpp",
      "<(skia_src_path)/utils/SkFloatToDecimal.h",
      "<(skia_src_path)/utils/SkFloatUtils.h",
      "<(skia_src_path)/utils/SkFrontBufferedStream.cpp",
      "<(skia_src_path)/utils/SkInterpolator.cpp",
      "<(skia_src_path)/utils/SkJSON.cpp",
      "<(skia_src_path)/utils/SkJSON.h",
      "<(skia_src_path)/utils/SkJSONWriter.cpp",
      "<(skia_src_path)/utils/SkJSONWriter.h",
      "<(skia_src_path)/utils/SkMatrix22.cpp",
      "<(skia_src_path)/utils/SkMatrix22.h",
      "<(skia_src_path)/utils/SkMultiPictureDocument.cpp",
      "<(skia_src_path)/utils/SkNWayCanvas.cpp",
      "<(skia_src_path)/utils/SkNullCanvas.cpp",
      "<(skia_src_path)/utils/SkOSPath.cpp",
      "<(skia_src_path)/utils/SkOSPath.h",
      "<(skia_src_path)/utils/SkPaintFilterCanvas.cpp",
      "<(skia_src_path)/utils/SkParse.cpp",
      "<(skia_src_path)/utils/SkParseColor.cpp",
      "<(skia_src_path)/utils/SkParsePath.cpp",
      "<(skia_src_path)/utils/SkPatchUtils.cpp",
      "<(skia_src_path)/utils/SkPatchUtils.h",
      "<(skia_src_path)/utils/SkPolyUtils.cpp",
      "<(skia_src_path)/utils/SkPolyUtils.h",
      "<(skia_src_path)/utils/SkShadowTessellator.cpp",
      "<(skia_src_path)/utils/SkShadowTessellator.h",
      "<(skia_src_path)/utils/SkShadowUtils.cpp",
      "<(skia_src_path)/utils/SkShaperJSONWriter.h",
      "<(skia_src_path)/utils/SkShaperJSONWriter.cpp",
      "<(skia_src_path)/utils/SkTextUtils.cpp",
      "<(skia_src_path)/utils/SkThreadUtils_pthread.cpp",
      "<(skia_src_path)/utils/SkThreadUtils_win.cpp",
      "<(skia_src_path)/utils/SkUTF.cpp",
      "<(skia_src_path)/utils/SkUTF.h",
      "<(skia_src_path)/utils/SkWhitelistTypefaces.cpp",

      #mac
      "<(skia_src_path)/utils/mac/SkCreateCGImageRef.cpp",
      "<(skia_src_path)/utils/mac/SkUniqueCFRef.h",

      #windows
      "<(skia_src_path)/utils/win/SkAutoCoInitialize.h",
      "<(skia_src_path)/utils/win/SkAutoCoInitialize.cpp",
      "<(skia_src_path)/utils/win/SkDWrite.h",
      "<(skia_src_path)/utils/win/SkDWrite.cpp",
      "<(skia_src_path)/utils/win/SkDWriteFontFileStream.cpp",
      "<(skia_src_path)/utils/win/SkDWriteFontFileStream.h",
      "<(skia_src_path)/utils/win/SkDWriteGeometrySink.cpp",
      "<(skia_src_path)/utils/win/SkDWriteGeometrySink.h",
      "<(skia_src_path)/utils/win/SkDWriteNTDDI_VERSION.h",
      "<(skia_src_path)/utils/win/SkHRESULT.h",
      "<(skia_src_path)/utils/win/SkHRESULT.cpp",
      "<(skia_src_path)/utils/win/SkIStream.h",
      "<(skia_src_path)/utils/win/SkIStream.cpp",
      "<(skia_src_path)/utils/win/SkObjBase.h",
      "<(skia_src_path)/utils/win/SkTScopedComPtr.h",
      "<(skia_src_path)/utils/win/SkWGL.h",
      "<(skia_src_path)/utils/win/SkWGL_win.cpp",
    ],
  },
}
