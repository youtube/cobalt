// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_SHARED_STORAGE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_SHARED_STORAGE_MOJOM_TRAITS_H_

#include <string>

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom-shared.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT
    StructTraits<blink::mojom::SharedStorageKeyArgumentDataView, WTF::String> {
  static bool Read(blink::mojom::SharedStorageKeyArgumentDataView data,
                   WTF::String* out_key);

  static const WTF::String& data(const WTF::String& input) { return input; }
};

template <>
struct PLATFORM_EXPORT
    StructTraits<blink::mojom::SharedStorageValueArgumentDataView,
                 WTF::String> {
  static bool Read(blink::mojom::SharedStorageValueArgumentDataView data,
                   WTF::String* out_value);

  static const WTF::String& data(const WTF::String& input) { return input; }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_SHARED_STORAGE_MOJOM_TRAITS_H_
