// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/caption_bubble.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/language/core/common/language_util.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_settings.h"
#include "components/live_caption/views/format_constants.h"
#include "components/live_caption/views/translation_view_wrapper_base.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_ui_languages_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_mode_observer.h"
#include "ui/base/buildflags.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/md_text_button_with_down_arrow.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

// The CaptionBubbleLabel needs to be focusable in order for NVDA to enable
// document navigation. It is suspected that other screen readers on Windows and
// Linux will need this behavior, too. VoiceOver and ChromeVox do not need the
// label to be focusable.
#if BUILDFLAG(HAS_NATIVE_ACCESSIBILITY) && !BUILDFLAG(IS_MAC)
#define NEED_FOCUS_FOR_ACCESSIBILITY
#endif

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
#include "base/scoped_observation.h"
#include "ui/accessibility/platform/ax_platform.h"
#endif

namespace captions {
namespace {

constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(250);

std::unique_ptr<views::ImageButton> BuildImageButton(
    views::Button::PressedCallback callback,
    const int tooltip_text_id) {
  auto button = views::CreateVectorImageButton(std::move(callback));
  button->SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
  button->SizeToPreferredSize();
  views::InstallCircleHighlightPathGenerator(
      button.get(), gfx::Insets(kButtonCircleHighlightPaddingDip));
  return button;
}

// ui::CaptionStyle styles are CSS strings that can sometimes have !important.
// This method removes " !important" if it exists at the end of a CSS string.
std::string MaybeRemoveCSSImportant(std::string css_string) {
  RE2::Replace(&css_string, "\\s+!important", "");
  return css_string;
}

// Parses a CSS color string that is in the form rgba and has a non-zero alpha
// value into an SkColor and sets it to be the value of the passed-in SkColor
// address. Returns whether or not the operation was a success.
//
// `css_string` is the CSS color string passed in. It is in the form
//     rgba(#,#,#,#). r, g, and b are integers between 0 and 255, inclusive.
//     a is a double between 0.0 and 1.0. There may be whitespace in between the
//     commas.
// `sk_color` is the address of an SKColor. If the operation is a success, the
//     function will set sk_color's value to be the parsed SkColor. If the
//     operation is not a success, sk_color's value will not change.
//
// As of spring 2021, all OS's use rgba to define the caption style colors.
// However, the ui::CaptionStyle spec also allows for the use of any valid CSS
// color spec. This function will need to be revisited should ui::CaptionStyle
// colors use non-rgba to define their colors.
bool ParseNonTransparentRGBACSSColorString(
    std::string css_string,
    SkColor* sk_color,
    const ui::ColorProvider* color_provider) {
  std::string rgba = MaybeRemoveCSSImportant(css_string);
  if (rgba.empty())
    return false;
  uint16_t r, g, b;
  double a;
  bool match = RE2::FullMatch(
      rgba, "rgba\\((\\d+),\\s*(\\d+),\\s*(\\d+),\\s*(\\d+\\.?\\d*)\\)", &r, &g,
      &b, &a);
  // If the opacity is set to 0 (fully transparent), we ignore the user's
  // preferred style and use our default color.
  if (!match || a == 0)
    return false;
  uint16_t a_int = base::ClampRound<uint16_t>(a * 255);
#if BUILDFLAG(IS_MAC)
  // On Mac, any opacity lower than 90% leaves rendering artifacts which make
  // it appear like there is a layer of faint text beneath the actual text.
  // TODO(crbug.com/40177817): Fix the rendering issue and then remove this
  // workaround.
  a_int = std::max(static_cast<uint16_t>(SkColorGetA(color_provider->GetColor(
                       ui::kColorLiveCaptionBubbleBackgroundDefault))),
                   a_int);
#endif
  *sk_color = SkColorSetARGB(a_int, r, g, b);
  return match;
}

}  // namespace

// Helper class for observing mouse and key events from native window.
class CaptionBubbleEventObserver : public ui::EventObserver {
 public:
  explicit CaptionBubbleEventObserver(captions::CaptionBubble* caption_bubble,
                                      views::Widget* widget)
      : caption_bubble_(caption_bubble) {
    CHECK(widget);
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, widget->GetNativeWindow(),
        {ui::EventType::kMouseMoved, ui::EventType::kMouseExited,
         ui::EventType::kKeyPressed, ui::EventType::kKeyReleased});
  }

  CaptionBubbleEventObserver(const CaptionBubbleEventObserver&) = delete;
  CaptionBubbleEventObserver& operator=(const CaptionBubbleEventObserver&) =
      delete;
  ~CaptionBubbleEventObserver() override = default;

  void OnEvent(const ui::Event& event) override {
    if (event.IsKeyEvent()) {
      caption_bubble_->UpdateControlsVisibility(true);
      return;
    }

    // We check if the mouse is in bounds rather than strictly
    // checking mouse enter/exit events because of two reasons: 1. We get
    // mouse exit/enter events when the mouse moves between client and
    // non-client areas on Linux and Windows; 2. We get a mouse exit event when
    // a context menu is brought up, which might cause the caption bubble to be
    // stuck in the "in" state when some other window is on top of the caption
    // bubble.
    caption_bubble_->OnMouseEnteredOrExitedWindow(IsMouseInBounds());
  }

 private:
  bool IsMouseInBounds() {
    gfx::Point point = event_monitor_->GetLastMouseLocation();
    views::View::ConvertPointFromScreen(caption_bubble_, &point);

    return caption_bubble_->GetLocalBounds().Contains(point);
  }

  raw_ptr<captions::CaptionBubble> caption_bubble_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

#if BUILDFLAG(IS_CHROMEOS)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsCaptionBubbleKey, false)
#endif

#if BUILDFLAG(IS_WIN)
class MediaFoundationRendererErrorMessageView : public views::StyledLabel {
  METADATA_HEADER(MediaFoundationRendererErrorMessageView, views::StyledLabel)

 public:
  explicit MediaFoundationRendererErrorMessageView(
      CaptionBubble* caption_bubble)
      : caption_bubble_(caption_bubble) {}

  // views::View:
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override {
    switch (action_data.action) {
      case ax::mojom::Action::kDoDefault:
        caption_bubble_->OnContentSettingsLinkClicked();
        return true;
      default:
        break;
    }
    return views::StyledLabel::HandleAccessibleAction(action_data);
  }

 private:
  const raw_ptr<CaptionBubble> caption_bubble_;  // Not owned.
};

BEGIN_METADATA(MediaFoundationRendererErrorMessageView)
END_METADATA

#endif

// CaptionBubble implementation of BubbleFrameView. This class takes care
// of making the caption draggable.
class CaptionBubbleFrameView : public views::BubbleFrameView {
  METADATA_HEADER(CaptionBubbleFrameView, views::BubbleFrameView)

 public:
  explicit CaptionBubbleFrameView(
      std::vector<raw_ptr<views::View, VectorExperimental>> buttons)
      : views::BubbleFrameView(gfx::Insets(), gfx::Insets()),
        buttons_(buttons) {
    auto border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::FLOAT, views::BubbleBorder::DIALOG_SHADOW);
    border->set_rounded_corners(gfx::RoundedCornersF(kCornerRadiusDip));
    views::BubbleFrameView::SetBubbleBorder(std::move(border));
  }

  ~CaptionBubbleFrameView() override = default;
  CaptionBubbleFrameView(const CaptionBubbleFrameView&) = delete;
  CaptionBubbleFrameView& operator=(const CaptionBubbleFrameView&) = delete;

  // TODO(crbug.com/40119836): This does not work on Linux because the bubble is
  // not a top-level view, so it doesn't receive events. See crbug.com/1074054
  // for more about why it doesn't work.
  int NonClientHitTest(const gfx::Point& point) override {
    // Outside of the window bounds, do nothing.
    if (!bounds().Contains(point))
      return HTNOWHERE;

    // |point| is in coordinates relative to CaptionBubbleFrameView, i.e.
    // (0,0) is the upper left corner of this view. Convert it to screen
    // coordinates to see whether one of the buttons contains this point.
    // If it is, return HTCLIENT, so that the click is sent through to be
    // handled by CaptionBubble::BubblePressed().
    gfx::Point point_in_screen =
        GetBoundsInScreen().origin() + gfx::Vector2d(point.x(), point.y());
    for (views::View* button : buttons_) {
      if (button->GetBoundsInScreen().Contains(point_in_screen))
        return HTCLIENT;
    }

    // Ensure it's within the BubbleFrameView. This takes into account the
    // rounded corners and drop shadow of the BubbleBorder.
    int hit = views::BubbleFrameView::NonClientHitTest(point);

    // After BubbleFrameView::NonClientHitTest processes the bubble-specific
    // hits such as the rounded corners, it checks hits to the bubble's client
    // view. Any hits to ClientFrameView::NonClientHitTest return HTCLIENT or
    // HTNOWHERE. Override these to return HTCAPTION in order to make the
    // entire widget draggable.
    return (hit == HTCLIENT || hit == HTNOWHERE) ? HTCAPTION : hit;
  }

 private:
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons_;
};

BEGIN_METADATA(CaptionBubbleFrameView)
END_METADATA

class CaptionBubbleLabelAXModeObserver;

// CaptionBubble implementation of Label. This class takes care of setting up
// the accessible virtual views of the label in order to support braille
// accessibility. The CaptionBubbleLabel is a readonly document with a paragraph
// inside. Inside the paragraph are staticText nodes, one for each visual line
// in the rendered text of the label. These staticText nodes are shown on a
// braille display so that a braille user can read the caption text line by
// line.
class CaptionBubbleLabel : public views::Label {
  METADATA_HEADER(CaptionBubbleLabel, views::Label)

 public:
  CaptionBubbleLabel() {
    GetViewAccessibility().SetRole(ax::mojom::Role::kDocument);
    GetViewAccessibility().SetName(
        std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    GetViewAccessibility().SetReadOnly(true);
#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
    ax_mode_observer_ =
        std::make_unique<CaptionBubbleLabelAXModeObserver>(this);
    SetFocusBehaviorForAccessibility();
#endif
  }
  ~CaptionBubbleLabel() override = default;
  CaptionBubbleLabel(const CaptionBubbleLabel&) = delete;
  CaptionBubbleLabel& operator=(const CaptionBubbleLabel&) = delete;

  void SetMinimumHeight(int height) {
    if (minimum_height_ == height) {
      return;
    }

    minimum_height_ = height;
    PreferredSizeChanged();
  }

  void SetText(std::u16string_view text) override {
    views::Label::SetText(text);

    auto& ax_lines = GetViewAccessibility().virtual_children();
    if (text.empty() && !ax_lines.empty()) {
      GetViewAccessibility().RemoveAllVirtualChildViews();
      NotifyAccessibilityEventDeprecated(ax::mojom::Event::kChildrenChanged,
                                         true);
      return;
    }

    const size_t num_lines = GetRequiredLines();
    size_t start = 0;
    for (size_t i = 0; i < num_lines - 1; ++i) {
      size_t end = GetTextIndexOfLine(i + 1);
      std::u16string_view substring = text.substr(start, end - start);
      UpdateAXLine(substring, i, gfx::Range(start, end));
      start = end;
    }
    std::u16string_view substring = text.substr(start, text.size() - start);
    if (!substring.empty()) {
      UpdateAXLine(substring, num_lines - 1, gfx::Range(start, text.size()));
    }

    // Remove all ax_lines that don't have a corresponding line.
    size_t num_ax_lines = ax_lines.size();
    for (size_t i = num_lines; i < num_ax_lines; ++i) {
      GetViewAccessibility().RemoveVirtualChildView(ax_lines.back().get());
      NotifyAccessibilityEventDeprecated(ax::mojom::Event::kChildrenChanged,
                                         true);
    }
  }

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
  // The CaptionBubbleLabel needs to be focusable in order for NVDA to enable
  // document navigation. Making the CaptionBubbleLabel focusable means it gets
  // a tabstop, so it should only be focusable for screen reader users.
  void SetFocusBehaviorForAccessibility() {
    SetFocusBehavior(ui::AXPlatform::GetInstance().GetMode().has_mode(
                         ui::AXMode::kExtendedProperties)
                         ? FocusBehavior::ALWAYS
                         : FocusBehavior::NEVER);
  }
#endif

 private:
  void UpdateAXLine(std::u16string_view line_text,
                    const size_t line_index,
                    const gfx::Range& text_range) {
    auto& ax_lines = GetViewAccessibility().virtual_children();

    // Add a new virtual child for a new line of text.
    DCHECK(line_index <= ax_lines.size());
    if (line_index == ax_lines.size()) {
      auto ax_line = std::make_unique<views::AXVirtualView>();
      ax_line->SetRole(ax::mojom::Role::kStaticText);
      GetViewAccessibility().AddVirtualChildView(std::move(ax_line));
      NotifyAccessibilityEventDeprecated(ax::mojom::Event::kChildrenChanged,
                                         true);
    }

    // Set the virtual child's name as line text.
    if (ax_lines[line_index]->GetCachedName() != line_text) {
      ax_lines[line_index]->SetName(std::u16string(line_text));
      std::vector<gfx::Rect> bounds = GetSubstringBounds(text_range);
      ax_lines[line_index]->SetBounds(gfx::RectF(bounds[0]));
      ax_lines[line_index]->NotifyEvent(ax::mojom::Event::kTextChanged, true);
    }
  }

  // Label:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size preferred_size =
        views::Label::CalculatePreferredSize(available_size);
    preferred_size.set_height(
        std::max(minimum_height_, preferred_size.height()));
    return preferred_size;
  }

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
  std::unique_ptr<CaptionBubbleLabelAXModeObserver> ax_mode_observer_;
#endif
  int minimum_height_ = 0;
};

BEGIN_METADATA(CaptionBubbleLabel)
END_METADATA

#if defined(NEED_FOCUS_FOR_ACCESSIBILITY)
// A helper class to the CaptionBubbleLabel which observes AXMode changes and
// updates the CaptionBubbleLabel focus behavior in response.
// TODO(crbug.com/40756389): Implement a ui::AXModeObserver::OnAXModeRemoved
// method which observes the removal of AXModes. Without that, the caption
// bubble label will remain focusable once accessibility is enabled, even if
// accessibility is later disabled.
class CaptionBubbleLabelAXModeObserver : public ui::AXModeObserver {
 public:
  explicit CaptionBubbleLabelAXModeObserver(CaptionBubbleLabel* owner)
      : owner_(owner) {
    ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
  }

  ~CaptionBubbleLabelAXModeObserver() override = default;

  CaptionBubbleLabelAXModeObserver(const CaptionBubbleLabelAXModeObserver&) =
      delete;
  CaptionBubbleLabelAXModeObserver& operator=(
      const CaptionBubbleLabelAXModeObserver&) = delete;

  void OnAXModeAdded(ui::AXMode mode) override {
    owner_->SetFocusBehaviorForAccessibility();
  }

 private:
  raw_ptr<CaptionBubbleLabel> owner_;
  base::ScopedObservation<ui::AXPlatform, ui::AXModeObserver>
      ax_mode_observation_{this};
};
#endif

CaptionBubble::CaptionBubble(
    CaptionBubbleSettings* caption_bubble_settings,
    std::unique_ptr<TranslationViewWrapperBase> translation_view_wrapper,
    const std::string& application_locale,
    base::OnceClosure destroyed_callback)
    : views::BubbleDialogDelegateView(nullptr,
                                      views::BubbleBorder::TOP_LEFT,
                                      views::BubbleBorder::DIALOG_SHADOW,
                                      true),
      caption_bubble_settings_(caption_bubble_settings),
      translation_view_wrapper_(std::move(translation_view_wrapper)),
      destroyed_callback_(std::move(destroyed_callback)),
      application_locale_(application_locale),
      is_expanded_(caption_bubble_settings_->GetLiveCaptionBubbleExpanded()),
      controls_animation_(this) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  // While not shown, the title is still used to identify the window in the
  // window switcher.
  SetShowTitle(false);
  SetTitle(IDS_LIVE_CAPTION_BUBBLE_TITLE);
  set_has_parent(false);

  controls_animation_.SetSlideDuration(kAnimationDuration);
  controls_animation_.SetTweenType(gfx::Tween::LINEAR);

  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
}

CaptionBubble::~CaptionBubble() {
  if (model_)
    model_->RemoveObserver();
}

gfx::Rect CaptionBubble::GetBubbleBounds() {
  // Bubble bounds are what the computed bubble bounds would be, taking into
  // account the current bubble size.
  gfx::Rect bubble_bounds = views::BubbleDialogDelegateView::GetBubbleBounds();
  // Widget bounds are where the bubble currently is in space.
  gfx::Rect widget_bounds = GetWidget()->GetWindowBoundsInScreen();
  // Use the widget x and y to keep the bubble oriented at its current location,
  // and use the bubble width and height to set the correct bubble size.
  return gfx::Rect(widget_bounds.x(), widget_bounds.y(), bubble_bounds.width(),
                   bubble_bounds.height());
}

void CaptionBubble::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);
  UseCompactMargins();
  set_close_on_deactivate(false);
  SetCanActivate(true);

  views::View* header_container = new views::View();
  header_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  views::View* right_header_container = new views::View();
  views::View* left_header_container = new views::View();
  views::View* translate_header_container = new views::View();

  views::View* content_container = new views::View();
  content_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetInteriorMargin(gfx::Insets::VH(0, kSidePaddingDip))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width*/ true));

  auto label = std::make_unique<CaptionBubbleLabel>();
  label->SetMultiLine(true);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_TOP);
  label->SetCustomTooltipText(std::u16string());
  // Render text truncates the end of text that is greater than 10000 chars.
  // While it is unlikely that the text will exceed 10000 chars, it is not
  // impossible, if the speech service sends a very long transcription_result.
  // In order to guarantee that the caption bubble displays the last lines, and
  // in order to ensure that caption_bubble_->GetTextIndexOfLine() is correct,
  // set the truncate_length to 0 to ensure that it never truncates.
  label->SetTruncateLength(0);

  auto title = std::make_unique<views::Label>();
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title->SetText(l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_TITLE));
  title->GetViewAccessibility().SetIsIgnored(true);

  // Define an error message that will be displayed in the caption bubble if a
  // generic error is encountered.
  auto generic_error_text = std::make_unique<views::Label>();
  generic_error_text->SetBackgroundColor(SK_ColorTRANSPARENT);
  generic_error_text->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  generic_error_text->SetText(
      l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_ERROR));
  auto generic_error_message = std::make_unique<views::View>();
  generic_error_message
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kErrorMessageBetweenChildSpacingDip))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  generic_error_message->SetVisible(false);
  auto generic_error_icon = std::make_unique<views::ImageView>();

#if BUILDFLAG(IS_WIN)
  // Define an error message that will be displayed in the caption bubble if the
  // renderer is using hardware-based decryption.
  auto media_foundation_renderer_error_message =
      std::make_unique<views::View>();
  media_foundation_renderer_error_message
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kErrorMessageBetweenChildSpacingDip))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);
  media_foundation_renderer_error_message->SetVisible(false);
  auto media_foundation_renderer_error_icon =
      std::make_unique<views::ImageView>();
  auto media_foundation_renderer_error_text =
      std::make_unique<MediaFoundationRendererErrorMessageView>(this);
  media_foundation_renderer_error_text->SetAutoColorReadabilityEnabled(false);
  media_foundation_renderer_error_text->SetSubpixelRenderingEnabled(false);
  media_foundation_renderer_error_text->SetFocusBehavior(FocusBehavior::ALWAYS);
  media_foundation_renderer_error_text->SetTextContext(
      views::style::CONTEXT_DIALOG_BODY_TEXT);

  // Make the whole text view behave as a link for accessibility.
  media_foundation_renderer_error_text->GetViewAccessibility().SetRole(
      ax::mojom::Role::kLink);

  const std::u16string link =
      l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_CONTENT_SETTINGS);

  media_foundation_renderer_error_text->SetText(l10n_util::GetStringFUTF16(
      IDS_LIVE_CAPTION_BUBBLE_MEDIA_FOUNDATION_RENDERER_ERROR, link));

  auto media_foundation_renderer_error_checkbox =
      std::make_unique<views::Checkbox>(
          l10n_util::GetStringUTF16(
              IDS_LIVE_CAPTION_BUBBLE_MEDIA_FOUNDATION_RENDERER_ERROR_CHECKBOX),
          base::BindRepeating(
              &CaptionBubble::MediaFoundationErrorCheckboxPressed,
              base::Unretained(this)));
#endif

  base::RepeatingClosure expand_or_collapse_callback = base::BindRepeating(
      &CaptionBubble::ExpandOrCollapseButtonPressed, base::Unretained(this));
  auto expand_button = BuildImageButton(expand_or_collapse_callback,
                                        IDS_LIVE_CAPTION_BUBBLE_EXPAND);
  expand_button->SetVisible(!is_expanded_);

  auto collapse_button = BuildImageButton(
      std::move(expand_or_collapse_callback), IDS_LIVE_CAPTION_BUBBLE_COLLAPSE);
  collapse_button->SetVisible(is_expanded_);

  auto back_to_tab_button = BuildImageButton(
      base::BindRepeating(&CaptionBubble::BackToTabButtonPressed,
                          base::Unretained(this)),
      IDS_LIVE_CAPTION_BUBBLE_BACK_TO_TAB);
  back_to_tab_button->SetVisible(false);

  auto close_button =
      BuildImageButton(base::BindRepeating(&CaptionBubble::CloseButtonPressed,
                                           base::Unretained(this)),
                       IDS_LIVE_CAPTION_BUBBLE_CLOSE);

  back_to_tab_button_ =
      right_header_container->AddChildView(std::move(back_to_tab_button));
  close_button_ = right_header_container->AddChildView(std::move(close_button));

  title_ = content_container->AddChildView(std::move(title));
  label_ = content_container->AddChildView(std::move(label));

  auto download_progress_label = std::make_unique<views::Label>();
  download_progress_label->SetBackgroundColor(SK_ColorTRANSPARENT);
  download_progress_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  download_progress_label->SetVerticalAlignment(
      gfx::VerticalAlignment::ALIGN_MIDDLE);
  download_progress_label->SetVisible(false);
  download_progress_label_ =
      content_container->AddChildView(std::move(download_progress_label));

  generic_error_icon_ =
      generic_error_message->AddChildView(std::move(generic_error_icon));
  generic_error_text_ =
      generic_error_message->AddChildView(std::move(generic_error_text));
  generic_error_message_ =
      content_container->AddChildView(std::move(generic_error_message));

#if BUILDFLAG(IS_WIN)
  media_foundation_renderer_error_icon_ =
      media_foundation_renderer_error_message->AddChildView(
          std::move(media_foundation_renderer_error_icon));

  auto inner_box_layout = std::make_unique<views::BoxLayoutView>();
  inner_box_layout->SetOrientation(views::BoxLayout::Orientation::kVertical);
  inner_box_layout->SetBetweenChildSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  media_foundation_renderer_error_text_ = inner_box_layout->AddChildView(
      std::move(media_foundation_renderer_error_text));
  media_foundation_renderer_error_checkbox_ = inner_box_layout->AddChildView(
      std::move(media_foundation_renderer_error_checkbox));
  media_foundation_renderer_error_message->AddChildView(
      std::move(inner_box_layout));
  media_foundation_renderer_error_message_ = content_container->AddChildView(
      std::move(media_foundation_renderer_error_message));
#endif

  expand_button_ = content_container->AddChildView(std::move(expand_button));
  collapse_button_ =
      content_container->AddChildView(std::move(collapse_button));

  if (IsTranslateHeaderEnabled()) {
    translation_view_wrapper_->Init(translate_header_container,
                                    /*delegate=*/this);
  }

  std::unique_ptr<views::BoxLayout> right_header_container_layout =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal);
  right_header_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  right_header_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  right_header_container->SetLayoutManager(
      std::move(right_header_container_layout));
  std::unique_ptr<views::BoxLayout> translate_header_container_layout =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kLanguageButtonImageLabelSpacing);
  translate_header_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  translate_header_container->SetLayoutManager(
      std::move(translate_header_container_layout));
  translate_header_container_ = left_header_container->AddChildViewRaw(
      std::move(translate_header_container));
  std::unique_ptr<views::BoxLayout> left_header_container_layout =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::TLBR(
              0, close_button_->GetBorder()->GetInsets().width() / 2, 0, 0));
  left_header_container_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  left_header_container->SetLayoutManager(
      std::move(left_header_container_layout));

  left_header_container_ =
      header_container->AddChildViewRaw(std::move(left_header_container));
  header_container->AddChildViewRaw(std::move(right_header_container));
  header_container_ = AddChildViewRaw(std::move(header_container));
  AddChildViewRaw(std::move(content_container));

  if (IsTranslateHeaderEnabled()) {
    std::vector<raw_ptr<views::View, VectorExperimental>> buttons =
        GetButtons();
    for (views::View* button : buttons) {
      button->SetPaintToLayer();
      button->layer()->SetFillsBoundsOpaquely(false);
      button->layer()->SetOpacity(0);
    }

    translate_header_container_->SetPaintToLayer();
    translate_header_container_->layer()->SetFillsBoundsOpaquely(false);
    translate_header_container_->layer()->SetOpacity(0);
    download_progress_label_->SetPaintToLayer();
    download_progress_label_->layer()->SetFillsBoundsOpaquely(false);
    download_progress_label_->layer()->SetOpacity(0);
  }

  UpdateContentSize();
  UpdateAccessibleName();
  title_text_changed_callback_ =
      title_->AddTextChangedCallback(base::BindRepeating(
          &CaptionBubble::OnTitleTextChanged, weak_ptr_factory_.GetWeakPtr()));
}

void CaptionBubble::OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                             views::Widget* widget) const {
  params->type = views::Widget::InitParams::TYPE_WINDOW;
  params->z_order = ui::ZOrderLevel::kFloatingWindow;
  params->visible_on_all_workspaces = true;
  params->name = "LiveCaptionWindow";
#if BUILDFLAG(IS_CHROMEOS)
  params->init_properties_container.SetProperty(kIsCaptionBubbleKey, true);
#endif
}

bool CaptionBubble::ShouldShowCloseButton() const {
  // We draw our own close button so that we can capture the button presses and
  // so we can customize its appearance.
  return false;
}

std::unique_ptr<views::NonClientFrameView>
CaptionBubble::CreateNonClientFrameView(views::Widget* widget) {
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons = GetButtons();
  if (IsTranslateHeaderEnabled()) {
    caption_bubble_event_observer_ =
        std::make_unique<CaptionBubbleEventObserver>(this, widget);
  }

  auto frame = std::make_unique<CaptionBubbleFrameView>(buttons);
  frame_ = frame.get();
  return frame;
}

void CaptionBubble::OnWidgetActivationChanged(views::Widget* widget,
                                              bool active) {
  DCHECK_EQ(widget, GetWidget());

  if (!active && mouse_inside_window_) {
    active = true;
  }

  if (IsTranslateHeaderEnabled()) {
    UpdateControlsVisibility(active);
  }
}

std::u16string CaptionBubble::GetAccessibleWindowTitle() const {
  return std::u16string(title_->GetText());
}

void CaptionBubble::OnThemeChanged() {
  if (ThemeColorsChanged()) {
    SetCaptionBubbleStyle();
  }

  // Call this after SetCaptionButtonStyle(), not before, since
  // SetCaptionButtonStyle() calls SetBackgroundColor(), which
  // OnThemeChanged() will trigger a read of.
  views::BubbleDialogDelegateView::OnThemeChanged();
}

void CaptionBubble::BackToTabButtonPressed() {
  DCHECK(model_);
  DCHECK(model_->GetContext()->IsActivatable());
  model_->GetContext()->Activate();
}

void CaptionBubble::CloseButtonPressed() {
  LogSessionEvent(SessionEvent::kCloseButtonClicked);
  if (model_)
    model_->CloseButtonPressed();

#if BUILDFLAG(IS_CHROMEOS)
  caption_bubble_settings_->SetLiveCaptionEnabled(false);
#endif
}

void CaptionBubble::ExpandOrCollapseButtonPressed() {
  is_expanded_ = !is_expanded_;
  caption_bubble_settings_->SetLiveCaptionBubbleExpanded(is_expanded_);
  base::UmaHistogramBoolean("Accessibility.LiveCaption.ExpandBubble",
                            is_expanded_);

  SwapButtons(collapse_button_, expand_button_, is_expanded_);

  // The change of expanded state may cause the title to change visibility, and
  // it surely causes the content height to change, so redraw the bubble.
  Redraw();
  if (caption_bubble_settings_->ShouldAdjustPositionOnExpand() && model_ &&
      is_expanded_) {
    model_->GetContext()->GetBounds(
        base::BindOnce(&CaptionBubble::AdjustPosition,
                       weak_ptr_factory_.GetWeakPtr(), model_->unique_id()));
  }
}

void CaptionBubble::SwapButtons(views::Button* first_button,
                                views::Button* second_button,
                                bool show_first_button) {
  if (!show_first_button)
    std::swap(first_button, second_button);

  second_button->SetVisible(false);
  first_button->SetVisible(true);

  if (!first_button->HasFocus())
    first_button->RequestFocus();
}

void CaptionBubble::CaptionSettingsButtonPressed() {
  model_->GetContext()->GetOpenCaptionSettingsCallback().Run();
}

void CaptionBubble::SetModel(CaptionBubbleModel* model) {
  if (model_) {
    model_->RemoveObserver();
    model_->GetContext()->RemoveContextActivatabilityObserver();
  }
  model_ = model;
  if (model_) {
    model_->SetObserver(this);
    back_to_tab_button_->SetVisible(model_->GetContext()->IsActivatable());
    model_->GetContext()->SetContextActivatabilityObserver(this);
    translation_view_wrapper_->UpdateLanguageLabel();
  } else {
    UpdateBubbleVisibility();
  }
}

void CaptionBubble::AnimationProgressed(const gfx::Animation* animation) {
  if (!IsTranslateHeaderEnabled()) {
    return;
  }
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons = GetButtons();
  for (views::View* button : buttons) {
    button->layer()->SetOpacity(animation->GetCurrentValue());
  }
  translate_header_container_->layer()->SetOpacity(
      animation->GetCurrentValue());
  download_progress_label_->layer()->SetOpacity(animation->GetCurrentValue());
}

void CaptionBubble::OnContextActivatabilityChanged() {
  back_to_tab_button_->SetVisible(model_->GetContext()->IsActivatable());
  UpdateContentSize();
}

void CaptionBubble::OnTextChanged() {
  DCHECK(model_);
  std::string text = model_->GetFullText();
  label_->SetText(base::UTF8ToUTF16(text));
  UpdateBubbleAndTitleVisibility();
}

void CaptionBubble::OnDownloadProgressTextChanged() {
  if (!caption_bubble_settings_->IsLiveTranslateFeatureEnabled()) {
    return;
  }

  DCHECK(model_);
  download_progress_label_->SetText(model_->GetDownloadProgressText());
  download_progress_label_->SetVisible(true);

  // Do not display captions while language packs are downloading.
  label_->SetVisible(false);

  UpdateBubbleAndTitleVisibility();

  if (GetWidget()->IsVisible()) {
    UpdateControlsVisibility(true);
  }
}

void CaptionBubble::OnLanguagePackInstalled() {
  if (IsTranslateHeaderEnabled()) {
    download_progress_label_->SetVisible(false);
    label_->SetVisible(true);
  }
}

void CaptionBubble::OnAutoDetectedLanguageChanged() {
  if (!IsTranslateHeaderEnabled()) {
    return;
  }
  std::string auto_detected_language_code =
      model_->GetAutoDetectedLanguageCode();
  translation_view_wrapper_->OnAutoDetectedLanguageChanged(
      auto_detected_language_code);
}

bool CaptionBubble::ThemeColorsChanged() {
  const auto* const color_provider = GetColorProvider();
  SkColor text_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleForegroundDefault);
  SkColor icon_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleButtonIcon);
  SkColor icon_disabled_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleButtonIconDisabled);
  SkColor link_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleLink);
  SkColor checkbox_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleCheckbox);
  SkColor background_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleBackgroundDefault);

  bool theme_colors_changed =
      text_color != text_color_ || icon_color != icon_color_ ||
      icon_disabled_color != icon_disabled_color_ ||
      link_color != link_color_ || checkbox_color != checkbox_color_ ||
      background_color != background_color_;

  text_color_ = text_color;
  icon_color_ = icon_color;
  icon_disabled_color_ = icon_disabled_color;
  link_color_ = link_color;
  checkbox_color_ = checkbox_color;
  background_color_ = background_color;

  return theme_colors_changed;
}

void CaptionBubble::OnErrorChanged(
    CaptionBubbleErrorType error_type,
    OnErrorClickedCallback callback,
    OnDoNotShowAgainClickedCallback error_silenced_callback) {
  DCHECK(model_);
  error_clicked_callback_ = std::move(callback);
  error_silenced_callback_ = std::move(error_silenced_callback);
  bool has_error = model_->HasError();
  label_->SetVisible(!has_error);
  expand_button_->SetVisible(!has_error && !is_expanded_);
  collapse_button_->SetVisible(!has_error && is_expanded_);

#if BUILDFLAG(IS_WIN)
  if (error_type ==
      CaptionBubbleErrorType::kMediaFoundationRendererUnsupported) {
    media_foundation_renderer_error_message_->SetVisible(has_error);
    generic_error_message_->SetVisible(false);
  } else {
    generic_error_message_->SetVisible(has_error);
    media_foundation_renderer_error_message_->SetVisible(false);
  }
#else
  generic_error_message_->SetVisible(has_error);
#endif

  Redraw();
}

#if BUILDFLAG(IS_WIN)
void CaptionBubble::OnContentSettingsLinkClicked() {
  if (error_clicked_callback_) {
    error_clicked_callback_.Run();
  }
}
#endif

void CaptionBubble::UpdateControlsVisibility(bool show_controls) {
  if (show_controls) {
    controls_animation_.Show();
  } else {
    controls_animation_.Hide();
  }
}

void CaptionBubble::OnMouseEnteredOrExitedWindow(bool entered) {
  mouse_inside_window_ = entered;
  UpdateControlsVisibility(mouse_inside_window_);
}

void CaptionBubble::UpdateBubbleAndTitleVisibility() {
  // Show the title if there is room for it and no error.
  title_->SetVisible(model_ && !model_->HasError() &&
                     GetNumLinesInLabel() <
                         static_cast<size_t>(GetNumLinesVisible()));
  UpdateBubbleVisibility();
}

void CaptionBubble::UpdateBubbleVisibility() {
  DCHECK(GetWidget());
  // If there is no model set, do not show the bubble.
  if (!model_) {
    Hide();
    return;
  }

  // Hide the widget if the model is closed.
  if (model_->IsClosed()) {
    Hide();
    return;
  }

  // Show the widget if it has text or an error or download progress to display.
  if (!model_->GetFullText().empty() || model_->HasError() ||
      (caption_bubble_settings_->IsLiveTranslateFeatureEnabled() &&
       download_progress_label_->GetVisible())) {
    ShowInactive();
    return;
  }

  // No text and no error. Hide it.
  Hide();
}

void CaptionBubble::UpdateCaptionStyle(
    std::optional<ui::CaptionStyle> caption_style) {
  caption_style_ = caption_style;
  SetCaptionBubbleStyle();
  Redraw();
}

size_t CaptionBubble::GetTextIndexOfLineInLabel(size_t line) const {
  return label_->GetTextIndexOfLine(line);
}

size_t CaptionBubble::GetNumLinesInLabel() const {
  return label_->GetRequiredLines();
}

int CaptionBubble::GetNumLinesVisible() {
  return is_expanded_ ? kNumLinesExpanded : kNumLinesCollapsed;
}

double CaptionBubble::GetTextScaleFactor() {
  double textScaleFactor = 1;
  if (caption_style_) {
    std::string text_size = MaybeRemoveCSSImportant(caption_style_->text_size);
    if (!text_size.empty()) {
      // ui::CaptionStyle states that text_size is percentage as a CSS string.
      bool match =
          RE2::FullMatch(text_size, "(\\d+\\.?\\d*)%", &textScaleFactor);
      textScaleFactor = match ? textScaleFactor / 100 : 1;
    }
  }
  return textScaleFactor;
}
const gfx::FontList CaptionBubble::GetFontList(int font_size) {
  std::vector<std::string> font_names;
  if (caption_style_) {
    std::string font_family =
        MaybeRemoveCSSImportant(caption_style_->font_family);
    if (!font_family.empty())
      font_names.push_back(font_family);
  }
  font_names.push_back(kPrimaryFont);
  font_names.push_back(kSecondaryFont);
  font_names.push_back(kTertiaryFont);

  const gfx::FontList font_list = gfx::FontList(
      font_names, gfx::Font::FontStyle::NORMAL,
      font_size * GetTextScaleFactor(), gfx::Font::Weight::NORMAL);
  return font_list;
}

void CaptionBubble::SetTextSizeAndFontFamily() {
  double textScaleFactor = GetTextScaleFactor();
  const gfx::FontList font_list = GetFontList(kFontSizePx);
  label_->SetFontList(font_list);
  title_->SetFontList(font_list.DeriveWithStyle(gfx::Font::FontStyle::ITALIC));
  generic_error_text_->SetFontList(font_list);

  label_->SetLineHeight(kLineHeightDip * textScaleFactor);
  label_->SetMaximumWidth(kMaxWidthDip * textScaleFactor - kSidePaddingDip * 2);
  title_->SetLineHeight(kLineHeightDip * textScaleFactor);
  if (IsTranslateHeaderEnabled()) {
    download_progress_label_->SetLineHeight(kLiveTranslateLabelLineHeightDip *
                                            textScaleFactor);
    download_progress_label_->SetFontList(
        GetFontList(kLiveTranslateLabelFontSizePx));
    translation_view_wrapper_->SetTextSizeAndFontFamily(
        textScaleFactor, GetFontList(kLiveTranslateLabelFontSizePx));
  }
  generic_error_text_->SetLineHeight(kLineHeightDip * textScaleFactor);
  generic_error_icon_->SetImageSize(
      gfx::Size(kErrorImageSizeDip * textScaleFactor,
                kErrorImageSizeDip * textScaleFactor));

#if BUILDFLAG(IS_WIN)
  media_foundation_renderer_error_icon_->SetImageSize(
      gfx::Size(kErrorImageSizeDip, kErrorImageSizeDip));
  media_foundation_renderer_error_text_->SizeToFit(
      kMaxWidthDip * textScaleFactor - kSidePaddingDip * 2);
#endif
}

void CaptionBubble::SetTextColor() {
  const auto* const color_provider = GetColorProvider();
  SkColor primary_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleForegroundDefault);
  SkColor header_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleButtonIcon);
  SkColor language_label_color =
      color_provider->GetColor(ui::kColorRefPrimary80);
  SkColor language_label_border_color =
      color_provider->GetColor(ui::kColorRefSecondary50);
  SkColor icon_disabled_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleButtonIconDisabled);

  // Update Live Translate label style with the default colors before parsing
  // the CSS color string.
  download_progress_label_->SetEnabledColor(primary_color);

  if (caption_style_) {
    ParseNonTransparentRGBACSSColorString(caption_style_->text_color,
                                          &primary_color, color_provider);
    ParseNonTransparentRGBACSSColorString(caption_style_->text_color,
                                          &header_color, color_provider);
    ParseNonTransparentRGBACSSColorString(
        caption_style_->text_color, &language_label_color, color_provider);
    ParseNonTransparentRGBACSSColorString(caption_style_->text_color,
                                          &language_label_border_color,
                                          color_provider);
  }

  label_->SetEnabledColor(primary_color);
  title_->SetEnabledColor(primary_color);
  generic_error_text_->SetEnabledColor(primary_color);

  generic_error_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kErrorOutlineIcon, primary_color));

  if (IsTranslateHeaderEnabled()) {
    translation_view_wrapper_->SetTextColor(
        language_label_color, language_label_border_color, header_color);
  }

#if BUILDFLAG(IS_WIN)

  const std::u16string link =
      l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_CONTENT_SETTINGS);
  size_t offset;
  const std::u16string text = l10n_util::GetStringFUTF16(
      IDS_LIVE_CAPTION_BUBBLE_MEDIA_FOUNDATION_RENDERER_ERROR,
      l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_CONTENT_SETTINGS),
      &offset);

  media_foundation_renderer_error_text_->ClearStyleRanges();
  views::StyledLabel::RangeStyleInfo error_message_style;
  error_message_style.override_color = primary_color;
  media_foundation_renderer_error_text_->AddStyleRange(gfx::Range(0, offset),
                                                       error_message_style);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&CaptionBubble::OnContentSettingsLinkClicked,
                              base::Unretained(this)));
  link_style.override_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleLink);
  media_foundation_renderer_error_text_->AddStyleRange(
      gfx::Range(offset, offset + link.length()), link_style);

  media_foundation_renderer_error_text_->AddStyleRange(
      gfx::Range(offset + link.length(), text.length()), error_message_style);
  media_foundation_renderer_error_icon_->SetImage(
      ui::ImageModel::FromVectorIcon(vector_icons::kErrorOutlineIcon,
                                     primary_color));
  media_foundation_renderer_error_checkbox_->SetEnabledTextColors(
      primary_color);
  media_foundation_renderer_error_checkbox_->SetTextSubpixelRenderingEnabled(
      false);
  media_foundation_renderer_error_checkbox_->SetCheckedIconImageColor(
      color_provider->GetColor(ui::kColorLiveCaptionBubbleCheckbox));
#endif
  views::SetImageFromVectorIconWithColor(
      back_to_tab_button_, vector_icons::kBackToTabChromeRefreshIcon,
      kButtonDip, header_color, icon_disabled_color);
  views::SetImageFromVectorIconWithColor(
      close_button_, vector_icons::kCloseRoundedIcon, kButtonDip, header_color,
      icon_disabled_color);
  views::SetImageFromVectorIconWithColor(
      expand_button_, vector_icons::kCaretDownIcon, kButtonDip, header_color,
      icon_disabled_color);
  views::SetImageFromVectorIconWithColor(collapse_button_,
                                         vector_icons::kCaretUpIcon, kButtonDip,
                                         header_color, icon_disabled_color);
}

void CaptionBubble::SetBackgroundColor() {
  const auto* const color_provider = GetColorProvider();
  SkColor background_color =
      color_provider->GetColor(ui::kColorLiveCaptionBubbleBackgroundDefault);
  if (caption_style_ &&
      !ParseNonTransparentRGBACSSColorString(
          caption_style_->window_color, &background_color, color_provider)) {
    ParseNonTransparentRGBACSSColorString(caption_style_->background_color,
                                          &background_color, color_provider);
  }

  views::BubbleDialogDelegateView::SetBackgroundColor(background_color);
  GetWidget()->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kDark);
}

void CaptionBubble::OnLanguageChanged(const std::string& display_language) {
  UpdateLanguageDirection(display_language);
  SetTextColor();
  Redraw();
}

void CaptionBubble::UpdateLanguageDirection(
    const std::string& display_language) {
  label_->SetHorizontalAlignment(
      base::i18n::GetTextDirectionForLocale(display_language.c_str()) ==
              base::i18n::TextDirection::RIGHT_TO_LEFT
          ? gfx::HorizontalAlignment::ALIGN_RIGHT
          : gfx::HorizontalAlignment::ALIGN_LEFT);
}

void CaptionBubble::RepositionInContextRect(CaptionBubbleModel::Id model_id,
                                            const gfx::Rect& context_rect) {
  // We shouldn't reposition ourselves into the context rect of a model that is
  // no longer active.
  if (model_ == nullptr || model_->unique_id() != model_id) {
    return;
  }

  gfx::Rect inset_rect = context_rect;
  inset_rect.Inset(gfx::Insets(kMinAnchorMarginDip));
  gfx::Rect bubble_bounds = GetBubbleBounds();

  // The placement is based on the ratio between the center of the widget and
  // the center of the inset_rect.
  int target_x = inset_rect.x() + inset_rect.width() * kDefaultRatioInParentX -
                 bubble_bounds.width() / 2.0;
  int target_y = inset_rect.y() + inset_rect.height() * kDefaultRatioInParentY -
                 bubble_bounds.height() / 2.0;
  gfx::Rect target_bounds = gfx::Rect(target_x, target_y, bubble_bounds.width(),
                                      bubble_bounds.height());
  if (!inset_rect.Contains(target_bounds)) {
    target_bounds.AdjustToFit(inset_rect);
  }

  if (model_->GetContext()->ShouldAvoidOverlap()) {
    gfx::Rect intersection = context_rect;
    intersection.Intersect(target_bounds);
    if (intersection.size().GetArea() >
        context_rect.size().GetArea() * kContextSufficientOverlapRatio) {
      // Place below if there's room, otherwise place above.
      std::optional<display::Display> display =
          GetWidget()->GetNearestDisplay();
      if (!display.has_value() ||
          context_rect.bottom() + target_bounds.height() <
              display->bounds().bottom()) {
        target_bounds.Offset(0, context_rect.height());
      } else {
        target_bounds.Offset(0,
                             -context_rect.height() - kMinAnchorMarginDip * 2);
      }

      GetWidget()->SetBoundsConstrained(target_bounds);
      return;
    }
  }

  GetWidget()->SetBounds(target_bounds);
}

void CaptionBubble::AdjustPosition(CaptionBubbleModel::Id model_id,
                                   const gfx::Rect& context_rect) {
  // We shouldn't reposition ourselves into the context rect of a model that is
  // no longer active.
  if (model_ == nullptr || model_->unique_id() != model_id) {
    return;
  }
  gfx::Rect inset_rect = context_rect;
  inset_rect.Inset(gfx::Insets(kMinAnchorMarginDip));
  gfx::Rect bubble_bounds = GetBubbleBounds();
  if (!inset_rect.Contains(bubble_bounds)) {
    bubble_bounds.AdjustToFit(inset_rect);
    GetWidget()->SetBounds(bubble_bounds);
  }
}

void CaptionBubble::UpdateContentSize() {
  double text_scale_factor = GetTextScaleFactor();
  int width = kMaxWidthDip * text_scale_factor;
  int content_height =
      kLineHeightDip * GetNumLinesVisible() * text_scale_factor;

  // The title takes up 1 line.
  int label_height = title_->GetVisible()
                         ? content_height - kLineHeightDip * text_scale_factor
                         : content_height;
  label_->SetMinimumHeight(label_height);
  auto button_size = close_button_->GetPreferredSize({});

  // The back-to-tab button is only visible if the context can be activated. The
  // close button is always visible.
  int num_buttons = back_to_tab_button_->GetVisible() ? 2 : 1;

  auto left_header_width = width - num_buttons * button_size.width();
  left_header_container_->SetPreferredSize(
      gfx::Size(left_header_width, button_size.height()));
  download_progress_label_->SetPreferredSize(gfx::Size(width, content_height));

  if (IsTranslateHeaderEnabled()) {
    translation_view_wrapper_->UpdateContentSize();
  }

#if BUILDFLAG(IS_WIN)
  // The Media Foundation renderer error message should not scale with the
  // user's caption style preference.
  if (HasMediaFoundationError()) {
    width = kMaxWidthDip;
    content_height = media_foundation_renderer_error_message_
                         ->GetPreferredSize(
                             views::SizeBounds(width - kSidePaddingDip * 2, {}))
                         .height();
  }
#endif

  // The header height is the same as the close button height. The footer height
  // is the same as the expand button height.
  SetPreferredSize(gfx::Size(
      width, content_height + close_button_->GetPreferredSize({}).height() +
                 expand_button_->GetPreferredSize({}).height()));
}

void CaptionBubble::Redraw() {
  UpdateBubbleAndTitleVisibility();
  UpdateContentSize();
}

void CaptionBubble::ShowInactive() {
  DCHECK(model_);
  if (GetWidget()->IsVisible())
    return;

  GetWidget()->ShowInactive();
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      IDS_LIVE_CAPTION_BUBBLE_APPEAR_SCREENREADER_ANNOUNCEMENT));
  LogSessionEvent(SessionEvent::kStreamStarted);

  // If the caption bubble has already been shown, do not reposition it.
  if (has_been_shown_)
    return;
  has_been_shown_ = true;

  // The first time that the caption bubble is shown, reposition it to the
  // bottom center of the context widget for the currently set model.
  model_->GetContext()->GetBounds(
      base::BindOnce(&CaptionBubble::RepositionInContextRect,
                     weak_ptr_factory_.GetWeakPtr(), model_->unique_id()));
}

void CaptionBubble::Hide() {
  if (!GetWidget()->IsVisible())
    return;
  GetWidget()->Hide();
  LogSessionEvent(SessionEvent::kStreamEnded);
}

void CaptionBubble::MediaFoundationErrorCheckboxPressed() {
#if BUILDFLAG(IS_WIN)
  error_silenced_callback_.Run(
      CaptionBubbleErrorType::kMediaFoundationRendererUnsupported,
      media_foundation_renderer_error_checkbox_->GetChecked());
#endif
}

bool CaptionBubble::HasMediaFoundationError() {
  return (model_ && model_->HasError() &&
          model_->ErrorType() ==
              CaptionBubbleErrorType::kMediaFoundationRendererUnsupported);
}

void CaptionBubble::LogSessionEvent(SessionEvent event) {
  if (model_ && !model_->HasError()) {
    base::UmaHistogramEnumeration("Accessibility.LiveCaption.Session2", event);
  }
}

std::vector<raw_ptr<views::View, VectorExperimental>>
CaptionBubble::GetButtons() {
  // TODO: the extraction here needs to be removed once the VectorExperimental
  // alias is removed.
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons = {
      back_to_tab_button_.get(), close_button_.get(), expand_button_.get(),
      collapse_button_.get()};

  if (IsTranslateHeaderEnabled()) {
    std::vector<raw_ptr<views::View, VectorExperimental>> language_buttons =
        translation_view_wrapper_->GetButtons();
    buttons.insert(buttons.end(), language_buttons.begin(),
                   language_buttons.end());
  }

  return buttons;
}

views::Label* CaptionBubble::GetLabelForTesting() {
  return views::AsViewClass<views::Label>(label_);
}

views::Label* CaptionBubble::GetDownloadProgressLabelForTesting() {
  return views::AsViewClass<views::Label>(download_progress_label_);
}

bool CaptionBubble::IsGenericErrorMessageVisibleForTesting() const {
  return generic_error_message_->GetVisible();
}

void CaptionBubble::SetCaptionBubbleStyle() {
  SetTextSizeAndFontFamily();
  if (GetWidget()) {
    SetTextColor();
    SetBackgroundColor();
    GetWidget()->ThemeChanged();
  }
}

views::Button* CaptionBubble::GetCloseButtonForTesting() {
  return close_button_.get();
}

views::Button* CaptionBubble::GetBackToTabButtonForTesting() {
  return back_to_tab_button_.get();
}

views::View* CaptionBubble::GetHeaderForTesting() {
  return header_container_.get();
}

TranslationViewWrapperBase*
CaptionBubble::GetTranslationViewWrapperForTesting() {
  return translation_view_wrapper_.get();
}

void CaptionBubble::OnTitleTextChanged() {
  UpdateAccessibleName();
  if (views::Widget* widget = GetWidget()) {
    widget->UpdateAccessibleNameForRootView();
  }
}

void CaptionBubble::UpdateAccessibleName() {
  GetViewAccessibility().SetName(std::u16string(title_->GetText()));
}

bool CaptionBubble::IsTranslateHeaderEnabled() const {
  return caption_bubble_settings_->IsLiveTranslateFeatureEnabled() ||
         base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage);
}

BEGIN_METADATA(CaptionBubble)
END_METADATA

}  // namespace captions
