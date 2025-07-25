// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_REQUEST_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_REQUEST_MOJOM_TRAITS_H_

#include <memory>

#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/public/cpp/compositing/copy_output_result_mojom_traits.h"
#include "services/viz/public/mojom/compositing/copy_output_request.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::CopyOutputRequestDataView,
                    std::unique_ptr<viz::CopyOutputRequest>> {
  static viz::CopyOutputRequest::ResultFormat result_format(
      const std::unique_ptr<viz::CopyOutputRequest>& request) {
    return request->result_format();
  }

  static viz::CopyOutputRequest::ResultDestination result_destination(
      const std::unique_ptr<viz::CopyOutputRequest>& request) {
    return request->result_destination();
  }

  static const gfx::Vector2d& scale_from(
      const std::unique_ptr<viz::CopyOutputRequest>& request) {
    return request->scale_from();
  }

  static const gfx::Vector2d& scale_to(
      const std::unique_ptr<viz::CopyOutputRequest>& request) {
    return request->scale_to();
  }

  static const absl::optional<base::UnguessableToken>& source(
      const std::unique_ptr<viz::CopyOutputRequest>& request) {
    return request->source_;
  }

  static const absl::optional<gfx::Rect>& area(
      const std::unique_ptr<viz::CopyOutputRequest>& request) {
    return request->area_;
  }

  static const absl::optional<gfx::Rect>& result_selection(
      const std::unique_ptr<viz::CopyOutputRequest>& request) {
    return request->result_selection_;
  }

  static mojo::PendingRemote<viz::mojom::CopyOutputResultSender> result_sender(
      const std::unique_ptr<viz::CopyOutputRequest>& request);

  static bool Read(viz::mojom::CopyOutputRequestDataView data,
                   std::unique_ptr<viz::CopyOutputRequest>* out_p);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_COPY_OUTPUT_REQUEST_MOJOM_TRAITS_H_
