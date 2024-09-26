// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_LAYOUT_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_LAYOUT_PROVIDER_H_

#include <memory>

#include "chrome/browser/ui/views/chrome_typography_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_provider.h"

enum ChromeInsetsMetric {
  // Padding around buttons on the bookmarks bar.
  INSETS_BOOKMARKS_BAR_BUTTON = views::VIEWS_INSETS_END,
  // Margins used by toasts.
  INSETS_TOAST,
  // Padding used in an omnibox pill button.
  INSETS_OMNIBOX_PILL_BUTTON,
  // Padding used in an page info hover button.
  INSETS_PAGE_INFO_HOVER_BUTTON,
};

enum ChromeDistanceMetric {
  // Default minimum width of a button.
  DISTANCE_BUTTON_MINIMUM_WIDTH = views::VIEWS_DISTANCE_END,
  // Vertical spacing at the beginning and end of a content list (a vertical
  // stack of composite views that behaves like a menu) containing one item.
  DISTANCE_CONTENT_LIST_VERTICAL_SINGLE,
  // Same as |DISTANCE_CONTENT_LIST_VERTICAL_SINGLE|, but used at the beginning
  // and end of a multi-item content list.
  DISTANCE_CONTENT_LIST_VERTICAL_MULTI,
  // Vertical spacing between a list of multiple controls in one column.
  DISTANCE_CONTROL_LIST_VERTICAL,
  // Width of the space in a dropdown button between its label and down arrow.
  DISTANCE_DROPDOWN_BUTTON_LABEL_ARROW_SPACING,
  // Width of the horizontal padding in a dropdown button between the down arrow
  // and the button's border.
  DISTANCE_DROPDOWN_BUTTON_RIGHT_MARGIN,
  // Width and height of a button's icon in the extensions menu.
  DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE,
  // Width and height of an extension's icon in the extensions menu. This are
  // larger than menu button's icons because it contains internal padding to
  // provide space for badging.
  DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE,
  // Size difference between the two types of icons in the menu. This is used as
  // horizontal and vertical margins to align extensions menu rows.
  DISTANCE_EXTENSIONS_MENU_ICON_SPACING,
  // Vertical and horizontal margin for menu buttons.
  DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN,
  // Smaller horizontal spacing between other controls that are logically
  // related.
  DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL,
  // Smaller vertical spacing between controls that are logically related.
  DISTANCE_RELATED_CONTROL_VERTICAL_SMALL,
  // Horizontal spacing between an item and the related label, in the context of
  // a row of such items. E.g. the bookmarks bar.
  DISTANCE_RELATED_LABEL_HORIZONTAL_LIST,
  // Horizontal indent of a subsection relative to related items above, e.g.
  // checkboxes below explanatory text/headings.
  DISTANCE_SUBSECTION_HORIZONTAL_INDENT,
  // Vertical margin for controls in a toast.
  DISTANCE_TOAST_CONTROL_VERTICAL,
  // Vertical margin for labels in a toast.
  DISTANCE_TOAST_LABEL_VERTICAL,
  // Horizontal spacing between controls that are logically unrelated.
  DISTANCE_UNRELATED_CONTROL_HORIZONTAL,
  // Larger horizontal spacing between unrelated controls.
  DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE,
  // Larger vertical spacing between unrelated controls.
  DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE,
  // Width of larger modal dialogs that require extra width.
  DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH,
  // Width and height of a vector icon in a bubble's header (i.e. the one
  // returned from GetWindowIcon).
  DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE,
  // Width of a bubble that appears mid-screen (like a standalone dialog)
  // instead of being anchored.
  DISTANCE_STANDALONE_BUBBLE_PREFERRED_WIDTH,
  // Horizontal spacing between value and description in the row.
  DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL,
  // Vertical padding at the top and bottom of the an omnibox match row.
  DISTANCE_OMNIBOX_CELL_VERTICAL_PADDING,
  // Vertical padding at the top and bottom of the an omnibox match row for two
  // line layout.
  DISTANCE_OMNIBOX_TWO_LINE_CELL_VERTICAL_PADDING,
  // Width and Height of a vector icon in the side panel header.
  DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE,
  // Minimum size of the header vector icon buttons to get the proper ripple.
  DISTANCE_SIDE_PANEL_HEADER_BUTTON_MINIMUM_SIZE,
  // Horizontal spacing for separating side panel header border from controls.
  DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL,
};

class ChromeLayoutProvider : public views::LayoutProvider {
 public:
  ChromeLayoutProvider();

  ChromeLayoutProvider(const ChromeLayoutProvider&) = delete;
  ChromeLayoutProvider& operator=(const ChromeLayoutProvider&) = delete;

  ~ChromeLayoutProvider() override;

  static ChromeLayoutProvider* Get();
  static std::unique_ptr<views::LayoutProvider> CreateLayoutProvider();

  // views::LayoutProvider:
  gfx::Insets GetInsetsMetric(int metric) const override;
  int GetDistanceMetric(int metric) const override;
  int GetSnappedDialogWidth(int min_width) const override;
  const views::TypographyProvider& GetTypographyProvider() const override;

  // Returns whether to show the icon next to the title text on a dialog.
  virtual bool ShouldShowWindowIcon() const;

 private:
  const ChromeTypographyProvider typography_provider_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_LAYOUT_PROVIDER_H_
