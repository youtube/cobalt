// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor.h"

#include <utility>

#include "base/ranges/algorithm.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "services/device/public/mojom/sensor.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/modules/sensor/sensor_error_event.h"
#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

namespace {
const double kWaitingIntervalThreshold = 0.01;

bool AreFeaturesEnabled(
    ExecutionContext* context,
    const Vector<mojom::blink::PermissionsPolicyFeature>& features) {
  return base::ranges::all_of(
      features, [context](mojom::blink::PermissionsPolicyFeature feature) {
        return context->IsFeatureEnabled(feature,
                                         ReportOptions::kReportOnFailure);
      });
}

}  // namespace

Sensor::Sensor(ExecutionContext* execution_context,
               const SensorOptions* sensor_options,
               ExceptionState& exception_state,
               device::mojom::blink::SensorType type,
               const Vector<mojom::blink::PermissionsPolicyFeature>& features)
    : ActiveScriptWrappable<Sensor>({}),
      ExecutionContextLifecycleObserver(execution_context),
      frequency_(0.0),
      type_(type),
      state_(SensorState::kIdle),
      last_reported_timestamp_(0.0) {
  // [SecureContext] in idl.
  DCHECK(execution_context->IsSecureContext());
  DCHECK(!features.empty());

  if (!AreFeaturesEnabled(execution_context, features)) {
    exception_state.ThrowSecurityError(
        "Access to sensor features is disallowed by permissions policy");
    return;
  }

  // Check the given frequency value.
  if (sensor_options->hasFrequency()) {
    frequency_ = sensor_options->frequency();
    const double max_allowed_frequency =
        device::GetSensorMaxAllowedFrequency(type_);
    if (frequency_ > max_allowed_frequency) {
      frequency_ = max_allowed_frequency;
      String message = String::Format(
          "Maximum allowed frequency value for this sensor type is %.0f Hz.",
          max_allowed_frequency);
      auto* console_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kInfo, std::move(message));
      execution_context->AddConsoleMessage(console_message);
    }
  }
}

Sensor::Sensor(ExecutionContext* execution_context,
               const SpatialSensorOptions* options,
               ExceptionState& exception_state,
               device::mojom::blink::SensorType sensor_type,
               const Vector<mojom::blink::PermissionsPolicyFeature>& features)
    : Sensor(execution_context,
             static_cast<const SensorOptions*>(options),
             exception_state,
             sensor_type,
             features) {
  use_screen_coords_ = (options->referenceFrame() == "screen");
}

Sensor::~Sensor() = default;

void Sensor::start() {
  if (!GetExecutionContext())
    return;
  if (state_ != SensorState::kIdle)
    return;
  state_ = SensorState::kActivating;
  Activate();
}

void Sensor::stop() {
  if (state_ == SensorState::kIdle)
    return;
  Deactivate();
  state_ = SensorState::kIdle;
}

// Getters
bool Sensor::activated() const {
  return state_ == SensorState::kActivated;
}

bool Sensor::hasReading() const {
  if (!activated())
    return false;
  DCHECK(sensor_proxy_);
  return sensor_proxy_->GetReading().timestamp() != 0.0;
}

absl::optional<DOMHighResTimeStamp> Sensor::timestamp(
    ScriptState* script_state) const {
  if (!hasReading()) {
    return absl::nullopt;
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window) {
    return absl::nullopt;
  }

  WindowPerformance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);
  DCHECK(sensor_proxy_);

  return performance->MonotonicTimeToDOMHighResTimeStamp(
      base::TimeTicks() +
      base::Seconds(sensor_proxy_->GetReading().timestamp()));
}

void Sensor::Trace(Visitor* visitor) const {
  visitor->Trace(sensor_proxy_);
  ActiveScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTarget::Trace(visitor);
}

bool Sensor::HasPendingActivity() const {
  if (state_ == SensorState::kIdle)
    return false;
  return GetExecutionContext() && HasEventListeners();
}

auto Sensor::CreateSensorConfig() -> SensorConfigurationPtr {
  auto result = SensorConfiguration::New();

  double default_frequency = sensor_proxy_->GetDefaultFrequency();
  double minimum_frequency = sensor_proxy_->GetFrequencyLimits().first;
  double maximum_frequency = sensor_proxy_->GetFrequencyLimits().second;

  if (frequency_ == 0.0)  // i.e. was never set.
    frequency_ = default_frequency;
  if (frequency_ > maximum_frequency)
    frequency_ = maximum_frequency;
  if (frequency_ < minimum_frequency)
    frequency_ = minimum_frequency;

  result->frequency = frequency_;
  return result;
}

void Sensor::InitSensorProxyIfNeeded() {
  if (sensor_proxy_)
    return;

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  auto* provider = SensorProviderProxy::From(window);
  sensor_proxy_ = provider->GetSensorProxy(type_);

  if (!sensor_proxy_) {
    sensor_proxy_ =
        provider->CreateSensorProxy(type_, window->GetFrame()->GetPage());
  }
}

void Sensor::ContextDestroyed() {
  if (!IsIdleOrErrored())
    Deactivate();

  state_ = SensorState::kIdle;

  if (sensor_proxy_)
    sensor_proxy_->Detach();
}

void Sensor::OnSensorInitialized() {
  if (state_ != SensorState::kActivating)
    return;

  RequestAddConfiguration();
}

void Sensor::OnSensorReadingChanged() {
  if (state_ != SensorState::kActivated)
    return;

  // Return if reading update is already scheduled or the cached
  // reading is up-to-date.
  if (pending_reading_notification_.IsActive())
    return;
  double elapsedTime =
      sensor_proxy_->GetReading().timestamp() - last_reported_timestamp_;
  DCHECK_GT(elapsedTime, 0.0);

  DCHECK_GT(configuration_->frequency, 0.0);
  double waitingTime = 1 / configuration_->frequency - elapsedTime;

  // Negative or zero 'waitingTime' means that polling period has elapsed.
  // We also avoid scheduling if the elapsed time is slightly behind the
  // polling period.
  auto sensor_reading_changed =
      WTF::BindOnce(&Sensor::NotifyReading, WrapWeakPersistent(this));
  if (waitingTime < kWaitingIntervalThreshold) {
    // Invoke JS callbacks in a different callchain to obviate
    // possible modifications of SensorProxy::observers_ container
    // while it is being iterated through.
    pending_reading_notification_ = PostCancellableTask(
        *GetExecutionContext()->GetTaskRunner(TaskType::kSensor), FROM_HERE,
        std::move(sensor_reading_changed));
  } else {
    pending_reading_notification_ = PostDelayedCancellableTask(
        *GetExecutionContext()->GetTaskRunner(TaskType::kSensor), FROM_HERE,
        std::move(sensor_reading_changed), base::Seconds(waitingTime));
  }
}

void Sensor::OnSensorError(DOMExceptionCode code,
                           const String& sanitized_message,
                           const String& unsanitized_message) {
  HandleError(code, sanitized_message, unsanitized_message);
}

void Sensor::OnAddConfigurationRequestCompleted(bool result) {
  if (state_ != SensorState::kActivating)
    return;

  if (!result) {
    HandleError(DOMExceptionCode::kNotReadableError,
                "start() call has failed.");
    return;
  }

  if (!GetExecutionContext())
    return;

  pending_activated_notification_ = PostCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kSensor), FROM_HERE,
      WTF::BindOnce(&Sensor::NotifyActivated, WrapWeakPersistent(this)));
}

void Sensor::Activate() {
  DCHECK_EQ(state_, SensorState::kActivating);

  InitSensorProxyIfNeeded();
  DCHECK(sensor_proxy_);

  if (sensor_proxy_->IsInitialized())
    RequestAddConfiguration();
  else
    sensor_proxy_->Initialize();

  sensor_proxy_->AddObserver(this);
}

void Sensor::Deactivate() {
  DCHECK_NE(state_, SensorState::kIdle);
  // state_ is not set to kIdle here as on error it should
  // transition to the kIdle state in the same call chain
  // the error event is dispatched, i.e. inside NotifyError().
  pending_reading_notification_.Cancel();
  pending_activated_notification_.Cancel();
  pending_error_notification_.Cancel();

  if (!sensor_proxy_)
    return;

  if (sensor_proxy_->IsInitialized()) {
    DCHECK(configuration_);
    sensor_proxy_->RemoveConfiguration(configuration_->Clone());
    last_reported_timestamp_ = 0.0;
  }

  sensor_proxy_->RemoveObserver(this);
}

void Sensor::RequestAddConfiguration() {
  if (!configuration_) {
    configuration_ = CreateSensorConfig();
    DCHECK(configuration_);
    DCHECK_GE(configuration_->frequency,
              sensor_proxy_->GetFrequencyLimits().first);
    DCHECK_LE(configuration_->frequency,
              sensor_proxy_->GetFrequencyLimits().second);
  }

  DCHECK(sensor_proxy_);
  sensor_proxy_->AddConfiguration(
      configuration_->Clone(),
      WTF::BindOnce(&Sensor::OnAddConfigurationRequestCompleted,
                    WrapWeakPersistent(this)));
}

void Sensor::HandleError(DOMExceptionCode code,
                         const String& sanitized_message,
                         const String& unsanitized_message) {
  if (!GetExecutionContext()) {
    // Deactivate() is already called from Sensor::ContextDestroyed().
    return;
  }

  if (IsIdleOrErrored())
    return;

  Deactivate();

  auto* error = MakeGarbageCollected<DOMException>(code, sanitized_message,
                                                   unsanitized_message);
  pending_error_notification_ = PostCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kSensor), FROM_HERE,
      WTF::BindOnce(&Sensor::NotifyError, WrapWeakPersistent(this),
                    WrapPersistent(error)));
}

void Sensor::NotifyReading() {
  DCHECK_EQ(state_, SensorState::kActivated);
  last_reported_timestamp_ = sensor_proxy_->GetReading().timestamp();
  DispatchEvent(*Event::Create(event_type_names::kReading));
}

void Sensor::NotifyActivated() {
  DCHECK_EQ(state_, SensorState::kActivating);
  state_ = SensorState::kActivated;

  if (hasReading()) {
    // If reading has already arrived, process the reading values (a subclass
    // may do some filtering, for example) and then send an initial "reading"
    // event right away.
    DCHECK(!pending_reading_notification_.IsActive());
    pending_reading_notification_ = PostCancellableTask(
        *GetExecutionContext()->GetTaskRunner(TaskType::kSensor), FROM_HERE,
        WTF::BindOnce(&Sensor::OnSensorReadingChanged,
                      WrapWeakPersistent(this)));
  }

  DispatchEvent(*Event::Create(event_type_names::kActivate));
}

void Sensor::NotifyError(DOMException* error) {
  DCHECK_NE(state_, SensorState::kIdle);
  state_ = SensorState::kIdle;
  DispatchEvent(*SensorErrorEvent::Create(event_type_names::kError, error));
}

bool Sensor::IsIdleOrErrored() const {
  return (state_ == SensorState::kIdle) ||
         pending_error_notification_.IsActive();
}

const device::SensorReading& Sensor::GetReading() const {
  DCHECK(sensor_proxy_);
  return sensor_proxy_->GetReading(use_screen_coords_);
}

}  // namespace blink
