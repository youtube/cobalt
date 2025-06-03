// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge_manager_impl.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/scoped_nudge_pause.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ui/base/nudge_util.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Returns the `base::TimeDelta` constant based on a `NudgeDuration` enum value.
base::TimeDelta GetNudgeDuration(NudgeDuration duration) {
  switch (duration) {
    case NudgeDuration::kDefaultDuration:
      return AnchoredNudgeManagerImpl::kNudgeDefaultDuration;
    case NudgeDuration::kMediumDuration:
      return AnchoredNudgeManagerImpl::kNudgeMediumDuration;
    case NudgeDuration::kLongDuration:
      return AnchoredNudgeManagerImpl::kNudgeLongDuration;
  }
}

// An implicit animation observer that tracks the nudge widget's hide animation.
// Once the animation is complete the nudge widget will be destroyed.
class HideAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  // TODO(b/296948349): Pass the nudge id instead of a pointer to the nudge
  // and access it through a new `GetNudge(id)` function.
  HideAnimationObserver(AnchoredNudge* anchored_nudge)
      : anchored_nudge_(anchored_nudge) {}

  HideAnimationObserver(const HideAnimationObserver&) = delete;
  HideAnimationObserver& operator=(const HideAnimationObserver&) = delete;

  ~HideAnimationObserver() override { StopObservingImplicitAnimations(); }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    if (!anchored_nudge_) {
      return;
    }

    // Return early if the nudge widget has already been closed.
    auto* nudge_widget = anchored_nudge_->GetWidget();
    if (!nudge_widget || nudge_widget->IsClosed()) {
      return;
    }

    // `this` and other observers cleanup occurs on `OnWidgetDestroying()`.
    nudge_widget->CloseNow();
  }

 private:
  // Owned by the views hierarchy.
  raw_ptr<AnchoredNudge, ExperimentalAsh> anchored_nudge_;
};

}  // namespace

// Owns a `base::OneShotTimer` that can be paused and resumed.
class AnchoredNudgeManagerImpl::PausableTimer {
 public:
  PausableTimer() = default;
  PausableTimer(const PausableTimer&) = delete;
  PausableTimer& operator=(const PausableTimer&) = delete;
  ~PausableTimer() = default;

  void Start(base::TimeDelta duration, base::RepeatingClosure task) {
    DCHECK(!timer_.IsRunning());
    task_ = task;
    remaining_duration_ = duration;
    time_last_started_ = base::TimeTicks::Now();
    timer_.Start(FROM_HERE, remaining_duration_, task_);
  }

  void Pause() {
    DCHECK(timer_.IsRunning());
    timer_.Stop();
    remaining_duration_ -= base::TimeTicks::Now() - time_last_started_;
  }

  void Resume() {
    time_last_started_ = base::TimeTicks::Now();
    timer_.Start(FROM_HERE, remaining_duration_, task_);
  }

  void Stop() {
    remaining_duration_ = base::Seconds(0);
    task_.Reset();
    timer_.Stop();
  }

 private:
  base::OneShotTimer timer_;
  base::RepeatingClosure task_;
  base::TimeDelta remaining_duration_;
  base::TimeTicks time_last_started_;
};

// A hover observer used to pause or resume the dismiss timer, and to run
// provided callbacks that execute on hover state changes.
class AnchoredNudgeManagerImpl::NudgeHoverObserver : public ui::EventObserver {
 public:
  NudgeHoverObserver(aura::Window* widget_window,
                     const std::string& nudge_id,
                     HoverStateChangeCallback hover_state_change_callback,
                     AnchoredNudgeManagerImpl* anchored_nudge_mananger)
      : event_monitor_(views::EventMonitor::CreateWindowMonitor(
            /*event_observer=*/this,
            widget_window,
            {ui::ET_MOUSE_ENTERED, ui::ET_MOUSE_EXITED})),
        nudge_id_(nudge_id),
        hover_state_change_callback_(std::move(hover_state_change_callback)),
        anchored_nudge_manager_(anchored_nudge_mananger) {}

  NudgeHoverObserver(const NudgeHoverObserver&) = delete;

  NudgeHoverObserver& operator=(const NudgeHoverObserver&) = delete;

  ~NudgeHoverObserver() override = default;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    switch (event.type()) {
      case ui::ET_MOUSE_ENTERED:
        anchored_nudge_manager_->OnNudgeHoverStateChanged(nudge_id_,
                                                          /*is_hovering=*/true);
        if (!hover_state_change_callback_.is_null()) {
          std::move(hover_state_change_callback_).Run(true);
        }
        break;
      case ui::ET_MOUSE_EXITED:
        anchored_nudge_manager_->OnNudgeHoverStateChanged(
            nudge_id_, /*is_hovering=*/false);
        if (!hover_state_change_callback_.is_null()) {
          std::move(hover_state_change_callback_).Run(false);
        }
        break;
      default:
        NOTREACHED();
        break;
    }
  }

 private:
  // While this `EventMonitor` object exists, this object will only look for
  // `ui::ET_MOUSE_ENTERED` and `ui::ET_MOUSE_EXITED` events that occur in the
  // `widget_window` indicated in the constructor.
  std::unique_ptr<views::EventMonitor> event_monitor_;

  const std::string nudge_id_;

  // This is run whenever the mouse enters or exits the observed window with a
  // parameter to indicate whether the window is being hovered.
  HoverStateChangeCallback hover_state_change_callback_;

  // `NudgeHoverObserver` is guaranteed to not outlive
  // `anchored_nudge_manager_`, which is owned by `Shell`.
  const raw_ptr<AnchoredNudgeManagerImpl, ExperimentalAsh>
      anchored_nudge_manager_;
};

// A view observer that is used to close the nudge's widget whenever its
// `anchor_view` is deleted.
class AnchoredNudgeManagerImpl::AnchorViewObserver
    : public views::ViewObserver {
 public:
  // TODO(b/296948349): Pass the nudge id instead of a pointer to the nudge
  // and access it through a new `GetNudge(id)` function.
  AnchorViewObserver(AnchoredNudge* anchored_nudge,
                     views::View* anchor_view,
                     AnchoredNudgeManagerImpl* anchored_nudge_manager)
      : anchored_nudge_(anchored_nudge),
        anchor_view_(anchor_view),
        anchored_nudge_manager_(anchored_nudge_manager) {
    anchor_view_->AddObserver(this);
  }

  AnchorViewObserver(const AnchorViewObserver&) = delete;

  AnchorViewObserver& operator=(const AnchorViewObserver&) = delete;

  ~AnchorViewObserver() override {
    if (anchor_view_) {
      anchor_view_->RemoveObserver(this);
    }
  }

  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    HandleAnchorViewIsDeletingOrHiding(observed_view);
  }

  // ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override {
    if (!observed_view->GetVisible()) {
      HandleAnchorViewIsDeletingOrHiding(observed_view);
    }
  }

  void HandleAnchorViewIsDeletingOrHiding(views::View* observed_view) {
    CHECK_EQ(anchor_view_, observed_view);
    const std::string id = anchored_nudge_->id();

    // Make sure the nudge bubble no longer observes the anchor view.
    anchored_nudge_->SetAnchorView(nullptr);
    anchor_view_->RemoveObserver(this);
    anchor_view_ = nullptr;
    anchored_nudge_ = nullptr;
    anchored_nudge_manager_->Cancel(id);
  }

 private:
  // Owned by the views hierarchy.
  raw_ptr<AnchoredNudge> anchored_nudge_;
  raw_ptr<views::View> anchor_view_;

  // `AnchorViewObserver` is guaranteed to not outlive
  // `anchored_nudge_manager_`, which is owned by `Shell`.
  raw_ptr<AnchoredNudgeManagerImpl> anchored_nudge_manager_;
};

// A widget observer that is used to clean up the cached objects related to a
// nudge when its widget is destroying.
class AnchoredNudgeManagerImpl::NudgeWidgetObserver
    : public views::WidgetObserver {
 public:
  // TODO(b/296948349): Pass the nudge id instead of a pointer to the nudge
  // and access it through a new `GetNudge(id)` function.
  NudgeWidgetObserver(AnchoredNudge* anchored_nudge,
                      AnchoredNudgeManagerImpl* anchored_nudge_manager)
      : anchored_nudge_(anchored_nudge),
        anchored_nudge_manager_(anchored_nudge_manager) {
    DCHECK(anchored_nudge->GetWidget());
    anchored_nudge->GetWidget()->AddObserver(this);
  }

  NudgeWidgetObserver(const NudgeWidgetObserver&) = delete;

  NudgeWidgetObserver& operator=(const NudgeWidgetObserver&) = delete;

  ~NudgeWidgetObserver() override {
    if (anchored_nudge_ && anchored_nudge_->GetWidget()) {
      anchored_nudge_->GetWidget()->RemoveObserver(this);
    }
  }

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    widget->RemoveObserver(this);
    anchored_nudge_manager_->HandleNudgeWidgetDestroying(anchored_nudge_->id());
  }

 private:
  // Owned by the views hierarchy.
  raw_ptr<AnchoredNudge> anchored_nudge_;

  // `NudgeWidgetObserver` is guaranteed to not outlive
  // `anchored_nudge_manager_`, which is owned by `Shell`.
  raw_ptr<AnchoredNudgeManagerImpl> anchored_nudge_manager_;
};

AnchoredNudgeManagerImpl::AnchoredNudgeManagerImpl() {
  DCHECK(features::IsSystemNudgeV2Enabled());
  Shell::Get()->session_controller()->AddObserver(this);
}

AnchoredNudgeManagerImpl::~AnchoredNudgeManagerImpl() {
  CloseAllNudges();

  Shell::Get()->session_controller()->RemoveObserver(this);
}

void AnchoredNudgeManagerImpl::Show(AnchoredNudgeData& nudge_data) {
  std::string id = nudge_data.id;
  CHECK(!id.empty());

  // If `pause_counter_` is greater than 0, no nudges should be shown.
  if (pause_counter_ > 0) {
    return;
  }

  views::View* anchor_view = nudge_data.GetAnchorView();

  // Nudges with an anchor view won't show if their `anchor_view` was deleted,
  // it is not visible or does not have a widget.
  if (nudge_data.is_anchored() && (!anchor_view || !anchor_view->GetVisible() ||
                                   !anchor_view->GetWidget())) {
    return;
  }

  // If `id` is already in use, close the nudge without triggering its hide
  // animation so it can be immediately replaced.
  if (base::Contains(shown_nudges_, id)) {
    auto* nudge_widget = shown_nudges_[id]->GetWidget();
    if (nudge_widget && !nudge_widget->IsClosed()) {
      // Cache cleanup occurs on nudge's `OnWidgetDestroying()`.
      nudge_widget->CloseNow();
    }
  }

  // Chain callbacks with `Cancel()` so nudge is dismissed on button pressed.
  // TODO(b/285023559): Add `ChainedCancelCallback` class so we don't have to
  // manually modify the provided callbacks.
  if (!nudge_data.first_button_text.empty()) {
    nudge_data.first_button_callback =
        ChainCancelCallback(nudge_data.first_button_callback,
                            nudge_data.catalog_name, id, /*first_button=*/true);
  }

  if (!nudge_data.second_button_text.empty()) {
    nudge_data.second_button_callback = ChainCancelCallback(
        nudge_data.second_button_callback, nudge_data.catalog_name, id,
        /*first_button=*/false);
  }

  nudge_data.close_button_callback = base::BindRepeating(
      &AnchoredNudgeManagerImpl::Cancel, base::Unretained(this), id);

  auto anchored_nudge = std::make_unique<AnchoredNudge>(nudge_data);
  auto* anchored_nudge_ptr = anchored_nudge.get();
  shown_nudges_[id] = anchored_nudge_ptr;

  auto* anchored_nudge_widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(anchored_nudge));

  // The widget is not activated so the nudge does not steal focus.
  anchored_nudge_widget->ShowInactive();

  RecordNudgeShown(nudge_data.catalog_name);

  nudge_widget_observers_[id] = std::make_unique<NudgeWidgetObserver>(
      anchored_nudge_ptr, /*anchored_nudge_manager=*/this);

  if (anchor_view) {
    anchor_view_observers_[id] = std::make_unique<AnchorViewObserver>(
        anchored_nudge_ptr, anchor_view, /*anchored_nudge_manager=*/this);
  }

  nudge_hover_observers_[id] = std::make_unique<NudgeHoverObserver>(
      anchored_nudge_widget->GetNativeWindow(), id,
      std::move(nudge_data.hover_state_change_callback),
      /*anchored_nudge_manager=*/this);

  // Nudge duration will be updated from default to medium if the nudge has a
  // button or its body text has `kLongBodyTextLength` or more characters.
  if (nudge_data.duration == NudgeDuration::kDefaultDuration &&
      (nudge_data.body_text.length() >= kLongBodyTextLength ||
       !nudge_data.first_button_text.empty())) {
    nudge_data.duration = NudgeDuration::kMediumDuration;
  }

  dismiss_timers_[id].Start(
      GetNudgeDuration(nudge_data.duration),
      base::BindRepeating(&AnchoredNudgeManagerImpl::Cancel,
                          base::Unretained(this), id));
}

void AnchoredNudgeManagerImpl::Cancel(const std::string& id) {
  // Return early if the nudge is not cached in `shown_nudges_`, or the nudge
  // hide animation is already being observed.
  if (!base::Contains(shown_nudges_, id) ||
      base::Contains(hide_animation_observers_, id)) {
    return;
  }

  auto* anchored_nudge = shown_nudges_[id].get();
  auto* nudge_widget = anchored_nudge->GetWidget();

  // Return early if the nudge widget has been closed.
  if (!nudge_widget || nudge_widget->IsClosed()) {
    return;
  }

  // Observe hide animation to close the nudge widget on animation completed.
  hide_animation_observers_[id] =
      std::make_unique<HideAnimationObserver>(anchored_nudge);
  ui::ScopedLayerAnimationSettings animation_settings(
      nudge_widget->GetLayer()->GetAnimator());
  animation_settings.AddObserver(hide_animation_observers_[id].get());

  // Trigger the nudge widget hide animation. Widget is properly closed on
  // `OnImplicitAnimationsCompleted()`.
  nudge_widget->Hide();
}

void AnchoredNudgeManagerImpl::MaybeRecordNudgeAction(
    NudgeCatalogName catalog_name) {
  auto& nudge_registry = GetNudgeRegistry();
  auto it = std::find_if(
      std::begin(nudge_registry), std::end(nudge_registry),
      [catalog_name](
          const std::pair<NudgeCatalogName, base::TimeTicks> registry_entry) {
        return catalog_name == registry_entry.first;
      });

  // Don't record "TimeToAction" metric if the nudge hasn't been shown before.
  if (it == std::end(nudge_registry)) {
    return;
  }

  base::UmaHistogramEnumeration(chromeos::GetNudgeTimeToActionHistogramName(
                                    base::TimeTicks::Now() - it->second),
                                catalog_name);

  nudge_registry.erase(it);
}

std::unique_ptr<ScopedNudgePause>
AnchoredNudgeManagerImpl::CreateScopedPause() {
  return std::make_unique<ScopedNudgePause>();
}

void AnchoredNudgeManagerImpl::HandleNudgeWidgetDestroying(
    const std::string& id) {
  // TODO(b/296948349): Handle all observers in a single struct so they can be
  // destroyed together.
  dismiss_timers_.erase(id);
  nudge_hover_observers_.erase(id);
  if (anchor_view_observers_[id]) {
    anchor_view_observers_.erase(id);
  }
  hide_animation_observers_.erase(id);
  nudge_widget_observers_.erase(id);
  shown_nudges_.erase(id);
}

void AnchoredNudgeManagerImpl::OnNudgeHoverStateChanged(const std::string& id,
                                                        bool is_hovering) {
  if (is_hovering) {
    dismiss_timers_[id].Pause();
  } else {
    dismiss_timers_[id].Resume();
  }
}

void AnchoredNudgeManagerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  CloseAllNudges();
}

// TODO(b/296948349): Replace this with a new `GetNudge(id)` function as this
// does not accurately reflect is a nudge is shown or not.
bool AnchoredNudgeManagerImpl::IsNudgeShown(const std::string& id) {
  return base::Contains(shown_nudges_, id);
}

const std::u16string& AnchoredNudgeManagerImpl::GetNudgeBodyTextForTest(
    const std::string& id) {
  CHECK(base::Contains(shown_nudges_, id));
  return shown_nudges_[id]->GetBodyText();
}

views::View* AnchoredNudgeManagerImpl::GetNudgeAnchorViewForTest(
    const std::string& id) {
  CHECK(base::Contains(shown_nudges_, id));
  return shown_nudges_[id]->GetAnchorView();
}

views::LabelButton* AnchoredNudgeManagerImpl::GetNudgeFirstButtonForTest(
    const std::string& id) {
  CHECK(base::Contains(shown_nudges_, id));
  return shown_nudges_[id]->GetFirstButton();
}

views::LabelButton* AnchoredNudgeManagerImpl::GetNudgeSecondButtonForTest(
    const std::string& id) {
  CHECK(base::Contains(shown_nudges_, id));
  return shown_nudges_[id]->GetSecondButton();
}

AnchoredNudge* AnchoredNudgeManagerImpl::GetShownNudgeForTest(
    const std::string& id) {
  CHECK(base::Contains(shown_nudges_, id));
  return shown_nudges_[id];
}

void AnchoredNudgeManagerImpl::ResetNudgeRegistryForTesting() {
  GetNudgeRegistry().clear();
}

// static
std::vector<std::pair<NudgeCatalogName, base::TimeTicks>>&
AnchoredNudgeManagerImpl::GetNudgeRegistry() {
  static auto nudge_registry =
      std::vector<std::pair<NudgeCatalogName, base::TimeTicks>>();
  return nudge_registry;
}

void AnchoredNudgeManagerImpl::RecordNudgeShown(NudgeCatalogName catalog_name) {
  base::UmaHistogramEnumeration(
      chromeos::kNotifierFrameworkNudgeShownCountHistogram, catalog_name);

  // Record nudge shown time in the nudge registry.
  auto& nudge_registry = GetNudgeRegistry();
  auto it = std::find_if(
      std::begin(nudge_registry), std::end(nudge_registry),
      [catalog_name](
          const std::pair<NudgeCatalogName, base::TimeTicks> registry_entry) {
        return catalog_name == registry_entry.first;
      });

  if (it == std::end(nudge_registry)) {
    nudge_registry.emplace_back(catalog_name, base::TimeTicks::Now());
  } else {
    it->second = base::TimeTicks::Now();
  }
}

void AnchoredNudgeManagerImpl::RecordButtonPressed(
    NudgeCatalogName catalog_name,
    bool first_button) {
  base::UmaHistogramEnumeration(
      first_button ? "Ash.NotifierFramework.Nudge.FirstButtonPressed"
                   : "Ash.NotifierFramework.Nudge.SecondButtonPressed",
      catalog_name);
}

void AnchoredNudgeManagerImpl::CloseAllNudges() {
  // A while-loop over the original list is used to avoid race conditions that
  // could occur by copying the list, making it possible to iterate through an
  // item that might not exist in `shown_nudges_` anymore.
  while (!shown_nudges_.empty()) {
    auto* nudge_widget = shown_nudges_.begin()->second->GetWidget();
    if (nudge_widget && !nudge_widget->IsClosed()) {
      // Cache cleanup occurs on nudge's `OnWidgetDestroying()`.
      nudge_widget->CloseNow();
    }
  }
}

base::RepeatingClosure AnchoredNudgeManagerImpl::ChainCancelCallback(
    base::RepeatingClosure callback,
    NudgeCatalogName catalog_name,
    const std::string& id,
    bool first_button) {
  return std::move(callback)
      .Then(base::BindRepeating(&AnchoredNudgeManagerImpl::Cancel,
                                base::Unretained(this), id))
      .Then(base::BindRepeating(&AnchoredNudgeManagerImpl::RecordButtonPressed,
                                base::Unretained(this), catalog_name,
                                first_button));
}

void AnchoredNudgeManagerImpl::Pause() {
  ++pause_counter_;

  // Immediately close all nudges.
  CloseAllNudges();
}

void AnchoredNudgeManagerImpl::Resume() {
  CHECK_GT(pause_counter_, 0);
  --pause_counter_;
}

}  // namespace ash
