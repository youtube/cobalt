// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_HAPTIC_ACTUATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_HAPTIC_ACTUATOR_H_

#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class GamepadDispatcher;
class GamepadEffectParameters;
enum class GamepadHapticActuatorType;
class ScriptState;
class ScriptPromise;
class ScriptPromiseResolver;

class GamepadHapticActuator final : public ScriptWrappable,
                                    public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  GamepadHapticActuator(ExecutionContext& context,
                        int pad_index,
                        device::GamepadHapticActuatorType type);
  ~GamepadHapticActuator() override;

  const String& type() const { return type_; }
  void SetType(device::GamepadHapticActuatorType);

  ScriptPromise playEffect(ScriptState*,
                           const String&,
                           const GamepadEffectParameters*);

  ScriptPromise reset(ScriptState*);

  bool canPlay(const String& type);

  void Trace(Visitor*) const override;

 private:
  void OnPlayEffectCompleted(ScriptPromiseResolver*,
                             device::mojom::GamepadHapticsResult);
  void OnResetCompleted(ScriptPromiseResolver*,
                        device::mojom::GamepadHapticsResult);
  void ResetVibrationIfNotPreempted();

  int pad_index_;
  String type_;
  bool should_reset_ = false;
  HashSet<String> supported_effect_types_;

  Member<GamepadDispatcher> gamepad_dispatcher_;
};

typedef HeapVector<Member<GamepadHapticActuator>> GamepadHapticActuatorVector;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_HAPTIC_ACTUATOR_H_
