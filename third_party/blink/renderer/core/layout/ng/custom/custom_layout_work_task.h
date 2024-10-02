// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_CUSTOM_LAYOUT_WORK_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_CUSTOM_LAYOUT_WORK_TASK_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_custom_layout_constraints_options.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ComputedStyle;
class CustomLayoutChild;
class CustomLayoutToken;
class LayoutUnit;
class NGConstraintSpace;
class NGLayoutInputNode;
class SerializedScriptValue;
class ScriptPromiseResolver;

// Contains all the information needed to resolve a promise with a fragment or
// intrinsic-sizes.
class CustomLayoutWorkTask final
    : public GarbageCollected<CustomLayoutWorkTask> {
 public:
  enum TaskType {
    kLayoutFragment,
    kIntrinsicSizes,
  };

  // Used when resolving a promise with intrinsic-sizes.
  CustomLayoutWorkTask(CustomLayoutChild*,
                       CustomLayoutToken*,
                       ScriptPromiseResolver*,
                       const TaskType type);

  // Used when resolving a promise with a fragment.
  CustomLayoutWorkTask(CustomLayoutChild*,
                       CustomLayoutToken*,
                       ScriptPromiseResolver*,
                       const CustomLayoutConstraintsOptions*,
                       scoped_refptr<SerializedScriptValue> constraint_data,
                       const TaskType type);
  ~CustomLayoutWorkTask();
  void Trace(Visitor*) const;

  // Runs this work task.
  void Run(const NGConstraintSpace& parent_space,
           const ComputedStyle& parent_style,
           const LayoutUnit child_available_block_size,
           bool* child_depends_on_block_constraints = nullptr);

 private:
  Member<CustomLayoutChild> child_;
  Member<CustomLayoutToken> token_;
  Member<ScriptPromiseResolver> resolver_;
  Member<const CustomLayoutConstraintsOptions> options_;
  scoped_refptr<SerializedScriptValue> constraint_data_;
  TaskType type_;

  void RunLayoutFragmentTask(const NGConstraintSpace& parent_space,
                             const ComputedStyle& parent_style,
                             NGLayoutInputNode child);
  void RunIntrinsicSizesTask(const NGConstraintSpace& parent_space,
                             const ComputedStyle& parent_style,
                             const LayoutUnit child_available_block_size,
                             NGLayoutInputNode child,
                             bool* child_depends_on_block_constraints);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_CUSTOM_LAYOUT_WORK_TASK_H_
