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

# Translated from: gn/effects_imagefilters.gni

{
  'variables': {
    'skia_effects_imagefilter_public' : [
      "<(skia_include_path)/effects/SkAlphaThresholdFilter.h",
      "<(skia_include_path)/effects/SkArithmeticImageFilter.h",
      "<(skia_include_path)/effects/SkBlurImageFilter.h",
      "<(skia_include_path)/effects/SkColorFilterImageFilter.h",
      "<(skia_include_path)/effects/SkDisplacementMapEffect.h",
      "<(skia_include_path)/effects/SkDropShadowImageFilter.h",
      "<(skia_include_path)/effects/SkImageFilters.h",
      "<(skia_include_path)/effects/SkImageSource.h",
      "<(skia_include_path)/effects/SkLightingImageFilter.h",
      "<(skia_include_path)/effects/SkMagnifierImageFilter.h",
      "<(skia_include_path)/effects/SkMorphologyImageFilter.h",
      "<(skia_include_path)/effects/SkOffsetImageFilter.h",
      "<(skia_include_path)/effects/SkPaintImageFilter.h",
      "<(skia_include_path)/effects/SkTileImageFilter.h",
      "<(skia_include_path)/effects/SkXfermodeImageFilter.h",
    ],

    'skia_effects_imagefilter_sources' : [
      "<(skia_src_path)/effects/imagefilters/SkAlphaThresholdFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkArithmeticImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkColorFilterImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkComposeImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkDisplacementMapEffect.cpp",
      "<(skia_src_path)/effects/imagefilters/SkDropShadowImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkImageFilters.cpp",
      "<(skia_src_path)/effects/imagefilters/SkImageSource.cpp",
      "<(skia_src_path)/effects/imagefilters/SkLightingImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkMagnifierImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkMatrixConvolutionImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkMergeImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkMorphologyImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkOffsetImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkPaintImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkPictureImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkTileImageFilter.cpp",
      "<(skia_src_path)/effects/imagefilters/SkXfermodeImageFilter.cpp",
    ],

    'skia_effects_imagefilter_sources_no_asan' : [
      "<(skia_src_path)/effects/imagefilters/SkBlurImageFilter.cpp",
    ],
  },
}
