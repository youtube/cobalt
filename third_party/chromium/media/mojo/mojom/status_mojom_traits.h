// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_STATUS_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_STATUS_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "base/values.h"
#include "third_party/chromium/media/base/ipc/media_param_traits.h"
#include "third_party/chromium/media/base/status.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

template <>
struct StructTraits<media_m96::mojom::StatusDataDataView,
                    media_m96::internal::StatusData> {
  static media_m96::StatusCodeType code(const media_m96::internal::StatusData& input) {
    return input.code;
  }

  static media_m96::StatusGroupType group(
      const media_m96::internal::StatusData& input) {
    return input.group;
  }

  static std::string message(const media_m96::internal::StatusData& input) {
    return input.message;
  }

  static base::span<base::Value> frames(media_m96::internal::StatusData& input) {
    return input.frames;
  }

  static base::span<media_m96::internal::StatusData> causes(
      media_m96::internal::StatusData& input) {
    return input.causes;
  }

  static base::Value data(const media_m96::internal::StatusData& input) {
    return input.data.Clone();
  }

  static bool Read(media_m96::mojom::StatusDataDataView data,
                   media_m96::internal::StatusData* output);
};

template <typename StatusEnum, typename DataView>
struct StructTraits<DataView, media_m96::TypedStatus<StatusEnum>> {
  static absl::optional<media_m96::internal::StatusData> internal(
      const media_m96::TypedStatus<StatusEnum>& input) {
    if (input.data_)
      return *input.data_;
    return absl::nullopt;
  }

  static bool Read(DataView data, media_m96::TypedStatus<StatusEnum>* output) {
    absl::optional<media_m96::internal::StatusData> internal;
    if (!data.ReadInternal(&internal))
      return false;
    if (internal)
      output->data_ = internal->copy();
    return true;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_STATUS_MOJOM_TRAITS_H_
