/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/audio_param.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <xmmintrin.h>
#elif defined(CPU_ARM_NEON)
#include <arm_neon.h>
#endif

namespace blink {

AudioParam::AudioParam(BaseAudioContext& context,
                       const String& parent_uuid,
                       AudioParamHandler::AudioParamType param_type,
                       double default_value,
                       AudioParamHandler::AutomationRate rate,
                       AudioParamHandler::AutomationRateMode rate_mode,
                       float min_value,
                       float max_value)
    : InspectorHelperMixin(context.GraphTracer(), parent_uuid),
      handler_(AudioParamHandler::Create(context,
                                         param_type,
                                         default_value,
                                         rate,
                                         rate_mode,
                                         min_value,
                                         max_value)),
      context_(context),
      deferred_task_handler_(&context.GetDeferredTaskHandler()) {}

AudioParam* AudioParam::Create(BaseAudioContext& context,
                               const String& parent_uuid,
                               AudioParamHandler::AudioParamType param_type,
                               double default_value,
                               AudioParamHandler::AutomationRate rate,
                               AudioParamHandler::AutomationRateMode rate_mode,
                               float min_value,
                               float max_value) {
  DCHECK_LE(min_value, max_value);

  return MakeGarbageCollected<AudioParam>(context, parent_uuid, param_type,
                                          default_value, rate, rate_mode,
                                          min_value, max_value);
}

AudioParam::~AudioParam() {
  // The graph lock is required to destroy the handler. And we can't use
  // `context_` to touch it, since that object may also be a dead heap object.
  {
    DeferredTaskHandler::GraphAutoLocker locker(*deferred_task_handler_);
    handler_ = nullptr;
  }
}

void AudioParam::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  InspectorHelperMixin::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

float AudioParam::value() const {
  return Handler().Value();
}

void AudioParam::WarnIfOutsideRange(const String& param_method, float value) {
  if (Context()->GetExecutionContext() &&
      (value < minValue() || value > maxValue())) {
    Context()->GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning,
            Handler().GetParamName() + "." + param_method + " " +
                String::Number(value) + " outside nominal range [" +
                String::Number(minValue()) + ", " + String::Number(maxValue()) +
                "]; value will be clamped."));
  }
}

void AudioParam::setValue(float value) {
  WarnIfOutsideRange("value", value);
  Handler().SetValue(value);
}

void AudioParam::setValue(float value, ExceptionState& exception_state) {
  WarnIfOutsideRange("value", value);

  // This is to signal any errors, if necessary, about conflicting
  // automations.
  setValueAtTime(value, Context()->currentTime(), exception_state);
  // This is to change the value so that an immediate query for the
  // value returns the expected values.
  Handler().SetValue(value);
}

float AudioParam::defaultValue() const {
  return Handler().DefaultValue();
}

float AudioParam::minValue() const {
  return Handler().MinValue();
}

float AudioParam::maxValue() const {
  return Handler().MaxValue();
}

void AudioParam::SetParamType(AudioParamHandler::AudioParamType param_type) {
  Handler().SetParamType(param_type);
}

void AudioParam::SetCustomParamName(const String name) {
  Handler().SetCustomParamName(name);
}

String AudioParam::automationRate() const {
  switch (Handler().GetAutomationRate()) {
    case AudioParamHandler::AutomationRate::kAudio:
      return "a-rate";
    case AudioParamHandler::AutomationRate::kControl:
      return "k-rate";
    default:
      NOTREACHED();
      return "a-rate";
  }
}

void AudioParam::setAutomationRate(const String& rate,
                                   ExceptionState& exception_state) {
  if (Handler().IsAutomationRateFixed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        Handler().GetParamName() +
            ".automationRate is fixed and cannot be changed to \"" + rate +
            "\"");
    return;
  }

  if (rate == "a-rate") {
    Handler().SetAutomationRate(AudioParamHandler::AutomationRate::kAudio);
  } else if (rate == "k-rate") {
    Handler().SetAutomationRate(AudioParamHandler::AutomationRate::kControl);
  }
}

AudioParam* AudioParam::setValueAtTime(float value,
                                       double time,
                                       ExceptionState& exception_state) {
  WarnIfOutsideRange("setValueAtTime value", value);
  Handler().Timeline().SetValueAtTime(value, time, exception_state);
  return this;
}

AudioParam* AudioParam::linearRampToValueAtTime(
    float value,
    double time,
    ExceptionState& exception_state) {
  WarnIfOutsideRange("linearRampToValueAtTime value", value);
  Handler().Timeline().LinearRampToValueAtTime(
      value, time, Handler().IntrinsicValue(), Context()->currentTime(),
      exception_state);

  return this;
}

AudioParam* AudioParam::exponentialRampToValueAtTime(
    float value,
    double time,
    ExceptionState& exception_state) {
  WarnIfOutsideRange("exponentialRampToValue value", value);
  Handler().Timeline().ExponentialRampToValueAtTime(
      value, time, Handler().IntrinsicValue(), Context()->currentTime(),
      exception_state);

  return this;
}

AudioParam* AudioParam::setTargetAtTime(float target,
                                        double time,
                                        double time_constant,
                                        ExceptionState& exception_state) {
  WarnIfOutsideRange("setTargetAtTime value", target);
  Handler().Timeline().SetTargetAtTime(target, time, time_constant,
                                       exception_state);
  return this;
}

AudioParam* AudioParam::setValueCurveAtTime(const Vector<float>& curve,
                                            double time,
                                            double duration,
                                            ExceptionState& exception_state) {
  float min = minValue();
  float max = maxValue();

  // Find the first value in the curve (if any) that is outside the
  // nominal range.  It's probably not necessary to produce a warning
  // on every value outside the nominal range.
  for (float value : curve) {
    if (value < min || value > max) {
      WarnIfOutsideRange("setValueCurveAtTime value", value);
      break;
    }
  }

  Handler().Timeline().SetValueCurveAtTime(curve, time, duration,
                                           exception_state);
  return this;
}

AudioParam* AudioParam::cancelScheduledValues(double start_time,
                                              ExceptionState& exception_state) {
  Handler().Timeline().CancelScheduledValues(start_time, exception_state);
  return this;
}

AudioParam* AudioParam::cancelAndHoldAtTime(double start_time,
                                            ExceptionState& exception_state) {
  Handler().Timeline().CancelAndHoldAtTime(start_time, exception_state);
  return this;
}

}  // namespace blink
