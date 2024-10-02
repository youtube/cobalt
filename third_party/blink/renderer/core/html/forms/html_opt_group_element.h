/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_OPT_GROUP_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_OPT_GROUP_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

class HTMLSelectElement;
class HTMLDivElement;

class CORE_EXPORT HTMLOptGroupElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLOptGroupElement(Document&);
  ~HTMLOptGroupElement() override;

  bool IsDisabledFormControl() const override;
  String DefaultToolTip() const override;
  HTMLSelectElement* OwnerSelectElement() const;

  String GroupLabelText() const;
  HTMLDivElement& OptGroupLabelElement() const;

  void ManuallyAssignSlots() override;

  void Trace(Visitor*) const override;

 private:
  bool SupportsFocus() const override;
  void ChildrenChanged(const ChildrenChange& change) override;
  bool ChildrenChangedAllChildrenRemovedNeedsList() const override;
  void ParseAttribute(const AttributeModificationParams&) override;
  void AccessKeyAction(SimulatedClickCreationScope creation_scope) override;
  void DidAddUserAgentShadowRoot(ShadowRoot&) override;
  bool MatchesEnabledPseudoClass() const override;
  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  void UpdateGroupLabel();

  Member<HTMLSlotElement> opt_group_slot_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_OPT_GROUP_ELEMENT_H_
