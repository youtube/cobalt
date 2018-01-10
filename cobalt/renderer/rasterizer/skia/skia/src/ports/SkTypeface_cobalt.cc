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

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkTypeface_cobalt.h"

#include "SkFontDescriptor.h"
#include "SkFontStyle.h"
#include "SkTypefaceCache.h"

SkTypeface_Cobalt::SkTypeface_Cobalt(int face_index, Style style,
                                     bool is_fixed_pitch,
                                     const SkString& family_name)
    : INHERITED(SkFontStyle::FromOldStyle(style), is_fixed_pitch),
      face_index_(face_index),
      family_name_(family_name),
      synthesizes_bold_(!isBold()) {}

void SkTypeface_Cobalt::onGetFamilyName(SkString* family_name) const {
  *family_name = family_name_;
}

SkTypeface_CobaltStream::SkTypeface_CobaltStream(SkStreamAsset* stream,
                                                 int face_index, Style style,
                                                 bool is_fixed_pitch,
                                                 const SkString& family_name)
    : INHERITED(face_index, style, is_fixed_pitch, family_name),
      stream_(stream) {
  LOG(INFO) << "Created SkTypeface_CobaltStream: " << family_name.c_str() << "("
            << style << "); Size: " << stream_->getLength() << " bytes";
}

void SkTypeface_CobaltStream::onGetFontDescriptor(SkFontDescriptor* descriptor,
                                                  bool* serialize) const {
  SkASSERT(descriptor);
  SkASSERT(serialize);
  descriptor->setFamilyName(family_name_.c_str());
  *serialize = true;
}

SkStreamAsset* SkTypeface_CobaltStream::onOpenStream(int* face_index) const {
  *face_index = face_index_;
  return stream_->duplicate();
}

size_t SkTypeface_CobaltStream::GetStreamLength() const {
  return stream_->getLength();
}

SkTypeface_CobaltStreamProvider::SkTypeface_CobaltStreamProvider(
    SkFileMemoryChunkStreamProvider* stream_provider, int face_index,
    Style style, bool is_fixed_pitch, const SkString& family_name,
    bool disable_synthetic_bolding)
    : INHERITED(face_index, style, is_fixed_pitch, family_name),
      stream_provider_(stream_provider) {
  if (disable_synthetic_bolding) {
    synthesizes_bold_ = false;
  }
  LOG(INFO) << "Created SkTypeface_CobaltStreamProvider: "
            << family_name.c_str() << "(" << style << "); File: \""
            << stream_provider->file_path() << "\"";
}

void SkTypeface_CobaltStreamProvider::onGetFontDescriptor(
    SkFontDescriptor* descriptor, bool* serialize) const {
  SkASSERT(descriptor);
  SkASSERT(serialize);
  descriptor->setFamilyName(family_name_.c_str());
  *serialize = false;
}

SkStreamAsset* SkTypeface_CobaltStreamProvider::onOpenStream(
    int* face_index) const {
  *face_index = face_index_;
  return stream_provider_->OpenStream();
}

size_t SkTypeface_CobaltStreamProvider::GetStreamLength() const {
  DLOG(WARNING)
      << "Requesting stream length of SkTypeface_CobaltStreamProvider. "
         "This requires a file load and should be used sparingly.";
  std::unique_ptr<SkFileMemoryChunkStream> stream(
      stream_provider_->OpenStream());
  return stream->getLength();
}
