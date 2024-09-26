/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_OBJECT_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_OBJECT_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/forms/form_associated.h"
#include "third_party/blink/renderer/core/html/forms/listed_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"

namespace blink {

class HTMLFormElement;

// Inheritance of ListedElement was used for NPAPI form association, but
// is still kept here so that legacy APIs such as form attribute can keep
// working according to the spec.  See:
// https://html.spec.whatwg.org/C/#the-object-element
class CORE_EXPORT HTMLObjectElement final : public HTMLPlugInElement,
                                            public ListedElement,
                                            public FormAssociated {
  DEFINE_WRAPPERTYPEINFO();

 public:
  HTMLObjectElement(Document&, const CreateElementFlags);
  ~HTMLObjectElement() override = default;
  void Trace(Visitor*) const override;

  // Returns attributes that should be checked against Trusted Types
  const AttrNameToTrustedType& GetCheckedAttributeTypes() const override;

  const String& ClassId() const { return class_id_; }

  HTMLFormElement* formOwner() const override;

  bool ContainsJavaApplet() const;

  bool HasFallbackContent() const override;
  bool UseFallbackContent() const override;

  bool IsFormControlElement() const override { return false; }

  bool IsEnumeratable() const override { return true; }

  bool ChildrenCanHaveStyle() const override { return UseFallbackContent(); }

  FrameOwnerElementType OwnerType() const final {
    return FrameOwnerElementType::kObject;
  }

  // Implementations of constraint validation API.
  // Note that the object elements are always barred from constraint validation.
  String validationMessage() const override { return String(); }
  bool checkValidity() { return true; }
  bool reportValidity() { return true; }
  void setCustomValidity(const String&) override {}

  bool CanContainRangeEndPoint() const override { return UseFallbackContent(); }

  bool IsExposed() const;

  bool WillUseFallbackContentAtLayout() const;

  FormAssociated* ToFormAssociatedOrNull() override { return this; }
  void AssociateWith(HTMLFormElement*) override;

  // Returns true if this object started to load something, and finished
  // the loading regardless of success or failure.
  bool DidFinishLoading() const;

  enum class ErrorEventPolicy {
    kDoNotDispatch,
    kDispatch,
  };
  void RenderFallbackContent(ErrorEventPolicy should_dispatch_error_event);

  static bool IsClassOf(const FrameOwner& owner);

 private:
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;

  void DidMoveToNewDocument(Document& old_document) override;

  void ChildrenChanged(const ChildrenChange&) override;

  bool IsURLAttribute(const Attribute&) const override;
  bool HasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& SubResourceAttributeName() const override;
  const AtomicString ImageSourceURL() const override;

  LayoutEmbeddedContent* ExistingLayoutEmbeddedContent() const override;

  void UpdatePluginInternal() override;
  void UpdateDocNamedItem();

  void ReattachFallbackContent();

  // FIXME: This function should not deal with url or serviceType
  // so that we can better share code between <object> and <embed>.
  void ParametersForPlugin(PluginParameters& plugin_params);

  bool HasValidClassId() const;

  void ReloadPluginOnAttributeChange(const QualifiedName&);

  NamedItemType GetNamedItemType() const override {
    return NamedItemType::kNameOrId;
  }

  int DefaultTabIndex() const override;

  String class_id_;
  bool use_fallback_content_ : 1;
};

// Like To<HTMLObjectElement>() but accepts a ListedElement as input
// instead of a Node.
const HTMLObjectElement* ToHTMLObjectElementFromListedElement(
    const ListedElement*);
const HTMLObjectElement& ToHTMLObjectElementFromListedElement(
    const ListedElement&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_OBJECT_ELEMENT_H_
