// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_COMMON_MEDIA_TYPE_CONVERTERS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_COMMON_MEDIA_TYPE_CONVERTERS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "third_party/chromium/media/mojo/mojom/content_decryption_module.mojom.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace media_m96 {
class AudioBuffer;
class DecoderBuffer;
class DecryptConfig;
}  // namespace media_m96

// These are specializations of mojo::TypeConverter and have to be in the mojo
// namespace.
namespace mojo {

template <>
struct TypeConverter<media_m96::mojom::DecryptConfigPtr, media_m96::DecryptConfig> {
  static media_m96::mojom::DecryptConfigPtr Convert(
      const media_m96::DecryptConfig& input);
};
template <>
struct TypeConverter<std::unique_ptr<media_m96::DecryptConfig>,
                     media_m96::mojom::DecryptConfigPtr> {
  static std::unique_ptr<media_m96::DecryptConfig> Convert(
      const media_m96::mojom::DecryptConfigPtr& input);
};

template <>
struct TypeConverter<media_m96::mojom::DecoderBufferPtr, media_m96::DecoderBuffer> {
  static media_m96::mojom::DecoderBufferPtr Convert(
      const media_m96::DecoderBuffer& input);
};
template <>
struct TypeConverter<scoped_refptr<media_m96::DecoderBuffer>,
                     media_m96::mojom::DecoderBufferPtr> {
  static scoped_refptr<media_m96::DecoderBuffer> Convert(
      const media_m96::mojom::DecoderBufferPtr& input);
};

template <>
struct TypeConverter<media_m96::mojom::AudioBufferPtr, media_m96::AudioBuffer> {
  static media_m96::mojom::AudioBufferPtr Convert(const media_m96::AudioBuffer& input);
};
template <>
struct TypeConverter<scoped_refptr<media_m96::AudioBuffer>,
                     media_m96::mojom::AudioBufferPtr> {
  static scoped_refptr<media_m96::AudioBuffer> Convert(
      const media_m96::mojom::AudioBufferPtr& input);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_COMMON_MEDIA_TYPE_CONVERTERS_H_
