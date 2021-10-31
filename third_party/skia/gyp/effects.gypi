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

# Translated from: gn/effects.gni

{
  'variables': {
    'skia_effects_public': [
      "<(skia_include_path)/effects/Sk1DPathEffect.h",
      "<(skia_include_path)/effects/Sk2DPathEffect.h",
      "<(skia_include_path)/effects/SkBlurDrawLooper.h",
      "<(skia_include_path)/effects/SkBlurMaskFilter.h",
      "<(skia_include_path)/effects/SkColorMatrix.h",
      "<(skia_include_path)/effects/SkColorMatrixFilter.h",
      "<(skia_include_path)/effects/SkCornerPathEffect.h",
      "<(skia_include_path)/effects/SkDashPathEffect.h",
      "<(skia_include_path)/effects/SkDiscretePathEffect.h",
      "<(skia_include_path)/effects/SkGradientShader.h",
      "<(skia_include_path)/effects/SkLayerDrawLooper.h",
      "<(skia_include_path)/effects/SkLumaColorFilter.h",
      "<(skia_include_path)/effects/SkOverdrawColorFilter.h",
      "<(skia_include_path)/effects/SkPerlinNoiseShader.h",
      "<(skia_include_path)/effects/SkShaderMaskFilter.h",
      "<(skia_include_path)/effects/SkTableColorFilter.h",
      "<(skia_include_path)/effects/SkTableMaskFilter.h",
    ],

    'skia_effects_sources': [
      "<(skia_src_path)/c/sk_effects.cpp",

      "<(skia_src_path)/effects/Sk1DPathEffect.cpp",
      "<(skia_src_path)/effects/Sk2DPathEffect.cpp",
      "<(skia_src_path)/effects/SkColorMatrix.cpp",
      "<(skia_src_path)/effects/SkColorMatrixFilter.cpp",
      "<(skia_src_path)/effects/SkCornerPathEffect.cpp",
      "<(skia_src_path)/effects/SkDashPathEffect.cpp",
      "<(skia_src_path)/effects/SkDiscretePathEffect.cpp",
      "<(skia_src_path)/effects/SkEmbossMask.cpp",
      "<(skia_src_path)/effects/SkEmbossMask.h",
      "<(skia_src_path)/effects/SkEmbossMaskFilter.cpp",
      "<(skia_src_path)/effects/SkHighContrastFilter.cpp",
      "<(skia_src_path)/effects/SkLayerDrawLooper.cpp",
      "<(skia_src_path)/effects/SkLumaColorFilter.cpp",
      "<(skia_src_path)/effects/SkOpPathEffect.cpp",
      "<(skia_src_path)/effects/SkOverdrawColorFilter.cpp",
      "<(skia_src_path)/effects/SkPackBits.cpp",
      "<(skia_src_path)/effects/SkPackBits.h",
      "<(skia_src_path)/effects/SkShaderMaskFilter.cpp",
      "<(skia_src_path)/effects/SkTableColorFilter.cpp",
      "<(skia_src_path)/effects/SkTableMaskFilter.cpp",
      "<(skia_src_path)/effects/SkTrimPathEffect.cpp",

      "<(skia_src_path)/shaders/SkPerlinNoiseShader.cpp",
      "<(skia_src_path)/shaders/gradients/Sk4fGradientBase.cpp",
      "<(skia_src_path)/shaders/gradients/Sk4fGradientBase.h",
      "<(skia_src_path)/shaders/gradients/Sk4fGradientPriv.h",
      "<(skia_src_path)/shaders/gradients/Sk4fLinearGradient.cpp",
      "<(skia_src_path)/shaders/gradients/Sk4fLinearGradient.h",
      "<(skia_src_path)/shaders/gradients/SkGradientShader.cpp",
      "<(skia_src_path)/shaders/gradients/SkGradientShaderPriv.h",
      "<(skia_src_path)/shaders/gradients/SkLinearGradient.cpp",
      "<(skia_src_path)/shaders/gradients/SkLinearGradient.h",
      "<(skia_src_path)/shaders/gradients/SkRadialGradient.cpp",
      "<(skia_src_path)/shaders/gradients/SkRadialGradient.h",
      "<(skia_src_path)/shaders/gradients/SkTwoPointConicalGradient.cpp",
      "<(skia_src_path)/shaders/gradients/SkTwoPointConicalGradient.h",
      "<(skia_src_path)/shaders/gradients/SkSweepGradient.cpp",
      "<(skia_src_path)/shaders/gradients/SkSweepGradient.h",
    ],
  },
}
