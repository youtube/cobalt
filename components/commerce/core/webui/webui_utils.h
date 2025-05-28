// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEBUI_WEBUI_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_WEBUI_WEBUI_UTILS_H_

#include <memory>

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/mojom/shared.mojom.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"

class GURL;

namespace commerce {

struct ProductInfo;

// Returns a mojo ProductInfo for use in IPC constructed from the shopping
// service's ProductInfo.
shared::mojom::ProductInfoPtr ProductInfoToMojoProduct(
    const GURL& url,
    const std::optional<const ProductInfo>& info,
    const std::string& locale);

shared::mojom::ProductSpecificationsSetPtr ProductSpecsSetToMojo(
    const ProductSpecificationsSet& set);

shared::mojom::BookmarkProductInfoPtr BookmarkNodeToMojoProduct(
    bookmarks::BookmarkModel& model,
    const bookmarks::BookmarkNode* node,
    const std::string& locale);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEBUI_WEBUI_UTILS_H_
