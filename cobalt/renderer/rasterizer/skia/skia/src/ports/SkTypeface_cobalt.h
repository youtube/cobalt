// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKTYPEFACE_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKTYPEFACE_COBALT_H_

#include "base/memory/ref_counted.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkStream_cobalt.h"
#include "SkStream.h"
#include "SkString.h"
#include "third_party/skia/src/ports/SkFontHost_FreeType_common.h"

class SkFontMgr_Cobalt;

class SkTypeface_Cobalt : public SkTypeface_FreeType {
 public:
  SkTypeface_Cobalt(int face_index, Style style, bool is_fixed_pitch,
                    const SkString& family_name);

  virtual size_t GetStreamLength() const = 0;

  bool synthesizes_bold() const { return synthesizes_bold_; }

 protected:
  void onGetFamilyName(SkString* family_name) const SK_OVERRIDE;

  int face_index_;
  SkString family_name_;
  bool synthesizes_bold_;

 private:
  typedef SkTypeface_FreeType INHERITED;
};

class SkTypeface_CobaltStream : public SkTypeface_Cobalt {
 public:
  SkTypeface_CobaltStream(SkStreamAsset* stream, int face_index, Style style,
                          bool is_fixed_pitch, const SkString& family_name);

  void onGetFontDescriptor(SkFontDescriptor* descriptor,
                           bool* serialize) const SK_OVERRIDE;

  SkStreamAsset* onOpenStream(int* face_index) const SK_OVERRIDE;

  size_t GetStreamLength() const SK_OVERRIDE;

 private:
  typedef SkTypeface_Cobalt INHERITED;

  scoped_ptr<SkStreamAsset> stream_;
};

class SkTypeface_CobaltStreamProvider : public SkTypeface_Cobalt {
 public:
  SkTypeface_CobaltStreamProvider(
      SkFileMemoryChunkStreamProvider* stream_provider, int face_index,
      Style style, bool is_fixed_pitch, const SkString& family_name,
      bool disable_synthetic_bolding);

  void onGetFontDescriptor(SkFontDescriptor* descriptor,
                           bool* serialize) const SK_OVERRIDE;

  SkStreamAsset* onOpenStream(int* face_index) const SK_OVERRIDE;

  size_t GetStreamLength() const SK_OVERRIDE;

 private:
  typedef SkTypeface_Cobalt INHERITED;

  SkFileMemoryChunkStreamProvider* const stream_provider_;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKTYPEFACE_COBALT_H_
