/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FRAME_CARET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FRAME_CARET_H_

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class CaretDisplayItemClient;
class EffectPaintPropertyNode;
class FrameCaret;
class GraphicsContext;
class LayoutBlock;
class LocalFrame;
class NGPhysicalBoxFragment;
struct PaintInvalidatorContext;
struct PhysicalOffset;
class SelectionEditor;

class CORE_EXPORT FrameCaret final : public GarbageCollected<FrameCaret> {
 public:
  FrameCaret(LocalFrame&, const SelectionEditor&);
  FrameCaret(const FrameCaret&) = delete;
  FrameCaret& operator=(const FrameCaret&) = delete;
  ~FrameCaret();

  bool IsActive() const;

  void ScheduleVisualUpdateForPaintInvalidationIfNeeded();

  // Used to suspend caret blinking while the mouse is down.
  void SetCaretBlinkingSuspended(bool suspended) {
    is_caret_blinking_suspended_ = suspended;
  }
  bool IsCaretBlinkingSuspended() const { return is_caret_blinking_suspended_; }
  void StopCaretBlinkTimer();
  void StartBlinkCaret();
  void SetCaretEnabled(bool);
  gfx::Rect AbsoluteCaretBounds() const;

  // Paint invalidation methods delegating to DisplayItemClient.
  void LayoutBlockWillBeDestroyed(const LayoutBlock&);
  void UpdateStyleAndLayoutIfNeeded();
  void InvalidatePaint(const LayoutBlock&, const PaintInvalidatorContext&);

  bool ShouldPaintCaret(const LayoutBlock&) const;
  bool ShouldPaintCaret(const NGPhysicalBoxFragment&) const;
  void PaintCaret(GraphicsContext&, const PhysicalOffset&) const;

  const EffectPaintPropertyNode& CaretEffectNode() const { return *effect_; }

  // For unit tests.
  void RecreateCaretBlinkTimerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner>,
      const base::TickClock* tick_clock);

  void Trace(Visitor*) const;

 private:
  friend class FrameCaretTest;
  friend class FrameSelectionTest;
  friend class CaretDisplayItemClientTest;

  EffectPaintPropertyNode::State CaretEffectNodeState(
      bool visible,
      const TransformPaintPropertyNodeOrAlias& local_transform_space) const;

  const PositionWithAffinity CaretPosition() const;

  bool ShouldShowCaret() const;
  void CaretBlinkTimerFired(TimerBase*);
  void UpdateAppearance();
  void SetVisibleIfActive(bool visible);
  bool IsVisibleIfActive() const { return effect_->Opacity() == 1.f; }

  const Member<const SelectionEditor> selection_editor_;
  const Member<LocalFrame> frame_;
  const Member<CaretDisplayItemClient> display_item_client_;
  // TODO(https://crbug.com/1123630): Consider moving the timer into the
  // compositor thread.
  HeapTaskRunnerTimer<FrameCaret> caret_blink_timer_;
  bool is_caret_enabled_ = false;
  bool should_show_caret_ = false;
  bool is_caret_blinking_suspended_ = false;
  // Controls visibility of caret with opacity when the caret is blinking.
  scoped_refptr<EffectPaintPropertyNode> effect_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FRAME_CARET_H_
