/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/track/vtt/vtt_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

static const QualifiedName& NodeTypeToTagName(VttNodeType node_type) {
  // Use predefined AtomicStrings in html_names to reduce AtomicString
  // creation cost.
  DEFINE_STATIC_LOCAL(QualifiedName, c_tag, (AtomicString("c")));
  DEFINE_STATIC_LOCAL(QualifiedName, v_tag, (AtomicString("v")));
  DEFINE_STATIC_LOCAL(QualifiedName, lang_tag,
                      (html_names::kLangAttr.LocalName()));
  DEFINE_STATIC_LOCAL(QualifiedName, b_tag, (html_names::kBTag.LocalName()));
  DEFINE_STATIC_LOCAL(QualifiedName, u_tag, (html_names::kUTag.LocalName()));
  DEFINE_STATIC_LOCAL(QualifiedName, i_tag, (html_names::kITag.LocalName()));
  DEFINE_STATIC_LOCAL(QualifiedName, ruby_tag,
                      (html_names::kRubyTag.LocalName()));
  DEFINE_STATIC_LOCAL(QualifiedName, rt_tag, (html_names::kRtTag.LocalName()));
  switch (node_type) {
    case VttNodeType::kClass:
      return c_tag;
    case VttNodeType::kItalic:
      return i_tag;
    case VttNodeType::kLanguage:
      return lang_tag;
    case VttNodeType::kBold:
      return b_tag;
    case VttNodeType::kUnderline:
      return u_tag;
    case VttNodeType::kRuby:
      return ruby_tag;
    case VttNodeType::kRubyText:
      return rt_tag;
    case VttNodeType::kVoice:
      return v_tag;
    case VttNodeType::kNone:
    default:
      NOTREACHED();
  }
}

VTTElement::VTTElement(VttNodeType node_type, Document* document)
    : Element(NodeTypeToTagName(node_type), document, kCreateElement),
      is_past_node_(0),
      vtt_node_type_(static_cast<unsigned>(node_type)) {}

Element& VTTElement::CloneWithoutAttributesAndChildren(
    Document& factory) const {
  auto* clone = MakeGarbageCollected<VTTElement>(
      static_cast<VttNodeType>(vtt_node_type_), &factory);
  clone->SetLanguage(language_);
  clone->SetTrack(track_);
  return *clone;
}

HTMLElement* VTTElement::CreateEquivalentHTMLElement(Document& document) {
  Element* html_element = nullptr;
  switch (GetVttNodeType()) {
    case VttNodeType::kClass:
    case VttNodeType::kLanguage:
    case VttNodeType::kVoice:
      html_element =
          document.CreateRawElement(html_names::kSpanTag, CreateElementFlags());
      html_element->setAttribute(html_names::kTitleAttr,
                                 getAttribute(VoiceAttributeName()));
      html_element->setAttribute(html_names::kLangAttr,
                                 getAttribute(LangAttributeName()));
      break;
    case VttNodeType::kItalic:
      html_element =
          document.CreateRawElement(html_names::kITag, CreateElementFlags());
      break;
    case VttNodeType::kBold:
      html_element =
          document.CreateRawElement(html_names::kBTag, CreateElementFlags());
      break;
    case VttNodeType::kUnderline:
      html_element =
          document.CreateRawElement(html_names::kUTag, CreateElementFlags());
      break;
    case VttNodeType::kRuby:
      html_element =
          document.CreateRawElement(html_names::kRubyTag, CreateElementFlags());
      break;
    case VttNodeType::kRubyText:
      html_element =
          document.CreateRawElement(html_names::kRtTag, CreateElementFlags());
      break;
    default:
      NOTREACHED();
  }

  html_element->setAttribute(html_names::kClassAttr,
                             getAttribute(html_names::kClassAttr));
  return To<HTMLElement>(html_element);
}

void VTTElement::SetIsPastNode(bool is_past_node) {
  if (!!is_past_node_ == is_past_node)
    return;

  is_past_node_ = is_past_node;
  SetNeedsStyleRecalc(
      kLocalStyleChange,
      StyleChangeReasonForTracing::CreateWithExtraData(
          style_change_reason::kPseudoClass, style_change_extra_data::g_past));
}

void VTTElement::SetTrack(TextTrack* track) {
  track_ = track;
}

void VTTElement::Trace(Visitor* visitor) const {
  visitor->Trace(track_);
  Element::Trace(visitor);
}

}  // namespace blink
