// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_com_win.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/enum_variant.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_enum_localization_util.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/win/accessibility_ids_win.h"
#include "ui/base/win/atl_module.h"

// There is no easy way to decouple |kScreenReader| and |kHTML| accessibility
// modes when Windows screen readers are used. For example, certain roles use
// the HTML tag name. Input fields require their type attribute to be exposed.
const uint32_t kScreenReaderAndHTMLAccessibilityModes =
    ui::AXMode::kScreenReader | ui::AXMode::kHTML;

namespace content {

void AddAccessibilityModeFlags(ui::AXMode mode_flags) {
  BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
      mode_flags);
}

//
// BrowserAccessibilityComWin::WinAttributes
//

BrowserAccessibilityComWin::WinAttributes::WinAttributes()
    : ignored(false), ia_role(0), ia_state(0), ia2_role(0), ia2_state(0) {}

BrowserAccessibilityComWin::WinAttributes::~WinAttributes() {}

//
// BrowserAccessibilityComWin
//
BrowserAccessibilityComWin::BrowserAccessibilityComWin()
    : owner_(nullptr),
      win_attributes_(new WinAttributes()),
      previous_scroll_x_(0),
      previous_scroll_y_(0) {}

BrowserAccessibilityComWin::~BrowserAccessibilityComWin() = default;

//
// IAccessible2 overrides:
//

IFACEMETHODIMP BrowserAccessibilityComWin::get_attributes(BSTR* attributes) {
  // This can be removed once ISimpleDOMNode is migrated
  return AXPlatformNodeWin::get_attributes(attributes);
}

IFACEMETHODIMP BrowserAccessibilityComWin::scrollTo(IA2ScrollType scroll_type) {
  // This can be removed once ISimpleDOMNode is migrated
  return AXPlatformNodeWin::scrollTo(scroll_type);
}

//
// IAccessibleApplication methods.
//

IFACEMETHODIMP BrowserAccessibilityComWin::get_appName(BSTR* app_name) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_APP_NAME);

  if (!app_name)
    return E_INVALIDARG;

  // GetProduct() returns a string like "Chrome/aa.bb.cc.dd", split out
  // the part before the "/".
  std::vector<std::string> product_components =
      base::SplitString(GetContentClient()->browser()->GetProduct(), "/",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // |GetProduct| will return an empty string if we are running the content
  // shell instead of Chrome.
  if (product_components.size() != 2)
    return E_FAIL;
  *app_name = SysAllocString(base::UTF8ToWide(product_components[0]).c_str());
  DCHECK(*app_name);
  return *app_name ? S_OK : E_FAIL;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_appVersion(BSTR* app_version) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_APP_VERSION);

  if (!app_version)
    return E_INVALIDARG;

  // GetProduct() returns a string like "Chrome/aa.bb.cc.dd", split out
  // the part after the "/".
  std::vector<std::string> product_components =
      base::SplitString(GetContentClient()->browser()->GetProduct(), "/",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // |GetProduct| will return an empty string if we are running the content
  // shell instead of Chrome.
  if (product_components.size() != 2)
    return E_FAIL;
  *app_version =
      SysAllocString(base::UTF8ToWide(product_components[1]).c_str());
  DCHECK(*app_version);
  return *app_version ? S_OK : E_FAIL;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_toolkitName(BSTR* toolkit_name) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TOOLKIT_NAME);
  if (!toolkit_name)
    return E_INVALIDARG;

  *toolkit_name = SysAllocString(FRAMEWORK_ID);
  DCHECK(*toolkit_name);
  return *toolkit_name ? S_OK : E_FAIL;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_toolkitVersion(
    BSTR* toolkit_version) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TOOLKIT_VERSION);
  if (!toolkit_version)
    return E_INVALIDARG;

  std::string user_agent = GetContentClient()->browser()->GetUserAgent();
  *toolkit_version = SysAllocString(base::UTF8ToWide(user_agent).c_str());
  DCHECK(*toolkit_version);
  return *toolkit_version ? S_OK : E_FAIL;
}

//
// IAccessibleImage methods.
//

IFACEMETHODIMP BrowserAccessibilityComWin::get_description(BSTR* desc) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_DESCRIPTION);
  if (!owner())
    return E_FAIL;

  if (!desc)
    return E_INVALIDARG;

  if (description().empty())
    return S_FALSE;

  *desc = SysAllocString(description().c_str());

  DCHECK(*desc);
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_imagePosition(
    IA2CoordinateType coordinate_type,
    LONG* x,
    LONG* y) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_IMAGE_POSITION);
  if (!owner())
    return E_FAIL;

  if (!x || !y)
    return E_INVALIDARG;

  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    gfx::Rect bounds = owner()->GetUnclippedScreenBoundsRect();
    *x = bounds.x();
    *y = bounds.y();
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    gfx::Rect bounds = owner()->GetClippedRootFrameBoundsRect();
    gfx::Rect parent_bounds =
        owner()->PlatformGetParent()
            ? owner()->PlatformGetParent()->GetClippedRootFrameBoundsRect()
            : gfx::Rect();
    *x = bounds.x() - parent_bounds.x();
    *y = bounds.y() - parent_bounds.y();
  } else {
    return E_INVALIDARG;
  }

  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_imageSize(LONG* height,
                                                         LONG* width) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_IMAGE_SIZE);
  if (!owner())
    return E_FAIL;

  if (!height || !width)
    return E_INVALIDARG;

  *height = owner()->GetClippedRootFrameBoundsRect().height();
  *width = owner()->GetClippedRootFrameBoundsRect().width();
  return S_OK;
}

//
// IAccessibleText methods.
//

IFACEMETHODIMP BrowserAccessibilityComWin::get_characterExtents(
    LONG offset,
    IA2CoordinateType coordinate_type,
    LONG* out_x,
    LONG* out_y,
    LONG* out_width,
    LONG* out_height) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CHARACTER_EXTENTS);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  if (!out_x || !out_y || !out_width || !out_height)
    return E_INVALIDARG;

  const std::u16string& text_str = GetHypertext();
  HandleSpecialTextOffset(&offset);
  if (offset < 0 || offset > static_cast<LONG>(text_str.size()))
    return E_INVALIDARG;

  gfx::Rect character_bounds;
  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    character_bounds = owner()->GetScreenHypertextRangeBoundsRect(
        offset, 1, ui::AXClippingBehavior::kUnclipped);
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    character_bounds = owner()->GetRootFrameHypertextRangeBoundsRect(
        offset, 1, ui::AXClippingBehavior::kUnclipped);
    if (owner()->PlatformGetParent()) {
      character_bounds -= owner()
                              ->PlatformGetParent()
                              ->GetUnclippedRootFrameBoundsRect()
                              .OffsetFromOrigin();
    }
  } else {
    return E_INVALIDARG;
  }

  *out_x = character_bounds.x();
  *out_y = character_bounds.y();
  *out_width = character_bounds.width();
  *out_height = character_bounds.height();

  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_nSelections(LONG* n_selections) {
  return AXPlatformNodeWin::get_nSelections(n_selections);
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_selection(LONG selection_index,
                                                         LONG* start_offset,
                                                         LONG* end_offset) {
  return AXPlatformNodeWin::get_selection(selection_index, start_offset,
                                          end_offset);
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_text(LONG start_offset,
                                                    LONG end_offset,
                                                    BSTR* text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TEXT);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!text)
    return E_INVALIDARG;

  const std::u16string& text_str = GetHypertext();
  HandleSpecialTextOffset(&start_offset);
  HandleSpecialTextOffset(&end_offset);

  // The spec allows the arguments to be reversed.
  if (start_offset > end_offset)
    std::swap(start_offset, end_offset);

  LONG len = static_cast<LONG>(text_str.length());
  if (start_offset < 0 || start_offset > len)
    return E_INVALIDARG;
  if (end_offset < 0 || end_offset > len)
    return E_INVALIDARG;

  std::u16string substr =
      text_str.substr(start_offset, end_offset - start_offset);
  if (substr.empty())
    return S_FALSE;

  *text = SysAllocString(base::as_wcstr(substr));
  DCHECK(*text);
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_newText(
    IA2TextSegment* new_text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_NEW_TEXT);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!new_text)
    return E_INVALIDARG;

  if (!old_win_attributes_)
    return E_FAIL;

  size_t start, old_len, new_len;
  ComputeHypertextRemovedAndInserted(&start, &old_len, &new_len);
  if (new_len == 0)
    return E_FAIL;

  std::u16string substr = GetHypertext().substr(start, new_len);
  new_text->text = SysAllocString(base::as_wcstr(substr));
  new_text->start = static_cast<LONG>(start);
  new_text->end = static_cast<LONG>(start + new_len);
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_oldText(
    IA2TextSegment* old_text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_OLD_TEXT);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!old_text)
    return E_INVALIDARG;

  if (!old_win_attributes_)
    return E_FAIL;

  size_t start, old_len, new_len;
  ComputeHypertextRemovedAndInserted(&start, &old_len, &new_len);
  if (old_len == 0)
    return E_FAIL;

  std::u16string old_hypertext = old_hypertext_.hypertext;
  std::u16string substr = old_hypertext.substr(start, old_len);
  old_text->text = SysAllocString(base::as_wcstr(substr));
  old_text->start = static_cast<LONG>(start);
  old_text->end = static_cast<LONG>(start + old_len);
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::scrollSubstringTo(
    LONG start_index,
    LONG end_index,
    IA2ScrollType scroll_type) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_SUBSTRING_TO);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  // TODO(dmazzoni): adjust this for the start and end index, too.
  return scrollTo(scroll_type);
}

IFACEMETHODIMP BrowserAccessibilityComWin::scrollSubstringToPoint(
    LONG start_index,
    LONG end_index,
    IA2CoordinateType coordinate_type,
    LONG x,
    LONG y) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_SUBSTRING_TO_POINT);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  if (start_index > end_index)
    std::swap(start_index, end_index);
  LONG length = end_index - start_index + 1;
  DCHECK_GE(length, 0);

  gfx::Rect string_bounds = owner()->GetRootFrameHypertextRangeBoundsRect(
      start_index, length, ui::AXClippingBehavior::kUnclipped);
  string_bounds -=
      owner()->GetUnclippedRootFrameBoundsRect().OffsetFromOrigin();
  x -= string_bounds.x();
  y -= string_bounds.y();

  return scrollToPoint(coordinate_type, x, y);
}

IFACEMETHODIMP BrowserAccessibilityComWin::setCaretOffset(LONG offset) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SET_CARET_OFFSET);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;
  SetIA2HypertextSelection(offset, offset);
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::setSelection(LONG selection_index,
                                                        LONG start_offset,
                                                        LONG end_offset) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SET_SELECTION);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;
  if (selection_index != 0)
    return E_INVALIDARG;
  SetIA2HypertextSelection(start_offset, end_offset);
  return S_OK;
}

// IAccessibleText::get_attributes()
// Returns text attributes -- not HTML attributes!
IFACEMETHODIMP BrowserAccessibilityComWin::get_attributes(
    LONG offset,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text_attributes) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_IATEXT_GET_ATTRIBUTES);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!start_offset || !end_offset || !text_attributes)
    return E_INVALIDARG;

  *start_offset = *end_offset = 0;
  *text_attributes = nullptr;
  if (!owner())
    return E_FAIL;

  const std::u16string text = GetHypertext();
  HandleSpecialTextOffset(&offset);
  if (offset < 0 || offset > static_cast<LONG>(text.size()))
    return E_INVALIDARG;

  ComputeStylesIfNeeded();
  *start_offset = FindStartOfStyle(offset, ax::mojom::MoveDirection::kBackward);
  *end_offset = FindStartOfStyle(offset, ax::mojom::MoveDirection::kForward);

  std::ostringstream attributes_stream;
  auto iter = offset_to_text_attributes().find(*start_offset);
  if (iter != offset_to_text_attributes().end()) {
    const ui::TextAttributeList& attributes = iter->second;

    for (const ui::TextAttribute& attribute : attributes) {
      // Don't expose the default language value of "en-US".
      // TODO(nektar): Determine if it's possible to check against the interface
      // language.
      if (attribute.first == "language" && attribute.second == "en-US")
        continue;

      attributes_stream << attribute.first << ":" << attribute.second << ";";
    }
  }
  std::wstring attributes_str = base::UTF8ToWide(attributes_stream.str());

  // Returning an empty string is valid and indicates no attributes.
  // This is better than returning S_FALSE which the screen reader
  // may not recognize as valid attributes.
  *text_attributes = SysAllocString(attributes_str.c_str());
  DCHECK(*text_attributes);
  return S_OK;
}

//
// IAccessibleHypertext methods.
//

IFACEMETHODIMP BrowserAccessibilityComWin::get_nHyperlinks(
    LONG* hyperlink_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_HYPERLINKS);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!hyperlink_count)
    return E_INVALIDARG;

  *hyperlink_count = hypertext_.hyperlink_offset_to_index.size();

  DCHECK(!ui::IsIframe(owner()->GetRole()) || *hyperlink_count <= 1)
      << "iframes should have 1 hyperlink, unless the child document is "
         "destroyed/unloaded, in which case it should have 0";

  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_hyperlink(
    LONG index,
    IAccessibleHyperlink** hyperlink) {
  *hyperlink = nullptr;
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_HYPERLINK);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!hyperlink || index < 0 ||
      index >= static_cast<LONG>(hypertext_.hyperlinks.size())) {
    return E_INVALIDARG;
  }

  DCHECK(!ui::IsIframe(owner()->GetRole()) || index == 0)
      << "An iframe cannot have more than 1 hyperlink";

  int32_t id = hypertext_.hyperlinks[index];
  AXPlatformNode* node = AXPlatformNodeWin::GetFromUniqueId(id);
  if (!node) {
    // TODO(https://crbug.com/id=1164043) Fix illegal hyperlink of iframes.
    // Based on information received from DumpWithoutCrashing() reports, this
    // is still sometimes occurring when get_hyperlink() is called on an
    // iframe, which would have exactly 1 hyperlink and no text. The
    // DumpWithoutCrashing() was removed to reduced crash report noise.
    // Interestingly, the top url reported was always called "empty".
    // Sample report for iframe issue: go/crash/93d7fce137a15ef0
#if defined(AX_FAIL_FAST_BUILD)  // Fail in debug/sanitizers/clusterfuzz.
    bool do_report = true;
#else
    std::srand(std::time(nullptr));             // Use current time as seed.
    bool do_report = (std::rand() % 100 == 0);  // Roughly 1% of the time.
#endif
    if (do_report) {
      LONG num_hyperlinks = -1;
      get_nHyperlinks(&num_hyperlinks);
      std::ostringstream error;
      ui::AXTreeManager* manager = GetDelegate()->GetTreeManager();
      LOG(FATAL) << "Hyperlink error:\n index=" << index
                 << " nHyperLinks=" << hypertext_.hyperlinks.size()
                 << " hyperlink_id=" << id
                 << "\nparent=" << GetDelegate()->node()
                 << "\nframe=" << manager->GetRoot()
                 << "\nroot=" << manager->GetRootManager()->GetRoot();
    }
    return E_FAIL;
  }
  auto* link = static_cast<BrowserAccessibilityComWin*>(node);
  if (!link)
    return E_FAIL;

  *hyperlink = static_cast<IAccessibleHyperlink*>(link->NewReference());
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_hyperlinkIndex(
    LONG char_index,
    LONG* hyperlink_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_HYPERLINK_INDEX);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!hyperlink_index)
    return E_INVALIDARG;

  if (char_index < 0 ||
      char_index >= static_cast<LONG>(GetHypertext().size())) {
    return E_INVALIDARG;
  }

  std::map<int32_t, int32_t>::iterator it =
      hypertext_.hyperlink_offset_to_index.find(char_index);
  if (it == hypertext_.hyperlink_offset_to_index.end()) {
    *hyperlink_index = -1;
    return S_FALSE;
  }

  *hyperlink_index = it->second;
  return S_OK;
}

//
// IAccessibleHyperlink methods.
//

// Currently, only text links are supported.
IFACEMETHODIMP BrowserAccessibilityComWin::get_anchor(LONG index,
                                                      VARIANT* anchor) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ANCHOR);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner() || !IsHyperlink())
    return E_FAIL;

  // IA2 text links can have only one anchor, that is the text inside them.
  if (index != 0 || !anchor)
    return E_INVALIDARG;

  BSTR ia2_hypertext = SysAllocString(base::as_wcstr(GetHypertext()));
  DCHECK(ia2_hypertext);
  anchor->vt = VT_BSTR;
  anchor->bstrVal = ia2_hypertext;

  // Returning S_FALSE is not mentioned in the IA2 Spec, but it might have been
  // an oversight.
  if (!SysStringLen(ia2_hypertext))
    return S_FALSE;

  return S_OK;
}

// Currently, only text links are supported.
IFACEMETHODIMP BrowserAccessibilityComWin::get_anchorTarget(
    LONG index,
    VARIANT* anchor_target) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ANCHOR_TARGET);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner() || !IsHyperlink())
    return E_FAIL;

  // IA2 text links can have at most one target, that is when they represent an
  // HTML hyperlink, i.e. an <a> element with a "href" attribute.
  if (index != 0 || !anchor_target)
    return E_INVALIDARG;

  BSTR target;
  if (!(MSAAState() & STATE_SYSTEM_LINKED) ||
      FAILED(GetStringAttributeAsBstr(ax::mojom::StringAttribute::kUrl,
                                      &target))) {
    target = SysAllocString(L"");
  }
  DCHECK(target);
  anchor_target->vt = VT_BSTR;
  anchor_target->bstrVal = target;

  // Returning S_FALSE is not mentioned in the IA2 Spec, but it might have been
  // an oversight.
  if (!SysStringLen(target))
    return S_FALSE;

  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_startIndex(LONG* index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_START_INDEX);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner() || !IsHyperlink())
    return E_FAIL;

  if (!index)
    return E_INVALIDARG;

  int32_t hypertext_offset = 0;
  auto* parent = owner()->PlatformGetParent();
  if (parent) {
    hypertext_offset =
        ToBrowserAccessibilityComWin(parent)->GetHypertextOffsetFromChild(this);
  }
  *index = static_cast<LONG>(hypertext_offset);
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_endIndex(LONG* index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_END_INDEX);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  LONG start_index;
  HRESULT hr = get_startIndex(&start_index);
  if (hr == S_OK)
    *index = start_index + 1;
  return hr;
}

// This method is deprecated in the IA2 Spec.
IFACEMETHODIMP BrowserAccessibilityComWin::get_valid(boolean* valid) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_VALID);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  return E_NOTIMPL;
}

//
// IAccessibleAction partly implemented.
//

IFACEMETHODIMP BrowserAccessibilityComWin::nActions(LONG* n_actions) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_N_ACTIONS);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!n_actions)
    return E_INVALIDARG;

  *n_actions = static_cast<LONG>(owner()->GetSupportedActions().size());
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::doAction(LONG action_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_DO_ACTION);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  const std::vector<ax::mojom::Action> actions = owner()->GetSupportedActions();
  if (action_index < 0 || action_index >= static_cast<LONG>(actions.size()))
    return E_INVALIDARG;

  ui::AXActionData data;
  data.action = actions[action_index];
  owner()->AccessibilityPerformAction(data);

  return S_OK;
}

IFACEMETHODIMP
BrowserAccessibilityComWin::get_description(LONG action_index,
                                            BSTR* description) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_IAACTION_GET_DESCRIPTION);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  return E_NOTIMPL;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_keyBinding(LONG action_index,
                                                          LONG n_max_bindings,
                                                          BSTR** key_bindings,
                                                          LONG* n_bindings) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_KEY_BINDING);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!key_bindings || !n_bindings)
    return E_INVALIDARG;

  *key_bindings = nullptr;
  *n_bindings = 0;

  const std::vector<ax::mojom::Action> actions = owner()->GetSupportedActions();
  if (action_index < 0 || action_index >= static_cast<LONG>(actions.size()))
    return E_INVALIDARG;

  // Only the default action, in index 0, may have a key binding. If it does,
  // it will be stored in the attribute kAccessKey.
  std::u16string key_binding_string;
  if (action_index != 0 || !owner()->HasDefaultActionVerb() ||
      !owner()->GetString16Attribute(ax::mojom::StringAttribute::kAccessKey,
                                     &key_binding_string)) {
    return S_FALSE;
  }

  *n_bindings = 1;
  *key_bindings = static_cast<BSTR*>(CoTaskMemAlloc(sizeof(BSTR)));
  (*key_bindings)[0] = SysAllocString(base::as_wcstr(key_binding_string));
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_name(LONG action_index,
                                                    BSTR* name) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_NAME);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!name)
    return E_INVALIDARG;

  const std::vector<ax::mojom::Action> actions = owner()->GetSupportedActions();
  if (action_index < 0 || action_index >= static_cast<LONG>(actions.size())) {
    *name = nullptr;
    return E_INVALIDARG;
  }

  int action;
  std::string action_verb;
  if (action_index == 0 &&
      owner()->GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb,
                               &action)) {
    action_verb =
        ui::ToString(static_cast<ax::mojom::DefaultActionVerb>(action));
  } else {
    action_verb = ui::ToString(actions[action_index]);
  }

  if (action_verb.empty() || action_verb.compare("none") == 0) {
    *name = nullptr;
    return S_FALSE;
  }

  *name = SysAllocString(base::as_wcstr(base::UTF8ToUTF16(action_verb)));
  DCHECK(name);
  return S_OK;
}

IFACEMETHODIMP
BrowserAccessibilityComWin::get_localizedName(LONG action_index,
                                              BSTR* localized_name) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LOCALIZED_NAME);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!localized_name)
    return E_INVALIDARG;

  const std::vector<ax::mojom::Action> actions = owner()->GetSupportedActions();
  if (action_index < 0 || action_index >= static_cast<LONG>(actions.size())) {
    *localized_name = nullptr;
    return E_INVALIDARG;
  }

  int action;
  if (!owner()->GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb,
                                &action) ||
      action_index != 0) {
    // There aren't localized names for actions except default ones, we fall
    // back to returning the hard-coded, not localized name.
    return get_name(action_index, localized_name);
  }

  std::string action_verb =
      ui::ToLocalizedString(static_cast<ax::mojom::DefaultActionVerb>(action));
  if (action_verb.empty()) {
    *localized_name = nullptr;
    return S_FALSE;
  }

  *localized_name =
      SysAllocString(base::as_wcstr(base::UTF8ToUTF16(action_verb)));
  DCHECK(localized_name);
  return S_OK;
}

//
// ISimpleDOMDocument methods.
//

IFACEMETHODIMP BrowserAccessibilityComWin::get_URL(BSTR* url) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_URL);
  if (!owner())
    return E_FAIL;

  auto* manager = Manager();
  if (!manager)
    return E_FAIL;

  if (!url)
    return E_INVALIDARG;

  if (owner() != manager->GetBrowserAccessibilityRoot())
    return E_FAIL;

  std::string str = manager->GetTreeData().url;
  if (str.empty())
    return S_FALSE;

  *url = SysAllocString(base::UTF8ToWide(str).c_str());
  DCHECK(*url);

  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_title(BSTR* title) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TITLE);
  if (!owner())
    return E_FAIL;

  auto* manager = Manager();
  if (!manager)
    return E_FAIL;

  if (!title)
    return E_INVALIDARG;

  std::string str = manager->GetTreeData().title;
  if (str.empty())
    return S_FALSE;

  *title = SysAllocString(base::UTF8ToWide(str).c_str());
  DCHECK(*title);

  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_mimeType(BSTR* mime_type) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_MIME_TYPE);
  if (!owner())
    return E_FAIL;

  auto* manager = Manager();
  if (!manager)
    return E_FAIL;

  if (!mime_type)
    return E_INVALIDARG;

  std::string str = manager->GetTreeData().mimetype;
  if (str.empty())
    return S_FALSE;

  *mime_type = SysAllocString(base::UTF8ToWide(str).c_str());
  DCHECK(*mime_type);

  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_docType(BSTR* doc_type) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_DOC_TYPE);
  if (!owner())
    return E_FAIL;

  auto* manager = Manager();
  if (!manager)
    return E_FAIL;

  if (!doc_type)
    return E_INVALIDARG;

  std::string str = manager->GetTreeData().doctype;
  if (str.empty())
    return S_FALSE;

  *doc_type = SysAllocString(base::UTF8ToWide(str).c_str());
  DCHECK(*doc_type);

  return S_OK;
}

IFACEMETHODIMP
BrowserAccessibilityComWin::get_nameSpaceURIForID(SHORT name_space_id,
                                                  BSTR* name_space_uri) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_NAMESPACE_URI_FOR_ID);
  return E_NOTIMPL;
}

IFACEMETHODIMP
BrowserAccessibilityComWin::put_alternateViewMediaTypes(
    BSTR* comma_separated_media_types) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_PUT_ALTERNATE_VIEW_MEDIA_TYPES);
  return E_NOTIMPL;
}

//
// ISimpleDOMNode methods.
//

IFACEMETHODIMP BrowserAccessibilityComWin::get_nodeInfo(
    BSTR* node_name,
    SHORT* name_space_id,
    BSTR* node_value,
    unsigned int* num_children,
    unsigned int* unique_id,
    USHORT* node_type) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_NODE_INFO);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!node_name || !name_space_id || !node_value || !num_children ||
      !unique_id || !node_type) {
    return E_INVALIDARG;
  }

  std::u16string tag;
  if (owner()->GetString16Attribute(ax::mojom::StringAttribute::kHtmlTag, &tag))
    *node_name = SysAllocString(base::as_wcstr(tag));
  else
    *node_name = nullptr;

  *name_space_id = 0;
  *node_value = SysAllocString(value().c_str());
  *num_children = owner()->PlatformChildCount();
  *unique_id = -AXPlatformNodeWin::GetUniqueId();

  if (ui::IsPlatformDocument(owner()->GetRole())) {
    *node_type = NODETYPE_DOCUMENT;
  } else if (owner()->IsText()) {
    *node_type = NODETYPE_TEXT;
  } else {
    *node_type = NODETYPE_ELEMENT;
  }

  return S_OK;
}

// ISimpleDOMNode::get_attributes()
// Returns HTML attributes -- not text attributes!
IFACEMETHODIMP BrowserAccessibilityComWin::get_attributes(USHORT max_attribs,
                                                          BSTR* attrib_names,
                                                          SHORT* name_space_id,
                                                          BSTR* attrib_values,
                                                          USHORT* num_attribs) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ISIMPLEDOMNODE_GET_ATTRIBUTES);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!attrib_names || !name_space_id || !attrib_values || !num_attribs)
    return E_INVALIDARG;

  *num_attribs = max_attribs;
  if (*num_attribs > owner()->GetHtmlAttributes().size())
    *num_attribs = owner()->GetHtmlAttributes().size();

  for (USHORT i = 0; i < *num_attribs; ++i) {
    const std::string& attribute = owner()->GetHtmlAttributes()[i].first;
    // Work around JAWS crash in JAWS <= 17, and unpatched versions of JAWS
    // 2018/2019.
    // TODO(accessibility) Remove once JAWS <= 17 is no longer a concern.
    // Wait until 2021 for this, as JAWS users are slow to update.
    if (attribute == "srcdoc" || attribute == "data-srcdoc")
      continue;

    attrib_names[i] = SysAllocString(base::UTF8ToWide(attribute).c_str());
    name_space_id[i] = 0;
    attrib_values[i] = SysAllocString(
        base::UTF8ToWide(owner()->GetHtmlAttributes()[i].second).c_str());
  }
  return S_OK;
}

// ISimpleDOMNode::get_attributesForNames()
// Returns HTML attributes -- not text attributes!
IFACEMETHODIMP BrowserAccessibilityComWin::get_attributesForNames(
    USHORT num_attribs,
    BSTR* attrib_names,
    SHORT* name_space_id,
    BSTR* attrib_values) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ATTRIBUTES_FOR_NAMES);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!attrib_names || !name_space_id || !attrib_values)
    return E_INVALIDARG;

  for (USHORT i = 0; i < num_attribs; ++i) {
    name_space_id[i] = 0;
    bool found = false;
    std::string name = base::WideToUTF8((LPCWSTR)attrib_names[i]);
    for (unsigned int j = 0; j < owner()->GetHtmlAttributes().size(); ++j) {
      if (owner()->GetHtmlAttributes()[j].first == name) {
        attrib_values[i] = SysAllocString(
            base::UTF8ToWide(owner()->GetHtmlAttributes()[j].second).c_str());
        found = true;
        break;
      }
    }
    if (!found) {
      attrib_values[i] = NULL;
    }
  }
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_computedStyle(
    USHORT max_style_properties,
    boolean use_alternate_view,
    BSTR* style_properties,
    BSTR* style_values,
    USHORT* num_style_properties) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COMPUTED_STYLE);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!style_properties || !style_values)
    return E_INVALIDARG;

  // We only cache a single style property for now: DISPLAY

  std::u16string display;
  if (max_style_properties == 0 ||
      !owner()->GetString16Attribute(ax::mojom::StringAttribute::kDisplay,
                                     &display)) {
    *num_style_properties = 0;
    return S_OK;
  }

  *num_style_properties = 1;
  style_properties[0] = SysAllocString(L"display");
  style_values[0] = SysAllocString(base::as_wcstr(display));

  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_computedStyleForProperties(
    USHORT num_style_properties,
    boolean use_alternate_view,
    BSTR* style_properties,
    BSTR* style_values) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COMPUTED_STYLE_FOR_PROPERTIES);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!style_properties || !style_values)
    return E_INVALIDARG;

  // We only cache a single style property for now: DISPLAY

  for (USHORT i = 0; i < num_style_properties; ++i) {
    std::u16string name =
        base::ToLowerASCII(base::as_u16cstr(style_properties[i]));
    if (name == u"display") {
      std::u16string display =
          owner()->GetString16Attribute(ax::mojom::StringAttribute::kDisplay);
      style_values[i] = SysAllocString(base::as_wcstr(display));
    } else {
      style_values[i] = NULL;
    }
  }

  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::scrollTo(boolean placeTopLeft) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ISIMPLEDOMNODE_SCROLL_TO);
  return scrollTo(placeTopLeft ? IA2_SCROLL_TYPE_TOP_LEFT
                               : IA2_SCROLL_TYPE_ANYWHERE);
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_parentNode(
    ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PARENT_NODE);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  *node = ToBrowserAccessibilityComWin(owner()->PlatformGetParent())
              ->NewReference();
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_firstChild(
    ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_FIRST_CHILD);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (owner()->PlatformChildCount() == 0) {
    *node = NULL;
    return S_FALSE;
  }

  *node = ToBrowserAccessibilityComWin(owner()->PlatformGetFirstChild())
              ->NewReference();
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_lastChild(
    ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LAST_CHILD);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (owner()->PlatformChildCount() == 0) {
    *node = NULL;
    return S_FALSE;
  }

  *node = ToBrowserAccessibilityComWin(owner()->PlatformGetLastChild())
              ->NewReference();
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_previousSibling(
    ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PREVIOUS_SIBLING);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  absl::optional<size_t> index_in_parent = absl::nullopt;
  if (owner()->PlatformGetParent())
    index_in_parent = GetIndexInParent();
  if (!index_in_parent.has_value() || index_in_parent.value() == 0) {
    *node = NULL;
    return S_FALSE;
  }

  *node = ToBrowserAccessibilityComWin(owner()->InternalGetPreviousSibling())
              ->NewReference();
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_nextSibling(
    ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_NEXT_SIBLING);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  absl::optional<size_t> index_in_parent = absl::nullopt;
  if (owner()->PlatformGetParent())
    index_in_parent = GetIndexInParent();
  if (!index_in_parent.has_value() ||
      (index_in_parent.value() + 1) >=
          owner()->PlatformGetParent()->InternalChildCount()) {
    *node = NULL;
    return S_FALSE;
  }

  *node = ToBrowserAccessibilityComWin(owner()->InternalGetNextSibling())
              ->NewReference();
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_childAt(unsigned int child_index,
                                                       ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CHILD_AT);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (child_index >= owner()->PlatformChildCount())
    return E_INVALIDARG;

  BrowserAccessibility* child = owner()->PlatformGetChild(child_index);
  if (!child) {
    *node = NULL;
    return S_FALSE;
  }

  *node = ToBrowserAccessibilityComWin(child)->NewReference();
  return S_OK;
}

// We only support this method for retrieving MathML content.
IFACEMETHODIMP BrowserAccessibilityComWin::get_innerHTML(BSTR* innerHTML) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_INNER_HTML);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;
  if (owner()->GetRole() != ax::mojom::Role::kMath &&
      owner()->GetRole() != ax::mojom::Role::kMathMLMath)
    return E_NOTIMPL;

  std::u16string inner_html =
      owner()->GetString16Attribute(ax::mojom::StringAttribute::kInnerHtml);
  *innerHTML = SysAllocString(base::as_wcstr(inner_html));
  DCHECK(*innerHTML);
  return S_OK;
}

IFACEMETHODIMP
BrowserAccessibilityComWin::get_localInterface(void** local_interface) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LOCAL_INTERFACE);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  return E_NOTIMPL;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_language(BSTR* language) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LANGUAGE);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!language)
    return E_INVALIDARG;
  *language = nullptr;

  if (!owner())
    return E_FAIL;

  std::wstring lang = base::UTF8ToWide(owner()->node()->GetLanguage());
  if (lang.empty())
    lang = L"en-US";

  *language = SysAllocString(lang.c_str());
  DCHECK(*language);
  return S_OK;
}

//
// ISimpleDOMText methods.
//

IFACEMETHODIMP BrowserAccessibilityComWin::get_domText(BSTR* dom_text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_DOM_TEXT);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!dom_text)
    return E_INVALIDARG;

  return GetNameAsBstr(dom_text);
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_clippedSubstringBounds(
    unsigned int start_index,
    unsigned int end_index,
    int* out_x,
    int* out_y,
    int* out_width,
    int* out_height) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CLIPPED_SUBSTRING_BOUNDS);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  // TODO(dmazzoni): fully support this API by intersecting the
  // rect with the container's rect.
  return get_unclippedSubstringBounds(start_index, end_index, out_x, out_y,
                                      out_width, out_height);
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_unclippedSubstringBounds(
    unsigned int start_index,
    unsigned int end_index,
    int* out_x,
    int* out_y,
    int* out_width,
    int* out_height) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_UNCLIPPED_SUBSTRING_BOUNDS);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  if (!out_x || !out_y || !out_width || !out_height)
    return E_INVALIDARG;

  unsigned int text_length = static_cast<unsigned int>(GetHypertext().size());
  if (start_index > text_length || end_index > text_length ||
      start_index > end_index) {
    return E_INVALIDARG;
  }

  gfx::Rect bounds = owner()->GetScreenHypertextRangeBoundsRect(
      start_index, end_index - start_index, ui::AXClippingBehavior::kUnclipped);
  *out_x = bounds.x();
  *out_y = bounds.y();
  *out_width = bounds.width();
  *out_height = bounds.height();
  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::scrollToSubstring(
    unsigned int start_index,
    unsigned int end_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_TO_SUBSTRING);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  auto* manager = Manager();
  if (!manager)
    return E_FAIL;

  unsigned int text_length = static_cast<unsigned int>(GetHypertext().size());
  if (start_index > text_length || end_index > text_length ||
      start_index > end_index) {
    return E_INVALIDARG;
  }

  manager->ScrollToMakeVisible(*owner(),
                               owner()->GetRootFrameHypertextRangeBoundsRect(
                                   start_index, end_index - start_index,
                                   ui::AXClippingBehavior::kUnclipped));

  return S_OK;
}

IFACEMETHODIMP BrowserAccessibilityComWin::get_fontFamily(BSTR* font_family) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_FONT_FAMILY);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!font_family)
    return E_INVALIDARG;
  *font_family = nullptr;

  if (!owner())
    return E_FAIL;

  std::u16string family = owner()->GetInheritedString16Attribute(
      ax::mojom::StringAttribute::kFontFamily);
  if (family.empty())
    return S_FALSE;

  *font_family = SysAllocString(base::as_wcstr(family));
  DCHECK(*font_family);
  return S_OK;
}

//
// IServiceProvider methods.
//

IFACEMETHODIMP BrowserAccessibilityComWin::QueryService(REFGUID guid_service,
                                                        REFIID riid,
                                                        void** object) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_QUERY_SERVICE);
  if (!owner())
    return E_FAIL;

  if (guid_service == GUID_IAccessibleContentDocument) {
    // Special Mozilla extension: return the accessible for the root document.
    // Screen readers use this to distinguish between a document loaded event
    // on the root document vs on an iframe.
    BrowserAccessibility* node = owner();
    while (node->PlatformGetParent())
      node =
          node->PlatformGetParent()->manager()->GetBrowserAccessibilityRoot();
    return ToBrowserAccessibilityComWin(node)->QueryInterface(IID_IAccessible2,
                                                              object);
  }

  if (guid_service == IID_IAccessible || guid_service == IID_IAccessible2 ||
      guid_service == IID_IAccessibleAction ||
      guid_service == IID_IAccessibleApplication ||
      guid_service == IID_IAccessibleHyperlink ||
      guid_service == IID_IAccessibleHypertext ||
      guid_service == IID_IAccessibleImage ||
      guid_service == IID_IAccessibleTable ||
      guid_service == IID_IAccessibleTable2 ||
      guid_service == IID_IAccessibleTableCell ||
      guid_service == IID_IAccessibleText ||
      guid_service == IID_IAccessibleTextSelectionContainer ||
      guid_service == IID_IAccessibleValue ||
      guid_service == IID_ISimpleDOMDocument ||
      guid_service == IID_ISimpleDOMNode ||
      guid_service == IID_ISimpleDOMText || guid_service == GUID_ISimpleDOM) {
    return QueryInterface(riid, object);
  }

  *object = NULL;
  return E_FAIL;
}

//
// CComObjectRootEx methods.
//

// static
STDMETHODIMP BrowserAccessibilityComWin::InternalQueryInterface(
    void* this_ptr,
    const _ATL_INTMAP_ENTRY* entries,
    REFIID iid,
    void** object) {
  BrowserAccessibilityComWin* accessibility =
      reinterpret_cast<BrowserAccessibilityComWin*>(this_ptr);

  if (!accessibility || !accessibility->owner()) {
    *object = nullptr;
    return E_NOINTERFACE;
  }

  if (iid == IID_IAccessibleImage) {
    const ax::mojom::Role role = accessibility->owner()->GetRole();
    if (!ui::IsImage(role)) {
      *object = nullptr;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_ISimpleDOMDocument) {
    if (!ui::IsPlatformDocument(accessibility->GetRole())) {
      *object = nullptr;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleHyperlink) {
    if (!accessibility->IsHyperlink()) {
      *object = nullptr;
      return E_NOINTERFACE;
    }
  }

  return AXPlatformNodeWin::InternalQueryInterface(this_ptr, entries, iid,
                                                   object);
}

void BrowserAccessibilityComWin::ComputeStylesIfNeeded() {
  if (!offset_to_text_attributes().empty())
    return;

  ui::TextAttributeList default_attributes =
      AXPlatformNodeWin::ComputeTextAttributes();
  ui::TextAttributeMap attributes_map =
      GetDelegate()->ComputeTextAttributeMap(default_attributes);
  win_attributes_->offset_to_text_attributes.swap(attributes_map);
}

//
// Private methods.
//

void BrowserAccessibilityComWin::UpdateStep1ComputeWinAttributes() {
  // Swap win_attributes_ to old_win_attributes_, allowing us to see
  // exactly what changed and fire appropriate events. Note that
  // old_win_attributes_ is cleared at the end of UpdateStep3FireEvents.
  old_win_attributes_.swap(win_attributes_);

  old_hypertext_ = hypertext_;
  hypertext_ = ui::AXLegacyHypertext();

  win_attributes_ = std::make_unique<WinAttributes>();

  win_attributes_->ia_role = MSAARole();
  win_attributes_->ia_state = MSAAState();

  win_attributes_->ia2_role = ComputeIA2Role();
  // If we didn't explicitly set the IAccessible2 role, make it the same
  // as the MSAA role.
  if (!win_attributes_->ia2_role)
    win_attributes_->ia2_role = win_attributes_->ia_role;
  win_attributes_->ia2_state = ComputeIA2State();
  win_attributes_->ia2_attributes = ComputeIA2Attributes();
  win_attributes_->name = base::UTF8ToWide(owner()->GetName());
  win_attributes_->description = base::UTF8ToWide(
      owner()->GetStringAttribute(ax::mojom::StringAttribute::kDescription));
  win_attributes_->value = base::UTF16ToWide(GetValueForControl());
  win_attributes_->ignored = owner()->IsIgnored();
}

void BrowserAccessibilityComWin::UpdateStep2ComputeHypertext() {
  UpdateComputedHypertext();
}

void BrowserAccessibilityComWin::UpdateStep3FireEvents() {
  const bool ignored = owner()->IsIgnored();

  // Suppress all of these events when the node is ignored, or when the ignored
  // state has changed on a node that isn't part of an active live region.
  if (ignored || (old_win_attributes_->ignored != ignored &&
                  !owner()->GetData().IsContainedInActiveLiveRegion() &&
                  !owner()->GetData().IsActiveLiveRegionRoot())) {
    return;
  }

  // The rest of the events only fire on changes, not on new objects.

  if (old_win_attributes_->ia_role != 0) {
    // Fire an event if the description, help, or value changes.
    if (description() != old_win_attributes_->description)
      FireNativeEvent(EVENT_OBJECT_DESCRIPTIONCHANGE);

    // Fire an event if this container object has scrolled.
    int sx = 0;
    int sy = 0;
    if (owner()->GetIntAttribute(ax::mojom::IntAttribute::kScrollX, &sx) &&
        owner()->GetIntAttribute(ax::mojom::IntAttribute::kScrollY, &sy)) {
      if (sx != previous_scroll_x_ || sy != previous_scroll_y_)
        FireNativeEvent(EVENT_SYSTEM_SCROLLINGEND);
      previous_scroll_x_ = sx;
      previous_scroll_y_ = sy;
    }

    // Fire hypertext-related events.
    // Do not fire removed/inserted when a name change event will be fired by
    // AXEventGenerator, as they are providing redundant information and will
    // lead to duplicate announcements.
    if (name() == old_win_attributes_->name ||
        GetNameFrom() == ax::mojom::NameFrom::kContents) {
      size_t start, old_len, new_len;
      ComputeHypertextRemovedAndInserted(&start, &old_len, &new_len);
      if (old_len > 0) {
        // In-process screen readers may call IAccessibleText::get_oldText
        // in reaction to this event to retrieve the text that was removed.
        FireNativeEvent(IA2_EVENT_TEXT_REMOVED);
      }
      if (new_len > 0) {
        // In-process screen readers may call IAccessibleText::get_newText
        // in reaction to this event to retrieve the text that was inserted.
        FireNativeEvent(IA2_EVENT_TEXT_INSERTED);
      }
    }
  }

  old_win_attributes_.reset(nullptr);
  old_hypertext_ = ui::AXLegacyHypertext();
}

BrowserAccessibilityManager* BrowserAccessibilityComWin::Manager() const {
  DCHECK(owner());

  auto* manager = owner()->manager();
  DCHECK(manager);
  return manager;
}

//
// AXPlatformNode overrides
//
void BrowserAccessibilityComWin::Destroy() {
  // Detach BrowserAccessibilityWin from us.
  owner_ = nullptr;
  AXPlatformNodeWin::Destroy();
}

void BrowserAccessibilityComWin::Init(ui::AXPlatformNodeDelegate* delegate) {
  owner_ = static_cast<BrowserAccessibilityWin*>(delegate);
  AXPlatformNodeWin::Init(delegate);
}

BrowserAccessibilityComWin* BrowserAccessibilityComWin::NewReference() {
  AddRef();
  return this;
}

BrowserAccessibilityComWin* BrowserAccessibilityComWin::GetTargetFromChildID(
    const VARIANT& var_id) {
  if (!owner())
    return nullptr;

  if (var_id.vt != VT_I4)
    return nullptr;

  LONG child_id = var_id.lVal;
  if (child_id == CHILDID_SELF)
    return this;

  if (child_id >= 1 &&
      child_id <= static_cast<LONG>(owner()->PlatformChildCount()))
    return ToBrowserAccessibilityComWin(
        owner()->PlatformGetChild(child_id - 1));

  auto* child = static_cast<BrowserAccessibilityComWin*>(
      AXPlatformNodeWin::GetFromUniqueId(-child_id));
  if (child && child->owner()->IsDescendantOf(owner()))
    return child;

  return nullptr;
}

HRESULT BrowserAccessibilityComWin::GetStringAttributeAsBstr(
    ax::mojom::StringAttribute attribute,
    BSTR* value_bstr) {
  if (!owner())
    return E_FAIL;

  std::u16string str;
  if (!owner()->GetString16Attribute(attribute, &str))
    return S_FALSE;

  *value_bstr = SysAllocString(base::as_wcstr(str));
  DCHECK(*value_bstr);

  return S_OK;
}

HRESULT BrowserAccessibilityComWin::GetNameAsBstr(BSTR* value_bstr) {
  if (!owner())
    return E_FAIL;

  std::u16string str;
  str = owner()->GetNameAsString16();
  *value_bstr = SysAllocString(base::as_wcstr(str));
  DCHECK(*value_bstr);

  return S_OK;
}

void BrowserAccessibilityComWin::SetIA2HypertextSelection(LONG start_offset,
                                                          LONG end_offset) {
  HandleSpecialTextOffset(&start_offset);
  HandleSpecialTextOffset(&end_offset);
  SetHypertextSelection(start_offset, end_offset);
}

LONG BrowserAccessibilityComWin::FindStartOfStyle(
    LONG start_offset,
    ax::mojom::MoveDirection direction) {
  LONG text_length = static_cast<LONG>(GetHypertext().length());
  DCHECK_GE(start_offset, 0);
  DCHECK_LE(start_offset, text_length);

  switch (direction) {
    case ax::mojom::MoveDirection::kNone:
      NOTREACHED();
      return start_offset;
    case ax::mojom::MoveDirection::kBackward: {
      if (offset_to_text_attributes().empty())
        return 0;

      auto iterator = offset_to_text_attributes().upper_bound(start_offset);
      --iterator;
      return static_cast<LONG>(iterator->first);
    }
    case ax::mojom::MoveDirection::kForward: {
      const auto iterator =
          offset_to_text_attributes().upper_bound(start_offset);
      if (iterator == offset_to_text_attributes().end())
        return text_length;
      return static_cast<LONG>(iterator->first);
    }
  }

  NOTREACHED();
  return start_offset;
}

BrowserAccessibilityComWin* BrowserAccessibilityComWin::GetFromID(
    int32_t id) const {
  if (!owner())
    return nullptr;
  return ToBrowserAccessibilityComWin(Manager()->GetFromID(id));
}

void BrowserAccessibilityComWin::FireNativeEvent(LONG win_event_type) const {
  // We only allow events on descendants of a platform leaf when that platform
  // leaf is a popup button parent of a menu list popup. On Windows, the menu
  // list popup is not part of the tree when its parent is collapsed but events
  // should be fired anyway.
  if (owner()->IsChildOfLeaf() &&
      !owner()->GetCollapsedMenuListSelectAncestor()) {
    return;
  }

  Manager()->ToBrowserAccessibilityManagerWin()->FireWinAccessibilityEvent(
      win_event_type, owner());
}

BrowserAccessibilityComWin* ToBrowserAccessibilityComWin(
    BrowserAccessibility* obj) {
  if (!obj)
    return nullptr;
  auto* result = static_cast<BrowserAccessibilityWin*>(obj)->GetCOM();
  return result;
}

}  // namespace content
