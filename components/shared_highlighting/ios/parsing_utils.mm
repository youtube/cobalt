// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/shared_highlighting/ios/parsing_utils.h"

#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kCaretWidth = 4.0;
}  // namespace

namespace shared_highlighting {

BOOL IsValidDictValue(const base::Value* value) {
  return value && value->is_dict() && !value->GetDict().empty();
}

absl::optional<CGRect> ParseRect(const base::Value::Dict* dict) {
  if (!dict || dict->empty()) {
    return absl::nullopt;
  }

  absl::optional<double> xValue = dict->FindDouble("x");
  absl::optional<double> yValue = dict->FindDouble("y");
  absl::optional<double> widthValue = dict->FindDouble("width");
  absl::optional<double> heightValue = dict->FindDouble("height");

  if (!xValue || !yValue || !widthValue || !heightValue) {
    return absl::nullopt;
  }

  return CGRectMake(*xValue, *yValue, *widthValue, *heightValue);
}

absl::optional<GURL> ParseURL(const std::string* url_value) {
  if (!url_value) {
    return absl::nullopt;
  }

  GURL url(*url_value);
  if (!url.is_empty() && url.is_valid()) {
    return url;
  }

  return absl::nullopt;
}

CGRect ConvertToBrowserRect(CGRect web_view_rect, web::WebState* web_state) {
  if (CGRectEqualToRect(web_view_rect, CGRectZero) || !web_state) {
    return web_view_rect;
  }

  id<CRWWebViewProxy> web_view_proxy = web_state->GetWebViewProxy();
  CGFloat zoom_scale = web_view_proxy.scrollViewProxy.zoomScale;
  UIEdgeInsets inset = web_view_proxy.scrollViewProxy.contentInset;

  return CGRectMake((web_view_rect.origin.x * zoom_scale) + inset.left,
                    (web_view_rect.origin.y * zoom_scale) + inset.top,
                    (web_view_rect.size.width * zoom_scale) + kCaretWidth,
                    web_view_rect.size.height * zoom_scale);
}

}  // namespace shared_highlighting
