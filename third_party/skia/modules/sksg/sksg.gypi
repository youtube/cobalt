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

# Translated from: sksg.gni

{
  'variables': {
    'sksg_src_path': '<(skia_modules_path)/sksg/src',

    'skia_sksg_sources' : [
      "<(sksg_src_path)/SkSGClipEffect.cpp",
      "<(sksg_src_path)/SkSGColorFilter.cpp",
      "<(sksg_src_path)/SkSGDraw.cpp",
      "<(sksg_src_path)/SkSGEffectNode.cpp",
      "<(sksg_src_path)/SkSGGeometryNode.cpp",
      "<(sksg_src_path)/SkSGGeometryTransform.cpp",
      "<(sksg_src_path)/SkSGGradient.cpp",
      "<(sksg_src_path)/SkSGGroup.cpp",
      "<(sksg_src_path)/SkSGImage.cpp",
      "<(sksg_src_path)/SkSGInvalidationController.cpp",
      "<(sksg_src_path)/SkSGMaskEffect.cpp",
      "<(sksg_src_path)/SkSGMerge.cpp",
      "<(sksg_src_path)/SkSGNode.cpp",
      "<(sksg_src_path)/SkSGNodePriv.h",
      "<(sksg_src_path)/SkSGOpacityEffect.cpp",
      "<(sksg_src_path)/SkSGPaint.cpp",
      "<(sksg_src_path)/SkSGPath.cpp",
      "<(sksg_src_path)/SkSGPlane.cpp",
      "<(sksg_src_path)/SkSGRect.cpp",
      "<(sksg_src_path)/SkSGRenderEffect.cpp",
      "<(sksg_src_path)/SkSGRenderNode.cpp",
      "<(sksg_src_path)/SkSGRoundEffect.cpp",
      "<(sksg_src_path)/SkSGScene.cpp",
      "<(sksg_src_path)/SkSGText.cpp",
      "<(sksg_src_path)/SkSGTransform.cpp",
      "<(sksg_src_path)/SkSGTransformPriv.h",
      "<(sksg_src_path)/SkSGTrimEffect.cpp",
    ],
  }
}
