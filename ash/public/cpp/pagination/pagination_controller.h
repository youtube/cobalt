// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PAGINATION_PAGINATION_CONTROLLER_H_
#define ASH_PUBLIC_CPP_PAGINATION_PAGINATION_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"

namespace gfx {
class Vector2dF;
class Rect;
}  // namespace gfx

namespace ash {

class PaginationModel;
// Receives user scroll events from various sources (mouse wheel, touchpad,
// touch gestures) and manipulates a PaginationModel as necessary.
class ASH_PUBLIC_EXPORT PaginationController {
 public:
  enum ScrollAxis { SCROLL_AXIS_HORIZONTAL, SCROLL_AXIS_VERTICAL };

  using RecordMetrics = base::RepeatingCallback<void(ui::EventType)>;

  // Creates a PaginationController. Does not take ownership of |model|. The
  // |model| is required to outlive this PaginationController. |scroll_axis|
  // specifies the axis in which the pages will scroll.
  PaginationController(PaginationModel* model,
                       ScrollAxis scroll_axis,
                       const RecordMetrics& record_metrics);

  PaginationController(const PaginationController&) = delete;
  PaginationController& operator=(const PaginationController&) = delete;

  ~PaginationController();

  ScrollAxis scroll_axis() const { return scroll_axis_; }

  // Handles a mouse wheel or touchpad scroll event in the area represented by
  // the PaginationModel. |offset| is the number of units scrolled in each axis.
  // Returns true if the event was captured and there was some room to scroll.
  bool OnScroll(const gfx::Vector2dF& offset, ui::EventType type);

  // Handles a touch gesture event in the area represented by the
  // PaginationModel. Returns true if the event was captured.
  bool OnGestureEvent(const ui::GestureEvent& event, const gfx::Rect& bounds);

  // Handles a mouse event in the area represented by the PaginationModel.
  // |drag_offset| should be in screen coordinates.
  void StartMouseDrag(const gfx::Vector2dF& drag_offset);
  void UpdateMouseDrag(const gfx::Vector2dF& drag_offset,
                       const gfx::Rect& bounds);
  void EndMouseDrag(const ui::MouseEvent& event);

 private:
  // Drag related functions. Utilized by both gesture drag and mouse drag:
  bool StartDrag(float scroll);
  bool UpdateDrag(float scroll, const gfx::Rect& bounds);
  bool EndDrag(const ui::LocatedEvent& event);

  // Helper function to change the page and callback record_metrics_.
  void SelectPageAndRecordMetric(int delta, ui::EventType type);

  const raw_ptr<PaginationModel, ExperimentalAsh>
      pagination_model_;  // Not owned.
  const ScrollAxis scroll_axis_;

  const RecordMetrics record_metrics_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PAGINATION_PAGINATION_CONTROLLER_H_
