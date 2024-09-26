// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_VIEW_TRANSITION_STATE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_VIEW_TRANSITION_STATE_MOJOM_TRAITS_H_

#include "base/check_op.h"
#include "services/viz/public/mojom/compositing/view_transition_element_resource_id.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/frame/view_transition_state.h"
#include "third_party/blink/public/mojom/frame/view_transition_state.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ViewTransitionElementDataView,
                 blink::ViewTransitionElement> {
  static const std::string& tag_name(const blink::ViewTransitionElement& r) {
    return r.tag_name;
  }

  static const gfx::SizeF& border_box_size_in_css_space(
      const blink::ViewTransitionElement& r) {
    return r.border_box_size_in_css_space;
  }

  static const gfx::Transform& viewport_matrix(
      const blink::ViewTransitionElement& r) {
    return r.viewport_matrix;
  }

  static const gfx::RectF& overflow_rect_in_layout_space(
      const blink::ViewTransitionElement& r) {
    return r.overflow_rect_in_layout_space;
  }

  static const viz::ViewTransitionElementResourceId& snapshot_id(
      const blink::ViewTransitionElement& r) {
    return r.snapshot_id;
  }

  static int32_t paint_order(const blink::ViewTransitionElement& r) {
    return r.paint_order;
  }

  static bool is_root(const blink::ViewTransitionElement& r) {
    return r.is_root;
  }

  static bool Read(blink::mojom::ViewTransitionElementDataView r,
                   blink::ViewTransitionElement* out);
};

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::ViewTransitionStateDataView,
                 blink::ViewTransitionState> {
  static const std::vector<blink::ViewTransitionElement>& elements(
      const blink::ViewTransitionState& r) {
    return r.elements;
  }

  static const base::UnguessableToken& navigation_id(
      const blink::ViewTransitionState& r) {
    return r.navigation_id;
  }

  static const gfx::Size& snapshot_root_size_at_capture(
      const blink::ViewTransitionState& r) {
    return r.snapshot_root_size_at_capture;
  }

  static bool Read(blink::mojom::ViewTransitionStateDataView r,
                   blink::ViewTransitionState* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_VIEW_TRANSITION_STATE_MOJOM_TRAITS_H_
