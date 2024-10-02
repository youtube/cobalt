// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_detailed_view.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "ash/system/audio/mic_gain_slider_controller.h"
#include "ash/system/audio/mic_gain_slider_view.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/audio/unified_volume_view.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/quick_settings_slider.h"
#include "ash/system/unified/unified_slider_view.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

const int kLabelFontSizeDelta = 1;
const int kToggleButtonRowViewSpacing = 18;
constexpr auto kLiveCaptionContainerMargins = gfx::Insets::TLBR(0, 0, 8, 0);
constexpr auto kToggleButtonRowLabelPadding = gfx::Insets::TLBR(16, 0, 15, 0);
constexpr auto kToggleButtonRowViewPadding = gfx::Insets::TLBR(0, 56, 8, 0);
constexpr auto kQsToggleButtonRowViewPadding = gfx::Insets::VH(0, 32);
constexpr auto kQsToggleButtonRowPreferredSize = gfx::Size(0, 32);
constexpr auto kQsToggleButtonRowLabelPadding = gfx::Insets::VH(8, 12);
constexpr auto kQsToggleButtonRowMargins = gfx::Insets::VH(4, 0);
constexpr auto kSeparatorMargins = gfx::Insets::TLBR(4, 32, 12, 32);
constexpr auto kTextRowInsets = gfx::Insets::VH(8, 24);
constexpr auto kDevicesNameViewPreferredSize = gfx::Size(0, 44);
constexpr auto kDevicesTriViewInsets = gfx::Insets::TLBR(0, 24, 0, 32);
constexpr auto kDevicesTriViewBorder = gfx::Insets::VH(0, 4);
constexpr auto kQsSubsectionMargins = gfx::Insets::TLBR(0, 0, 4, 0);

// This callback is only used for tests.
AudioDetailedView::NoiseCancellationCallback*
    g_noise_cancellation_toggle_callback = nullptr;

std::u16string GetAudioDeviceName(const AudioDevice& device) {
  switch (device.type) {
    case AudioDeviceType::kFrontMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_FRONT_MIC);
    case AudioDeviceType::kHeadphone:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_HEADPHONE);
    case AudioDeviceType::kInternalSpeaker:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_SPEAKER);
    case AudioDeviceType::kInternalMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_MIC);
    case AudioDeviceType::kRearMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_REAR_MIC);
    case AudioDeviceType::kUsb:
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_AUDIO_USB_DEVICE,
                                        base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kBluetooth:
      [[fallthrough]];
    case AudioDeviceType::kBluetoothNbMic:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_BLUETOOTH_DEVICE,
          base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kHdmi:
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_AUDIO_HDMI_DEVICE,
                                        base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kMic:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_MIC_JACK_DEVICE);
    default:
      return base::UTF8ToUTF16(device.display_name);
  }
}

speech::LanguageCode GetLiveCaptionLocale() {
  std::string live_caption_locale = speech::kUsEnglishLocale;
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (pref_service) {
    live_caption_locale = ::prefs::GetLiveCaptionLanguageCode(pref_service);
  }
  return speech::GetLanguageCode(live_caption_locale);
}

// The highlight path generator for the `device_name_container`. We need to
// customize the shape of the focus ring because we want the focus ring to
// encompass the inactive radio slider but let the `device_name_container`
// handle the events.
class DeviceNameContainerHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  explicit DeviceNameContainerHighlightPathGenerator(
      QuickSettingsSlider* slider)
      : slider_(slider) {}
  DeviceNameContainerHighlightPathGenerator(
      const DeviceNameContainerHighlightPathGenerator&) = delete;
  DeviceNameContainerHighlightPathGenerator& operator=(
      const DeviceNameContainerHighlightPathGenerator&) = delete;

 private:
  // HighlightPathGenerator:
  absl::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    gfx::Rect slider_bounds = slider_->GetInactiveRadioSliderRect();
    gfx::RectF bounds(slider_bounds.x() + kRadioSliderViewPadding.left(),
                      slider_bounds.y(), slider_bounds.width(),
                      slider_bounds.height());
    gfx::RoundedCornersF rounded(
        slider_->GetInactiveRadioSliderRoundedCornerRadius());
    return gfx::RRectF(bounds, rounded);
  }

  // Owned by views hierarchy.
  const raw_ptr<QuickSettingsSlider, ExperimentalAsh> slider_;
};

}  // namespace

AudioDetailedView::AudioDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  CreateItems();

  Shell::Get()->accessibility_controller()->AddObserver(this);

  if (!captions::IsLiveCaptionFeatureSupported()) {
    return;
  }
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (soda_installer) {
    soda_installer->AddObserver(this);
  }
}

AudioDetailedView::~AudioDetailedView() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  if (!captions::IsLiveCaptionFeatureSupported()) {
    return;
  }
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  // `soda_installer` is not guaranteed to be valid, since it's possible for
  // this class to out-live it. This means that this class cannot use
  // ScopedObservation and needs to manage removing the observer itself.
  if (soda_installer) {
    soda_installer->RemoveObserver(this);
  }
}

views::View* AudioDetailedView::GetAsView() {
  return this;
}

void AudioDetailedView::SetMapNoiseCancellationToggleCallbackForTest(
    AudioDetailedView::NoiseCancellationCallback*
        noise_cancellation_toggle_callback) {
  g_noise_cancellation_toggle_callback = noise_cancellation_toggle_callback;
}

void AudioDetailedView::Update() {
  UpdateAudioDevices();
  Layout();
}

void AudioDetailedView::OnAccessibilityStatusChanged() {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  if (features::IsQsRevampEnabled()) {
    // The live caption state has been updated.
    UpdateLiveCaptionView(controller->live_caption().enabled());
  } else if (live_caption_view_ &&
             controller->IsLiveCaptionSettingVisibleInTray()) {
    TrayPopupUtils::UpdateCheckMarkVisibility(
        live_caption_view_, controller->live_caption().enabled());
  }
}

void AudioDetailedView::AddAudioSubHeader(views::View* container,
                                          const gfx::VectorIcon& icon,
                                          const int text_id) {
  if (!features::IsQsRevampEnabled()) {
    TriView* header = AddScrollListSubHeader(container, icon, text_id);
    header->SetContainerVisible(TriView::Container::END, /*visible=*/false);
    return;
  }

  auto* sub_header_label_ = TrayPopupUtils::CreateDefaultLabel();
  sub_header_label_->SetText(l10n_util::GetStringUTF16(text_id));
  sub_header_label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  TrayPopupUtils::SetLabelFontList(sub_header_label_,
                                   TrayPopupUtils::FontStyle::kSubHeader);
  if (chromeos::features::IsJellyEnabled()) {
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                          *sub_header_label_);
  }
  sub_header_label_->SetBorder(views::CreateEmptyBorder(kTextRowInsets));
  container->AddChildView(sub_header_label_);
  return;
}

views::View* AudioDetailedView::AddDeviceSlider(
    views::View* container,
    AudioDevice const& device,
    HoverHighlightView* device_name_container,
    bool is_output_device) {
  device_name_container->SetPreferredSize(kDevicesNameViewPreferredSize);
  device_name_container->tri_view()->SetInsets(kDevicesTriViewInsets);
  device_name_container->tri_view()->SetContainerBorder(
      TriView::Container::CENTER,
      views::CreateEmptyBorder(kDevicesTriViewBorder));
  // TODO(b/262281693): Update the font for `device_name_container` text label.
  device_name_container->text_label()->SetEnabledColorId(
      device.active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                    : cros_tokens::kCrosSysOnSurfaceVariant);
  device_name_container->SetPaintToLayer();
  // If this device is the active one, disables event handling on
  // `device_name_container` so that `slider` can handle the events.
  if (device.active) {
    device_name_container->SetFocusBehavior(FocusBehavior::NEVER);
    device_name_container->SetCanProcessEventsWithinSubtree(false);
  }

  std::unique_ptr<views::View> device_container =
      std::make_unique<views::View>();

  UnifiedSliderView* unified_slider_view =
      is_output_device
          ? static_cast<UnifiedSliderView*>(device_container->AddChildView(
                unified_volume_slider_controller_->CreateVolumeSlider(
                    device.id)))
          : static_cast<UnifiedSliderView*>(device_container->AddChildView(
                mic_gain_controller_->CreateMicGainSlider(
                    device.id, device.IsInternalMic())));

  if (!device.active) {
    // Installs the customized focus ring path generator for
    // `device_name_container`.
    device_name_container->SetInstallFocusRingOnFocus(true);
    views::FocusRing::Get(device_name_container)
        ->SetPathGenerator(
            std::make_unique<DeviceNameContainerHighlightPathGenerator>(
                /*slider=*/static_cast<QuickSettingsSlider*>(
                    unified_slider_view->slider())));
    device_name_container->SetFocusPainter(nullptr);
    views::FocusRing::Get(device_name_container)
        ->SetColorId(cros_tokens::kCrosSysPrimary);
  }
  // Puts `slider` beneath `device_name_container`.
  device_name_container->AddLayerToRegion(unified_slider_view->layer(),
                                          views::LayerRegion::kBelow);
  device_container->AddChildView(device_name_container);
  device_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  auto* added_device = container->AddChildView(std::move(device_container));

  // If the `device_name_container` of this device is previously focused and
  // then becomes active, the slider of this device should preserve the focus.
  if (focused_device_id_ == device.id && device.active) {
    unified_slider_view->slider()->RequestFocus();
    // Resets the id.
    focused_device_id_ = -1;
  }
  return added_device;
}

void AudioDetailedView::CreateItems() {
  CreateScrollableList();
  if (features::IsQsRevampEnabled()) {
    CreateTitleRow(IDS_ASH_STATUS_TRAY_AUDIO_QS_REVAMP);
    // `live_caption_view_` will always shows up in the revamped
    // `AudioDetailedView`.
    CreateLiveCaptionView();
  } else {
    CreateTitleRow(IDS_ASH_STATUS_TRAY_AUDIO);
  }

  mic_gain_controller_ = std::make_unique<MicGainSliderController>();
  unified_volume_slider_controller_ =
      std::make_unique<UnifiedVolumeSliderController>();
}

void AudioDetailedView::CreateLiveCaptionView() {
  auto* live_caption_container =
      scroll_content()->AddChildViewAt(std::make_unique<RoundedContainer>(), 0);
  live_caption_container->SetProperty(views::kMarginsKey,
                                      kLiveCaptionContainerMargins);
  // Ensures the `HoverHighlightView` ink drop fills the whole container.
  live_caption_container->SetBorderInsets(gfx::Insets());

  live_caption_view_ = live_caption_container->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  live_caption_view_->SetFocusBehavior(FocusBehavior::NEVER);

  // Creates the icon and text for the `live_caption_view_`.
  const bool live_caption_enabled =
      Shell::Get()->accessibility_controller()->live_caption().enabled();
  auto toggle_icon = std::make_unique<views::ImageView>();
  toggle_icon->SetImage(ui::ImageModel::FromVectorIcon(
      live_caption_enabled ? kUnifiedMenuLiveCaptionIcon
                           : kUnifiedMenuLiveCaptionOffIcon,
      cros_tokens::kCrosSysOnSurface, kQsSliderIconSize));
  live_caption_icon_ = toggle_icon.get();
  // TODO(b/262281693): Update the font and color for `live_caption_view_` text.
  live_caption_view_->AddViewAndLabel(
      std::move(toggle_icon),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION));
  live_caption_view_->text_label()->SetEnabledColorId(
      cros_tokens::kCrosSysOnSurface);

  // Creates a toggle button on the right.
  auto toggle = std::make_unique<Switch>(base::BindRepeating(
      &AudioDetailedView::ToggleLiveCaptionState, weak_factory_.GetWeakPtr()));
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION));
  toggle->SetIsOn(live_caption_enabled);
  std::u16string toggle_tooltip =
      live_caption_enabled
          ? l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_LIVE_CAPTION_ENABLED_STATE_TOOLTIP)
          : l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_LIVE_CAPTION_DISABLED_STATE_TOOLTIP);
  toggle->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_LIVE_CAPTION_TOGGLE_TOOLTIP, toggle_tooltip));
  live_caption_button_ = toggle.get();
  live_caption_view_->AddRightView(toggle.release());

  // Allows the row to be taller than a typical tray menu item.
  live_caption_view_->SetExpandable(true);
  live_caption_view_->tri_view()->SetInsets(kTextRowInsets);
}

std::unique_ptr<views::View>
AudioDetailedView::CreateNoiseCancellationToggleRow(const AudioDevice& device) {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  auto noise_cancellation_toggle = std::make_unique<TrayToggleButton>(
      base::BindRepeating(
          &AudioDetailedView::OnInputNoiseCancellationTogglePressed,
          base::Unretained(this)),
      IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION);

  noise_cancellation_toggle->SetIsOn(
      audio_handler->GetNoiseCancellationState());

  auto noise_cancellation_toggle_row = std::make_unique<views::View>();

  auto* row_layout = noise_cancellation_toggle_row->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kToggleButtonRowViewPadding, kToggleButtonRowViewSpacing));

  noise_cancellation_toggle->SetFlipCanvasOnPaintForRTLUI(false);

  auto noise_cancellation_label =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION));

  noise_cancellation_label->SetEnabledColorId(kColorAshTextColorPrimary);
  noise_cancellation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  noise_cancellation_label->SetFontList(
      gfx::FontList().DeriveWithSizeDelta(kLabelFontSizeDelta));
  noise_cancellation_label->SetAutoColorReadabilityEnabled(false);
  noise_cancellation_label->SetSubpixelRenderingEnabled(false);
  noise_cancellation_label->SetBorder(
      views::CreateEmptyBorder(kToggleButtonRowLabelPadding));

  auto* label_ptr = noise_cancellation_toggle_row->AddChildView(
      std::move(noise_cancellation_label));
  row_layout->SetFlexForView(label_ptr, 1);

  noise_cancellation_toggle_row->AddChildView(
      std::move(noise_cancellation_toggle));

  // This is only used for testing.
  if (g_noise_cancellation_toggle_callback) {
    g_noise_cancellation_toggle_callback->Run(
        device.id, noise_cancellation_toggle_row.get());
  }

  return noise_cancellation_toggle_row;
}

std::unique_ptr<HoverHighlightView>
AudioDetailedView::CreateQsNoiseCancellationToggleRow(
    const AudioDevice& device) {
  bool noise_cancellation_state =
      CrasAudioHandler::Get()->GetNoiseCancellationState();

  auto noise_cancellation_view =
      std::make_unique<HoverHighlightView>(/*listener=*/this);

  auto toggle_icon =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          noise_cancellation_state ? kUnifiedMenuMicNoiseCancelHighIcon
                                   : kUnifiedMenuMicNoiseCancelOffIcon,
          cros_tokens::kCrosSysOnSurface, kQsSliderIconSize));
  noise_cancellation_icon_ = toggle_icon.get();

  noise_cancellation_view->AddViewAndLabel(
      std::move(toggle_icon),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION));
  if (chromeos::features::IsJellyEnabled()) {
    views::Label* noise_cancellation_label =
        noise_cancellation_view->text_label();
    noise_cancellation_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *noise_cancellation_label);
  }

  // Create a non-clickable non-focusable toggle button on the right. The events
  // and focus behavior should be handled by `noise_cancellation_view_` instead.
  auto toggle = std::make_unique<Switch>();
  toggle->SetIsOn(noise_cancellation_state);
  toggle->SetCanProcessEventsWithinSubtree(false);
  toggle->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  // Ignore the toggle for accessibility.
  auto& view_accessibility = toggle->GetViewAccessibility();
  view_accessibility.OverrideIsLeaf(true);
  view_accessibility.OverrideIsIgnored(true);
  noise_cancellation_button_ = toggle.get();
  noise_cancellation_view->AddRightView(toggle.release());

  noise_cancellation_view->tri_view()->SetInsets(kQsToggleButtonRowViewPadding);
  noise_cancellation_view->tri_view()->SetContainerLayout(
      TriView::Container::CENTER, std::make_unique<views::BoxLayout>(
                                      views::BoxLayout::Orientation::kVertical,
                                      kQsToggleButtonRowLabelPadding));
  noise_cancellation_view->SetPreferredSize(kQsToggleButtonRowPreferredSize);
  noise_cancellation_view->SetProperty(views::kMarginsKey,
                                       kQsToggleButtonRowMargins);
  noise_cancellation_view->SetAccessibilityState(
      noise_cancellation_state
          ? HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX
          : HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);

  // This is only used for testing.
  if (g_noise_cancellation_toggle_callback) {
    g_noise_cancellation_toggle_callback->Run(device.id,
                                              noise_cancellation_view.get());
  }

  return noise_cancellation_view;
}

void AudioDetailedView::MaybeShowSodaMessage(speech::LanguageCode language_code,
                                             std::u16string message) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  const bool is_live_caption_enabled = controller->live_caption().enabled();
  const bool is_live_caption_in_tray =
      live_caption_view_ && controller->IsLiveCaptionSettingVisibleInTray();
  // Only show updates for this feature if the language code applies to the SODA
  // binary (encoded by `LanguageCode::kNone`) or the language pack matching
  // the feature locale.
  const bool live_caption_has_update =
      language_code == speech::LanguageCode::kNone ||
      language_code == GetLiveCaptionLocale();

  if (live_caption_has_update && is_live_caption_enabled &&
      (features::IsQsRevampEnabled() || is_live_caption_in_tray)) {
    live_caption_view_->SetSubText(message);
  }
}

void AudioDetailedView::OnInputNoiseCancellationTogglePressed() {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  const bool new_state = !audio_handler->GetNoiseCancellationState();
  audio_handler->SetNoiseCancellationState(
      new_state, CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);
  if (features::IsQsRevampEnabled()) {
    noise_cancellation_icon_->SetImage(ui::ImageModel::FromVectorIcon(
        new_state ? kUnifiedMenuMicNoiseCancelHighIcon
                  : kUnifiedMenuMicNoiseCancelOffIcon,
        cros_tokens::kCrosSysOnSurface, kQsSliderIconSize));
    noise_cancellation_button_->SetIsOn(new_state);
  }
}

void AudioDetailedView::OnSettingsButtonClicked() {
  DCHECK(features::IsAudioSettingsPageEnabled());
  if (!TrayPopupUtils::CanOpenWebUISettings()) {
    return;
  }

  CloseBubble();  // Deletes |this|.
  Shell::Get()->system_tray_model()->client()->ShowAudioSettings();
}

void AudioDetailedView::ToggleLiveCaptionState() {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Updates the enable state for live caption.
  controller->live_caption().SetEnabled(!controller->live_caption().enabled());
}

void AudioDetailedView::UpdateLiveCaptionView(bool is_enabled) {
  live_caption_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      is_enabled ? kUnifiedMenuLiveCaptionIcon : kUnifiedMenuLiveCaptionOffIcon,
      cros_tokens::kCrosSysOnSurface, kQsSliderIconSize));

  // Updates the toggle button tooltip.
  std::u16string toggle_tooltip =
      is_enabled ? l10n_util::GetStringUTF16(
                       IDS_ASH_STATUS_TRAY_LIVE_CAPTION_ENABLED_STATE_TOOLTIP)
                 : l10n_util::GetStringUTF16(
                       IDS_ASH_STATUS_TRAY_LIVE_CAPTION_DISABLED_STATE_TOOLTIP);
  live_caption_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_LIVE_CAPTION_TOGGLE_TOOLTIP, toggle_tooltip));

  // Ensures the toggle button is in sync with the current Live Caption state.
  if (live_caption_button_->GetIsOn() != is_enabled) {
    live_caption_button_->SetIsOn(is_enabled);
  }

  InvalidateLayout();
}

void AudioDetailedView::UpdateAudioDevices() {
  output_devices_.clear();
  input_devices_.clear();
  AudioDeviceList devices;
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  audio_handler->GetAudioDevices(&devices);
  const bool has_dual_internal_mic = audio_handler->HasDualInternalMic();
  bool is_front_or_rear_mic_active = false;
  for (const auto& device : devices) {
    // Only display devices if they are for simple usage.
    if (!device.is_for_simple_usage()) {
      continue;
    }
    if (device.is_input) {
      // Do not expose the internal front and rear mic to UI.
      if (has_dual_internal_mic && audio_handler->IsFrontOrRearMic(device)) {
        if (device.active) {
          is_front_or_rear_mic_active = true;
        }
        continue;
      }
      input_devices_.push_back(device);
    } else {
      output_devices_.push_back(device);
    }
  }

  // Expose the dual internal mics as one device (internal mic) to user.
  if (has_dual_internal_mic) {
    // Create stub internal mic entry for UI rendering, which representing
    // both internal front and rear mics.
    AudioDevice internal_mic;
    internal_mic.is_input = true;
    // `stable_device_id_version` is used to differentiate `stable_device_id`
    // for backward compatibility. Version 2 means `deprecated_stable_device_id`
    // will contain deprecated, v1 stable device id version.
    internal_mic.stable_device_id_version = 2;
    internal_mic.type = AudioDeviceType::kInternalMic;
    internal_mic.active = is_front_or_rear_mic_active;
    input_devices_.push_back(internal_mic);
  }

  UpdateScrollableList();
}

void AudioDetailedView::UpdateScrollableList() {
  scroll_content()->RemoveAllChildViews();
  device_map_.clear();

  // Uses the `RoundedContainer` for QsRevamp.
  views::View* container = scroll_content();
  if (features::IsQsRevampEnabled()) {
    container =
        scroll_content()->AddChildView(std::make_unique<RoundedContainer>());
  }

  // Adds the live caption toggle.
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  if (features::IsQsRevampEnabled()) {
    CreateLiveCaptionView();
  } else if (controller->IsLiveCaptionSettingVisibleInTray()) {
    live_caption_view_ = AddScrollListCheckableItem(
        container, vector_icons::kLiveCaptionOnIcon,
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION),
        controller->live_caption().enabled(),
        controller->IsEnterpriseIconVisibleForLiveCaption());
    container->AddChildView(TrayPopupUtils::CreateListSubHeaderSeparator());
  }

  // Adds audio output devices.
  const bool has_output_devices = output_devices_.size() > 0;
  if (has_output_devices) {
    AddAudioSubHeader(container, kSystemMenuAudioOutputIcon,
                      IDS_ASH_STATUS_TRAY_AUDIO_OUTPUT);
  }

  views::View* last_output_device = nullptr;
  for (const auto& device : output_devices_) {
    HoverHighlightView* device_name_container = AddScrollListCheckableItem(
        container, gfx::kNoneIcon, GetAudioDeviceName(device), device.active);
    device_map_[device_name_container] = device;

    if (features::IsQsRevampEnabled()) {
      last_output_device =
          AddDeviceSlider(container, device, device_name_container,
                          /*is_output_device=*/true);
    }
  }

  if (has_output_devices) {
    if (features::IsQsRevampEnabled()) {
      last_output_device->SetProperty(views::kMarginsKey, kQsSubsectionMargins);
    } else {
      container->AddChildView(TrayPopupUtils::CreateListSubHeaderSeparator());
    }
  }

  // Adds audio input devices.
  const bool has_input_devices = input_devices_.size() > 0;
  if (has_input_devices) {
    AddAudioSubHeader(container, kSystemMenuAudioInputIcon,
                      IDS_ASH_STATUS_TRAY_AUDIO_INPUT);
  }

  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();

  for (const auto& device : input_devices_) {
    HoverHighlightView* device_name_container = AddScrollListCheckableItem(
        container, gfx::kNoneIcon, GetAudioDeviceName(device), device.active);
    device_map_[device_name_container] = device;

    if (features::IsQsRevampEnabled()) {
      AddDeviceSlider(container, device, device_name_container,
                      /*is_output_device=*/false);
    }

    // Adds the input noise cancellation toggle.
    if (audio_handler->GetPrimaryActiveInputNode() == device.id &&
        audio_handler->IsNoiseCancellationSupportedForDevice(device.id)) {
      if (features::IsQsRevampEnabled()) {
        noise_cancellation_view_ = container->AddChildView(
            AudioDetailedView::CreateQsNoiseCancellationToggleRow(device));

        // Adds a `Separator` if this input device is not the last one.
        if (&device != &input_devices_.back()) {
          auto* separator =
              container->AddChildView(std::make_unique<views::Separator>());
          separator->SetColorId(cros_tokens::kCrosSysSeparator);
          separator->SetOrientation(views::Separator::Orientation::kHorizontal);
          separator->SetProperty(views::kMarginsKey, kSeparatorMargins);
        }
      } else {
        container->AddChildView(
            AudioDetailedView::CreateNoiseCancellationToggleRow(device));
      }
    }

    if (!features::IsQsRevampEnabled()) {
      scroll_content()->AddChildView(mic_gain_controller_->CreateMicGainSlider(
          device.id, device.IsInternalMic()));
    }
  }

  container->SizeToPreferredSize();
  scroller()->Layout();
}

void AudioDetailedView::HandleViewClicked(views::View* view) {
  if (live_caption_view_ && view == live_caption_view_) {
    ToggleLiveCaptionState();
    return;
  }

  if (noise_cancellation_view_ && view == noise_cancellation_view_) {
    OnInputNoiseCancellationTogglePressed();
    return;
  }

  AudioDeviceMap::iterator iter = device_map_.find(view);
  if (iter == device_map_.end()) {
    return;
  }
  AudioDevice device = iter->second;

  // If the clicked view is focused, save the id of this device to preserve the
  // focus ring.
  if (view->HasFocus()) {
    focused_device_id_ = device.id;
  }

  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  if (device.type == AudioDeviceType::kInternalMic &&
      audio_handler->HasDualInternalMic()) {
    audio_handler->SwitchToFrontOrRearMic();
  } else {
    audio_handler->SwitchToDevice(device, true,
                                  CrasAudioHandler::ACTIVATE_BY_USER);
  }
}

void AudioDetailedView::CreateExtraTitleRowButtons() {
  if (features::IsAudioSettingsPageEnabled()) {
    tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);
    std::unique_ptr<views::Button> settings =
        base::WrapUnique(CreateSettingsButton(
            base::BindRepeating(&AudioDetailedView::OnSettingsButtonClicked,
                                weak_factory_.GetWeakPtr()),
            IDS_ASH_STATUS_TRAY_AUDIO_SETTINGS));
    settings_button_ =
        tri_view()->AddView(TriView::Container::END, std::move(settings));
  }
}

// SodaInstaller::Observer:
void AudioDetailedView::OnSodaInstalled(speech::LanguageCode language_code) {
  std::u16string message = l10n_util::GetStringUTF16(
      IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_COMPLETE);
  MaybeShowSodaMessage(language_code, message);
}

void AudioDetailedView::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  std::u16string error_message;
  switch (error_code) {
    case speech::SodaInstaller::ErrorCode::kUnspecifiedError: {
      error_message = l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_ERROR);
      break;
    }
    case speech::SodaInstaller::ErrorCode::kNeedsReboot: {
      error_message = l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_ERROR_REBOOT_REQUIRED);
      break;
    }
  }

  MaybeShowSodaMessage(language_code, error_message);
}

void AudioDetailedView::OnSodaProgress(speech::LanguageCode language_code,
                                       int progress) {
  std::u16string message = l10n_util::GetStringFUTF16Int(
      IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_PROGRESS, progress);
  MaybeShowSodaMessage(language_code, message);
}

BEGIN_METADATA(AudioDetailedView, views::View)
END_METADATA

}  // namespace ash
