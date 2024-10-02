// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/mojom/accessibility_helper.mojom-shared.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "ash/public/cpp/window_properties.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/ash/arc/accessibility/geometry_util.h"
#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/pref_names.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/event_router.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/controls/native/native_view_host.h"

namespace arc {
namespace {

using ::ash::AccessibilityManager;
using ::ash::AccessibilityNotificationType;
using ::ash::MagnificationManager;

// ClassName for toast from ARC++ R onwards.
constexpr char kToastEventSourceArcR[] = "android.widget.Toast";
// TODO(sarakato): Remove this once ARC++ P has been deprecated.
constexpr char kToastEventSourceArcP[] = "android.widget.Toast$TN";

bool ShouldAnnounceEvent(arc::mojom::AccessibilityEventData* event_data) {
  if (event_data->event_type ==
      arc::mojom::AccessibilityEventType::ANNOUNCEMENT) {
    return true;
  } else if (event_data->event_type ==
             arc::mojom::AccessibilityEventType::NOTIFICATION_STATE_CHANGED) {
    // Only announce the event from toast.
    if (!event_data->string_properties)
      return false;

    auto it = event_data->string_properties->find(
        arc::mojom::AccessibilityEventStringProperty::CLASS_NAME);
    if (it == event_data->string_properties->end())
      return false;

    return (it->second == kToastEventSourceArcP) ||
           (it->second == kToastEventSourceArcR);
  }
  return false;
}

void DispatchFocusChange(arc::mojom::AccessibilityNodeInfoData* node_data,
                         Profile* profile) {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (!node_data || !accessibility_manager ||
      accessibility_manager->profile() != profile)
    return;

  DCHECK(exo::WMHelper::HasInstance());
  aura::Window* active_window = exo::WMHelper::GetInstance()->GetActiveWindow();
  if (!active_window)
    return;

  // Convert bounds from Android pixels to Chrome DIP, and adjust coordinate to
  // Chrome's screen coordinate.
  gfx::Rect bounds_in_screen = gfx::ScaleToEnclosingRect(
      node_data->bounds_in_screen,
      1.0f / exo::WMHelper::GetInstance()->GetDeviceScaleFactorForWindow(
                 active_window));
  bounds_in_screen.Offset(0,
                          arc::GetChromeWindowHeightOffsetInDip(active_window));

  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(active_window);
  bounds_in_screen.Offset(display.bounds().x(), display.bounds().y());

  accessibility_manager->OnViewFocusedInArc(bounds_in_screen);
}

// Singleton factory for ArcAccessibilityHelperBridge.
class ArcAccessibilityHelperBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAccessibilityHelperBridge,
          ArcAccessibilityHelperBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAccessibilityHelperBridgeFactory";

  static ArcAccessibilityHelperBridgeFactory* GetInstance() {
    return base::Singleton<ArcAccessibilityHelperBridgeFactory>::get();
  }

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override { return true; }

 private:
  friend struct base::DefaultSingletonTraits<
      ArcAccessibilityHelperBridgeFactory>;

  ArcAccessibilityHelperBridgeFactory() {
    // ArcAccessibilityHelperBridge needs to track task creation and
    // destruction in the container, which are notified to ArcAppListPrefs
    // via Mojo.
    DependsOn(ArcAppListPrefsFactory::GetInstance());

    // ArcAccessibilityHelperBridge needs to track visibility change of Android
    // keyboard to delete its accessibility tree when it becomes hidden.
    DependsOn(ArcInputMethodManagerService::GetFactory());
  }
  ~ArcAccessibilityHelperBridgeFactory() override = default;
};

static constexpr const char* kTextShadowRaised =
    "-2px -2px 4px rgba(0, 0, 0, 0.5)";
static constexpr const char* kTextShadowDepressed =
    "2px 2px 4px rgba(0, 0, 0, 0.5)";
static constexpr const char* kTextShadowUniform =
    "-1px 0px 0px black, 0px -1px 0px black, 1px 0px 0px black, 0px  1px 0px "
    "black";
static constexpr const char* kTextShadowDropShadow =
    "0px 0px 2px rgba(0, 0, 0, 0.5), 2px 2px 2px black";

std::string GetARGBFromPrefs(PrefService* prefs,
                             const char* color_pref_name,
                             const char* opacity_pref_name) {
  const std::string color = prefs->GetString(color_pref_name);
  if (color.empty()) {
    return "";
  }
  const int opacity = prefs->GetInteger(opacity_pref_name);
  return base::StringPrintf("rgba(%s,%s)", color.c_str(),
                            base::NumberToString(opacity / 100.0).c_str());
}

}  // namespace

arc::mojom::CaptionStylePtr GetCaptionStyleFromPrefs(PrefService* prefs) {
  DCHECK(prefs);

  arc::mojom::CaptionStylePtr style = arc::mojom::CaptionStyle::New();

  style->text_size = prefs->GetString(prefs::kAccessibilityCaptionsTextSize);
  style->text_color =
      GetARGBFromPrefs(prefs, prefs::kAccessibilityCaptionsTextColor,
                       prefs::kAccessibilityCaptionsTextOpacity);
  style->background_color =
      GetARGBFromPrefs(prefs, prefs::kAccessibilityCaptionsBackgroundColor,
                       prefs::kAccessibilityCaptionsBackgroundOpacity);
  style->user_locale = prefs->GetString(language::prefs::kApplicationLocale);

  const std::string test_shadow =
      prefs->GetString(prefs::kAccessibilityCaptionsTextShadow);
  if (test_shadow == kTextShadowRaised) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::RAISED;
  } else if (test_shadow == kTextShadowDepressed) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::DEPRESSED;
  } else if (test_shadow == kTextShadowUniform) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::UNIFORM;
  } else if (test_shadow == kTextShadowDropShadow) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::DROP_SHADOW;
  } else {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::NONE;
  }

  return style;
}

// static
void ArcAccessibilityHelperBridge::CreateFactory() {
  ArcAccessibilityHelperBridgeFactory::GetInstance();
}

// static
ArcAccessibilityHelperBridge*
ArcAccessibilityHelperBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAccessibilityHelperBridgeFactory::GetForBrowserContext(context);
}

// The list of prefs we want to observe.
const char* const kCaptionStylePrefsToObserve[] = {
    prefs::kAccessibilityCaptionsTextSize,
    prefs::kAccessibilityCaptionsTextFont,
    prefs::kAccessibilityCaptionsTextColor,
    prefs::kAccessibilityCaptionsTextOpacity,
    prefs::kAccessibilityCaptionsBackgroundColor,
    prefs::kAccessibilityCaptionsTextShadow,
    prefs::kAccessibilityCaptionsBackgroundOpacity,
    language::prefs::kApplicationLocale};

ArcAccessibilityHelperBridge::ArcAccessibilityHelperBridge(
    content::BrowserContext* browser_context,
    ArcBridgeService* arc_bridge_service)
    : profile_(Profile::FromBrowserContext(browser_context)),
      arc_bridge_service_(arc_bridge_service),
      accessibility_helper_instance_(arc_bridge_service_),
      tree_tracker_(this,
                    profile_,
                    accessibility_helper_instance_,
                    arc_bridge_service_) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(
      Profile::FromBrowserContext(browser_context)->GetPrefs());

  for (const char* const pref_name : kCaptionStylePrefsToObserve) {
    pref_change_registrar_->Add(
        pref_name, base::BindRepeating(
                       &ArcAccessibilityHelperBridge::UpdateCaptionSettings,
                       base::Unretained(this)));
  }

  arc_bridge_service_->accessibility_helper()->SetHost(this);
  arc_bridge_service_->accessibility_helper()->AddObserver(this);

  automation_event_router_observer_.Observe(
      extensions::AutomationEventRouter::GetInstance());
}

ArcAccessibilityHelperBridge::~ArcAccessibilityHelperBridge() = default;

void ArcAccessibilityHelperBridge::SetNativeChromeVoxArcSupport(
    bool enabled,
    SetNativeChromeVoxCallback callback) {
  tree_tracker_.SetNativeChromeVoxArcSupport(enabled, std::move(callback));
}

bool ArcAccessibilityHelperBridge::EnableTree(const ui::AXTreeID& tree_id) {
  return tree_tracker_.EnableTree(tree_id);
}

void ArcAccessibilityHelperBridge::Shutdown() {
  // We do not unregister ourselves from WMHelper as an ActivationObserver
  // because it is always null at this point during teardown.

  arc_bridge_service_->accessibility_helper()->RemoveObserver(this);
  arc_bridge_service_->accessibility_helper()->SetHost(nullptr);

  tree_tracker_.Shutdown();
}

void ArcAccessibilityHelperBridge::OnConnectionReady() {
  UpdateEnabledFeature();
  UpdateCaptionSettings();

  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (accessibility_manager) {
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &ArcAccessibilityHelperBridge::OnAccessibilityStatusChanged,
            base::Unretained(this)));
    accessibility_helper_instance_.SetExploreByTouchEnabled(
        accessibility_manager->IsSpokenFeedbackEnabled());
  }
}

void ArcAccessibilityHelperBridge::OnAccessibilityEvent(
    mojom::AccessibilityEventDataPtr event_data) {
  filter_type_ = GetFilterType();
  switch (filter_type_) {
    case arc::mojom::AccessibilityFilterType::ALL:
      HandleFilterTypeAllEvent(std::move(event_data));
      break;
    case arc::mojom::AccessibilityFilterType::FOCUS:
      HandleFilterTypeFocusEvent(std::move(event_data));
      break;
    case arc::mojom::AccessibilityFilterType::OFF:
      break;
  }
}

void ArcAccessibilityHelperBridge::OnNotificationStateChanged(
    const std::string& notification_key,
    arc::mojom::AccessibilityNotificationStateType state) {
  tree_tracker_.OnNotificationStateChanged(std::move(notification_key),
                                           std::move(state));
}

void ArcAccessibilityHelperBridge::OnToggleNativeChromeVoxArcSupport(
    bool enabled) {
  tree_tracker_.OnToggleNativeChromeVoxArcSupport(enabled);
}

void ArcAccessibilityHelperBridge::OnAction(
    const ui::AXActionData& data) const {
  DCHECK(data.target_node_id);

  AXTreeSourceArc* tree_source =
      tree_tracker_.GetFromTreeId(data.target_tree_id);
  if (!tree_source)
    return;

  absl::optional<int32_t> window_id = tree_source->window_id();
  if (!window_id)
    return;

  const absl::optional<mojom::AccessibilityActionType> action =
      ConvertToAndroidAction(data.action);
  if (!action.has_value())
    return;

  arc::mojom::AccessibilityActionDataPtr action_data =
      arc::mojom::AccessibilityActionData::New();

  action_data->node_id = data.target_node_id;
  action_data->window_id = window_id.value();
  action_data->action_type = action.value();
  PopulateActionParameters(data, *action_data);

  if (action == arc::mojom::AccessibilityActionType::GET_TEXT_LOCATION) {
    action_data->start_index = data.start_index;
    action_data->end_index = data.end_index;
    if (!accessibility_helper_instance_.RefreshWithExtraData(
            std::move(action_data),
            base::BindOnce(
                &ArcAccessibilityHelperBridge::OnGetTextLocationDataResult,
                base::Unretained(this), data))) {
      OnActionResult(data, false);
    }
    return;
  }

  if (!accessibility_helper_instance_.PerformAction(
          std::move(action_data),
          base::BindOnce(&ArcAccessibilityHelperBridge::OnActionResult,
                         base::Unretained(this), data))) {
    // TODO(b/146809329): This case should probably destroy all trees.
    OnActionResult(data, false);
  }
}

void ArcAccessibilityHelperBridge::PopulateActionParameters(
    const ui::AXActionData& chrome_data,
    arc::mojom::AccessibilityActionData& action_data) const {
  switch (action_data.action_type) {
    case mojom::AccessibilityActionType::SCROLL_TO_POSITION: {
      base::flat_map<arc::mojom::ActionIntArgumentType, int32_t> args;
      const auto [row, column] = chrome_data.row_column;
      args[arc::mojom::ActionIntArgumentType::ROW_INT] = row;
      args[arc::mojom::ActionIntArgumentType::COLUMN_INT] = column;
      action_data.int_parameters = args;
      break;
    }
    case mojom::AccessibilityActionType::CUSTOM_ACTION:
      action_data.custom_action_id = chrome_data.custom_action_id;
      break;
    case mojom::AccessibilityActionType::NEXT_HTML_ELEMENT:
    case mojom::AccessibilityActionType::PREVIOUS_HTML_ELEMENT:
    case mojom::AccessibilityActionType::FOCUS:
    case mojom::AccessibilityActionType::CLEAR_FOCUS:
    case mojom::AccessibilityActionType::SELECT:
    case mojom::AccessibilityActionType::CLEAR_SELECTION:
    case mojom::AccessibilityActionType::CLICK:
    case mojom::AccessibilityActionType::LONG_CLICK:
    case mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS:
    case mojom::AccessibilityActionType::CLEAR_ACCESSIBILITY_FOCUS:
    case mojom::AccessibilityActionType::NEXT_AT_MOVEMENT_GRANULARITY:
    case mojom::AccessibilityActionType::PREVIOUS_AT_MOVEMENT_GRANULARITY:
    case mojom::AccessibilityActionType::SCROLL_FORWARD:
    case mojom::AccessibilityActionType::SCROLL_BACKWARD:
    case mojom::AccessibilityActionType::COPY:
    case mojom::AccessibilityActionType::PASTE:
    case mojom::AccessibilityActionType::CUT:
    case mojom::AccessibilityActionType::SET_SELECTION:
    case mojom::AccessibilityActionType::EXPAND:
    case mojom::AccessibilityActionType::COLLAPSE:
    case mojom::AccessibilityActionType::DISMISS:
    case mojom::AccessibilityActionType::SET_TEXT:
    case mojom::AccessibilityActionType::CONTEXT_CLICK:
    case mojom::AccessibilityActionType::SCROLL_DOWN:
    case mojom::AccessibilityActionType::SCROLL_LEFT:
    case mojom::AccessibilityActionType::SCROLL_RIGHT:
    case mojom::AccessibilityActionType::SCROLL_UP:
    case mojom::AccessibilityActionType::SET_PROGRESS:
    case mojom::AccessibilityActionType::SHOW_ON_SCREEN:
    case mojom::AccessibilityActionType::GET_TEXT_LOCATION:
    case mojom::AccessibilityActionType::SHOW_TOOLTIP:
    case mojom::AccessibilityActionType::HIDE_TOOLTIP:
      break;
  }
}

bool ArcAccessibilityHelperBridge::UseFullFocusMode() const {
  return use_full_focus_mode_;
}

void ArcAccessibilityHelperBridge::OnNotificationSurfaceAdded(
    ash::ArcNotificationSurface* surface) {
  tree_tracker_.OnNotificationSurfaceAdded(surface);
}

void ArcAccessibilityHelperBridge::AllAutomationExtensionsGone() {
  // Extension features are directly monitored, so no work needed here.
}

void ArcAccessibilityHelperBridge::ExtensionListenerAdded() {
  tree_tracker_.InvalidateTrees();
}

extensions::EventRouter* ArcAccessibilityHelperBridge::GetEventRouter() const {
  return extensions::EventRouter::Get(profile_);
}

arc::mojom::AccessibilityFilterType
ArcAccessibilityHelperBridge::GetFilterType() {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  const MagnificationManager* magnification_manager =
      MagnificationManager::Get();

  if (!accessibility_manager || !magnification_manager)
    return arc::mojom::AccessibilityFilterType::OFF;

  // TODO(yawano): Support the case where primary user is in background.
  if (accessibility_manager->profile() != profile_)
    return arc::mojom::AccessibilityFilterType::OFF;

  if (accessibility_manager->IsSelectToSpeakEnabled() ||
      accessibility_manager->IsSwitchAccessEnabled() ||
      accessibility_manager->IsSpokenFeedbackEnabled() ||
      magnification_manager->IsMagnifierEnabled() ||
      magnification_manager->IsDockedMagnifierEnabled()) {
    return arc::mojom::AccessibilityFilterType::ALL;
  }

  if (accessibility_manager->IsFocusHighlightEnabled()) {
    return arc::mojom::AccessibilityFilterType::FOCUS;
  }

  return arc::mojom::AccessibilityFilterType::OFF;
}

void ArcAccessibilityHelperBridge::UpdateCaptionSettings() const {
  arc::mojom::CaptionStylePtr caption_style =
      GetCaptionStyleFromPrefs(profile_->GetPrefs());

  if (!caption_style)
    return;

  accessibility_helper_instance_.SetCaptionStyle(std::move(caption_style));
}

void ArcAccessibilityHelperBridge::OnActionResult(const ui::AXActionData& data,
                                                  bool result) const {
  AXTreeSourceArc* tree_source =
      tree_tracker_.GetFromTreeId(data.target_tree_id);

  if (!tree_source)
    return;

  tree_source->NotifyActionResult(data, result);
}

void ArcAccessibilityHelperBridge::OnGetTextLocationDataResult(
    const ui::AXActionData& data,
    const absl::optional<gfx::Rect>& result_rect) const {
  AXTreeSourceArc* tree_source =
      tree_tracker_.GetFromTreeId(data.target_tree_id);

  if (!tree_source)
    return;

  tree_source->NotifyGetTextLocationDataResult(
      data,
      OnGetTextLocationDataResultInternal(data.target_tree_id, result_rect));
}

absl::optional<gfx::Rect>
ArcAccessibilityHelperBridge::OnGetTextLocationDataResultInternal(
    const ui::AXTreeID& ax_tree_id,
    const absl::optional<gfx::Rect>& result_rect) const {
  if (!result_rect)
    return absl::nullopt;

  AXTreeSourceArc* tree_source = tree_tracker_.GetFromTreeId(ax_tree_id);
  if (!tree_source)
    return absl::nullopt;

  aura::Window* window = tree_source->window();
  if (!window)
    return absl::nullopt;

  const gfx::RectF& rect_f =
      ScaleAndroidPxToChromePx(result_rect.value(), window);

  return gfx::ToEnclosingRect(rect_f);
}

void ArcAccessibilityHelperBridge::OnAccessibilityStatusChanged(
    const ash::AccessibilityStatusEventDetails& event_details) {
  if (event_details.notification_type !=
          AccessibilityNotificationType::kToggleFocusHighlight &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleSelectToSpeak &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleSpokenFeedback &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleSwitchAccess &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleDockedMagnifier &&
      event_details.notification_type !=
          AccessibilityNotificationType::kToggleScreenMagnifier) {
    return;
  }

  UpdateEnabledFeature();

  if (event_details.notification_type ==
      AccessibilityNotificationType::kToggleSpokenFeedback) {
    accessibility_helper_instance_.SetExploreByTouchEnabled(
        event_details.enabled);
  }
}

void ArcAccessibilityHelperBridge::UpdateEnabledFeature() {
  filter_type_ = GetFilterType();

  // Let Android know the filter type change.
  accessibility_helper_instance_.SetFilter(filter_type_);

  const AccessibilityManager* accessibility_manager =
      AccessibilityManager::Get();
  if (accessibility_manager) {
    is_focus_event_enabled_ = accessibility_manager->IsFocusHighlightEnabled();

    use_full_focus_mode_ = accessibility_manager->IsSwitchAccessEnabled() ||
                           accessibility_manager->IsSpokenFeedbackEnabled();
  }

  tree_tracker_.OnEnabledFeatureChanged(filter_type_);
}

void ArcAccessibilityHelperBridge::HandleFilterTypeFocusEvent(
    mojom::AccessibilityEventDataPtr event_data) {
  if (event_data.get()->node_data.size() == 1 &&
      event_data->event_type ==
          arc::mojom::AccessibilityEventType::VIEW_FOCUSED)
    DispatchFocusChange(event_data.get()->node_data[0].get(), profile_);
}

void ArcAccessibilityHelperBridge::HandleFilterTypeAllEvent(
    mojom::AccessibilityEventDataPtr event_data) {
  if (ShouldAnnounceEvent(event_data.get())) {
    DispatchEventTextAnnouncement(event_data.get());
    return;
  }

  if (event_data->node_data.empty())
    return;

  AXTreeSourceArc* tree_source =
      tree_tracker_.OnAccessibilityEvent(event_data.get());
  if (!tree_source)
    return;

  tree_source->NotifyAccessibilityEvent(event_data.get());

  bool is_notification_event = event_data->notification_key.has_value();
  if (is_notification_event &&
      event_data->event_type ==
          arc::mojom::AccessibilityEventType::VIEW_TEXT_SELECTION_CHANGED) {
    // If text selection changed event is dispatched from Android, it
    // means that user is trying to type a text in Android notification.
    // Dispatch text selection changed event to notification content view
    // as the view can take necessary actions, e.g. activate itself, etc.
    auto* surface_manager = ash::ArcNotificationSurfaceManager::Get();
    if (surface_manager) {
      ash::ArcNotificationSurface* surface =
          surface_manager->GetArcSurface(event_data->notification_key.value());
      if (surface) {
        surface->GetAttachedHost()->NotifyAccessibilityEvent(
            ax::mojom::Event::kTextSelectionChanged, true);
      }
    }
  }

  if (is_focus_event_enabled_ &&
      event_data->event_type ==
          arc::mojom::AccessibilityEventType::VIEW_FOCUSED) {
    for (size_t i = 0; i < event_data->node_data.size(); ++i) {
      if (event_data->node_data[i]->id == event_data->source_id) {
        DispatchFocusChange(event_data->node_data[i].get(), profile_);
        break;
      }
    }
  }
}

void ArcAccessibilityHelperBridge::DispatchEventTextAnnouncement(
    mojom::AccessibilityEventData* event_data) const {
  if (!event_data->event_text.has_value())
    return;

  auto event_args(
      extensions::api::accessibility_private::OnAnnounceForAccessibility::
          Create(*(event_data->event_text)));
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_ANNOUNCE_FOR_ACCESSIBILITY,
      extensions::api::accessibility_private::OnAnnounceForAccessibility::
          kEventName,
      std::move(event_args)));
  GetEventRouter()->BroadcastEvent(std::move(event));
}

// static
void ArcAccessibilityHelperBridge::EnsureFactoryBuilt() {
  ArcAccessibilityHelperBridgeFactory::GetInstance();
}

}  // namespace arc
