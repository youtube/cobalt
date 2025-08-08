// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/inner_text_builder.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"

namespace blink {

namespace {

// Returns true if the content of `iframe_element` should be added.
bool ShouldIncludeIFrame(const HTMLIFrameElement& iframe_element) {
  if (iframe_element.IsAdRelated()) {
    return false;
  }
  LocalFrame* iframe_frame =
      DynamicTo<LocalFrame>(iframe_element.ContentFrame());
  if (!iframe_frame || iframe_frame->IsCrossOriginToParentOrOuterDocument()) {
    return false;
  }
  auto* iframe_document = iframe_element.contentDocument();
  if (!iframe_document->body()) {
    return false;
  }
  return true;
}

}  // namespace

// static
mojom::blink::InnerTextFramePtr InnerTextBuilder::Build(
    LocalFrame& frame,
    const mojom::blink::InnerTextParams& params) {
  auto inner_text_frame = mojom::blink::InnerTextFrame::New();
  inner_text_frame->token = frame.GetLocalFrameToken();
  auto* body = frame.GetDocument()->body();
  if (!body) {
    return inner_text_frame;
  }
  HeapVector<Member<ChildIFrame>> child_iframes;
  InnerTextBuilder builder(params, child_iframes);
  builder.Build(*body, *inner_text_frame);
  return inner_text_frame;
}

InnerTextBuilder::InnerTextBuilder(
    const mojom::blink::InnerTextParams& params,
    HeapVector<Member<ChildIFrame>>& child_iframes)
    : params_(params), child_iframes_(child_iframes) {}

void InnerTextBuilder::Build(HTMLElement& body,
                             mojom::blink::InnerTextFrame& frame) {
  String inner_text = body.innerText(this);
  unsigned inner_text_offset = 0;
  for (auto& child_iframe : child_iframes_) {
    const HTMLIFrameElement* iframe_element = child_iframe->iframe;
    if (!ShouldIncludeIFrame(*iframe_element)) {
      continue;
    }
    AddNextNonFrameSegments(inner_text, child_iframe->offset, inner_text_offset,
                            frame);

    LocalFrame* iframe_frame =
        DynamicTo<LocalFrame>(iframe_element->ContentFrame());
    // ShouldIncludeIFrame() only returns true if all of these are true.
    CHECK(iframe_frame);
    auto* iframe_document = iframe_element->contentDocument();
    CHECK(iframe_document);
    CHECK(iframe_document->body());

    mojom::blink::InnerTextFramePtr child_inner_text_frame =
        mojom::blink::InnerTextFrame::New();
    child_inner_text_frame->token = iframe_frame->GetLocalFrameToken();

    HeapVector<Member<ChildIFrame>> child_iframes;
    InnerTextBuilder iframe_builder(params_, child_iframes);
    iframe_builder.Build(*iframe_document->body(), *child_inner_text_frame);
    frame.segments.push_back(mojom::blink::InnerTextSegment::NewFrame(
        std::move(child_inner_text_frame)));
  }
  AddNextNonFrameSegments(inner_text, inner_text.length(), inner_text_offset,
                          frame);
}

void InnerTextBuilder::AddNextNonFrameSegments(
    const String& text,
    unsigned next_child_offset,
    unsigned& text_offset,
    mojom::blink::InnerTextFrame& frame) {
  if (matching_node_location_ &&
      *matching_node_location_ <= next_child_offset) {
    if (text_offset != *matching_node_location_) {
      frame.segments.push_back(mojom::blink::InnerTextSegment::NewText(
          text.Substring(text_offset, *matching_node_location_ - text_offset)));
      text_offset = *matching_node_location_;
    }
    frame.segments.push_back(mojom::blink::InnerTextSegment::NewNodeLocation(
        mojom::blink::NodeLocationType::kStart));
    matching_node_location_.reset();
  }
  if (next_child_offset > text_offset) {
    frame.segments.push_back(mojom::blink::InnerTextSegment::NewText(
        text.Substring(text_offset, next_child_offset - text_offset)));
    text_offset = next_child_offset;
  }
}

void InnerTextBuilder::WillVisit(const Node& element, unsigned offset) {
  if (const auto* iframe = DynamicTo<HTMLIFrameElement>(&element)) {
    auto* child_iframe = MakeGarbageCollected<ChildIFrame>();
    child_iframe->offset = offset;
    child_iframe->iframe = iframe;
    child_iframes_.push_back(child_iframe);
  }
  if (params_.node_id && Node::FromDomNodeId(*params_.node_id) == &element) {
    matching_node_location_ = offset;
  }
}

void InnerTextBuilder::ChildIFrame::Trace(Visitor* visitor) const {
  visitor->Trace(iframe);
}

}  // namespace blink
