// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_ITEM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

// A LayoutObject subclass for 'display: list-item' in LayoutNG.
class CORE_EXPORT LayoutNGListItem final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGListItem(Element*);

  ListItemOrdinal& Ordinal() {
    NOT_DESTROYED();
    return ordinal_;
  }

  int Value() const;

  LayoutObject* Marker() const {
    NOT_DESTROYED();
    Element* list_item = To<Element>(GetNode());
    return list_item->PseudoElementLayoutObject(kPseudoIdMarker);
  }

  void UpdateMarkerTextIfNeeded();
  void UpdateCounterStyle();

  void OrdinalValueChanged();
  void WillCollectInlines() override;

  static const LayoutObject* FindSymbolMarkerLayoutText(const LayoutObject*);

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGListItem";
  }

 private:
  bool IsOfType(LayoutObjectType) const override;

  void InsertedIntoTree() override;
  void WillBeRemovedFromTree() override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void SubtreeDidChange() final;
  void WillBeDestroyed() override;

  ListItemOrdinal ordinal_;
};

template <>
struct DowncastTraits<LayoutNGListItem> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutNGListItem();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_ITEM_H_
