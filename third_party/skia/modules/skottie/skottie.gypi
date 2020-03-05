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

# Translated from: skottie.gni

{
  'variables': {
    'skottie_src_path': '<(skia_modules_path)/skottie/src',
    'skottie_include_path': '<(skia_modules_path)/skottie/include',

    'skia_skottie_public': [
      "<(skottie_include_path)/Skottie.h",
      "<(skottie_include_path)/SkottieProperty.h",
    ],

    'skia_skottie_sources' : [
      "<(skottie_src_path)/Composition.cpp",
      "<(skottie_src_path)/Layer.cpp",
      "<(skottie_src_path)/Skottie.cpp",
      "<(skottie_src_path)/SkottieAdapter.cpp",
      "<(skottie_src_path)/SkottieAdapter.h",
      "<(skottie_src_path)/SkottieAnimator.cpp",
      "<(skottie_src_path)/SkottieJson.cpp",
      "<(skottie_src_path)/SkottieJson.h",
      "<(skottie_src_path)/SkottiePriv.h",
      "<(skottie_src_path)/SkottieProperty.cpp",
      "<(skottie_src_path)/SkottieValue.cpp",
      "<(skottie_src_path)/SkottieValue.h",

      "<(skottie_src_path)/effects/DropShadowEffect.cpp",
      "<(skottie_src_path)/effects/Effects.cpp",
      "<(skottie_src_path)/effects/Effects.h",
      "<(skottie_src_path)/effects/FillEffect.cpp",
      "<(skottie_src_path)/effects/GaussianBlurEffect.cpp",
      "<(skottie_src_path)/effects/GradientEffect.cpp",
      "<(skottie_src_path)/effects/HueSaturationEffect.cpp",
      "<(skottie_src_path)/effects/LevelsEffect.cpp",
      "<(skottie_src_path)/effects/LinearWipeEffect.cpp",
      "<(skottie_src_path)/effects/MotionBlurEffect.cpp",
      "<(skottie_src_path)/effects/MotionBlurEffect.h",
      "<(skottie_src_path)/effects/MotionTileEffect.cpp",
      "<(skottie_src_path)/effects/RadialWipeEffect.cpp",
      "<(skottie_src_path)/effects/TintEffect.cpp",
      "<(skottie_src_path)/effects/TransformEffect.cpp",
      "<(skottie_src_path)/effects/TritoneEffect.cpp",
      "<(skottie_src_path)/effects/VenetianBlindsEffect.cpp",

      "<(skottie_src_path)/layers/ImageLayer.cpp",
      "<(skottie_src_path)/layers/NullLayer.cpp",
      "<(skottie_src_path)/layers/PrecompLayer.cpp",
      "<(skottie_src_path)/layers/ShapeLayer.cpp",
      "<(skottie_src_path)/layers/SolidLayer.cpp",
      "<(skottie_src_path)/layers/TextLayer.cpp",

      "<(skottie_src_path)/text/RangeSelector.cpp",
      "<(skottie_src_path)/text/RangeSelector.h",
      "<(skottie_src_path)/text/SkottieShaper.cpp",
      "<(skottie_src_path)/text/SkottieShaper.h",
      "<(skottie_src_path)/text/TextAdapter.cpp",
      "<(skottie_src_path)/text/TextAdapter.h",
      "<(skottie_src_path)/text/TextAnimator.cpp",
      "<(skottie_src_path)/text/TextAnimator.h",
      "<(skottie_src_path)/text/TextValue.cpp",
      "<(skottie_src_path)/text/TextValue.h",
    ],
  }
}
