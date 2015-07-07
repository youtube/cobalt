/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/layout/box_generator.h"

#include <string>

#include "base/bind.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_media_element.h"
#include "cobalt/dom/text.h"
#include "cobalt/layout/block_formatting_block_container_box.h"
#include "cobalt/layout/computed_style.h"
#include "cobalt/layout/inline_container_box.h"
#include "cobalt/layout/replaced_box.h"
#include "cobalt/layout/text_box.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/layout/white_space_processing.h"
#include "cobalt/render_tree/image.h"
#include "media/base/shell_video_frame_provider.h"

namespace cobalt {
namespace layout {

using ::media::ShellVideoFrameProvider;
using ::media::VideoFrame;

namespace {

scoped_refptr<render_tree::Image> GetVideoFrame(
    ShellVideoFrameProvider* frame_provider) {
  scoped_refptr<VideoFrame> video_frame =
      frame_provider ? frame_provider->GetCurrentFrame() : NULL;
  if (video_frame && video_frame->texture_id()) {
    scoped_refptr<render_tree::Image> image =
        reinterpret_cast<render_tree::Image*>(video_frame->texture_id());
    return image;
  }

  return NULL;
}

}  // namespace

BoxGenerator::BoxGenerator(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>&
        parent_computed_style,
    const UsedStyleProvider* used_style_provider,
    icu::BreakIterator* line_break_iterator)
    : parent_computed_style_(parent_computed_style),
      used_style_provider_(used_style_provider),
      line_break_iterator_(line_break_iterator) {}

BoxGenerator::~BoxGenerator() {}

namespace {

class ContainerBoxGenerator : public cssom::NotReachedPropertyValueVisitor {
 public:
  ContainerBoxGenerator(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider)
      : computed_style_(computed_style),
        transitions_(transitions),
        used_style_provider_(used_style_provider) {}

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;

  scoped_ptr<ContainerBox> PassContainerBox() { return container_box_.Pass(); }

 private:
  const scoped_refptr<const cssom::CSSStyleDeclarationData> computed_style_;
  const cssom::TransitionSet* transitions_;
  const UsedStyleProvider* const used_style_provider_;

  scoped_ptr<ContainerBox> container_box_;
};

void ContainerBoxGenerator::VisitKeyword(cssom::KeywordValue* keyword) {
  // See http://www.w3.org/TR/CSS21/visuren.html#display-prop.
  switch (keyword->value()) {
    // Generate a block-level block container box.
    case cssom::KeywordValue::kBlock:
      container_box_ = make_scoped_ptr(new BlockLevelBlockContainerBox(
          computed_style_, transitions_, used_style_provider_));
      break;
    // Generate one or more inline boxes. Note that more inline boxes may be
    // generated when the original inline box is split due to participation
    // in the formatting context.
    case cssom::KeywordValue::kInline:
      container_box_ = make_scoped_ptr(new InlineContainerBox(
          computed_style_, transitions_, used_style_provider_));
      break;
    // Generate an inline-level block container box. The inside of
    // an inline-block is formatted as a block box, and the element itself
    // is formatted as an atomic inline-level box.
    case cssom::KeywordValue::kInlineBlock:
      container_box_ = make_scoped_ptr(new InlineLevelBlockContainerBox(
          computed_style_, transitions_, used_style_provider_));
      break;
    // The element generates no boxes and has no effect on layout.
    case cssom::KeywordValue::kNone:
      // Leave |container_box_| NULL.
      break;
    case cssom::KeywordValue::kAbsolute:
    case cssom::KeywordValue::kAuto:
    case cssom::KeywordValue::kBaseline:
    case cssom::KeywordValue::kCenter:
    case cssom::KeywordValue::kContain:
    case cssom::KeywordValue::kCover:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kLeft:
    case cssom::KeywordValue::kMiddle:
    case cssom::KeywordValue::kNormal:
    case cssom::KeywordValue::kRelative:
    case cssom::KeywordValue::kRight:
    case cssom::KeywordValue::kStatic:
    case cssom::KeywordValue::kTop:
    case cssom::KeywordValue::kVisible:
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

void BoxGenerator::Visit(dom::Element* element) {
  // Update and cache computed values of the given HTML element.
  scoped_refptr<dom::HTMLElement> html_element = element->AsHTMLElement();
  DCHECK(html_element);

  ContainerBoxGenerator container_box_generator(html_element->computed_style(),
                                                html_element->transitions(),
                                                used_style_provider_);
  html_element->computed_style()->display()->Accept(&container_box_generator);
  scoped_ptr<ContainerBox> container_box_before_split =
      container_box_generator.PassContainerBox();
  if (container_box_before_split == NULL) {
    // The element with "display: none" generates no boxes and has no effect
    // on layout. Descendant elements do not generate any boxes either.
    // This behavior cannot be overridden by setting the "display" property on
    // the descendants.
    //   http://www.w3.org/TR/CSS21/visuren.html#display-prop
    return;
  }

  // For video elements, immediately create a replaced box and attach it to
  // the container box and return.  Thus, there will be two boxes associated
  // with every video, the ReplacedBox referencing the video, and the container
  // box referencing the ReplacedBox.  Keeping the container around is useful
  // because it allows ReplacedBox to re-use the layout logic that the container
  // box implements.
  scoped_refptr<dom::HTMLMediaElement> media_element =
      html_element->AsHTMLMediaElement();
  if (media_element) {
    VisitMediaElement(media_element, container_box_before_split.Pass());
  } else {
    VisitContainerElement(html_element, container_box_before_split.Pass());
  }
}

void BoxGenerator::VisitMediaElement(
    dom::HTMLMediaElement* media_element,
    scoped_ptr<ContainerBox> first_container_box) {
  scoped_ptr<Box> replaced_box = make_scoped_ptr<Box>(new ReplacedBox(
      first_container_box->computed_style(),
      cssom::TransitionSet::EmptyTransitionSet(),
      base::Bind(GetVideoFrame, media_element->GetVideoFrameProvider()),
      used_style_provider_));
  bool added = first_container_box->TryAddChild(&replaced_box);
  DCHECK(added);
  boxes_.push_back(first_container_box.release());
}

void BoxGenerator::VisitContainerElement(
    dom::HTMLElement* html_element,
    scoped_ptr<ContainerBox> first_container_box) {
  boxes_.push_back(first_container_box.release());

  // Generate child boxes.
  for (scoped_refptr<dom::Node> child_node = html_element->first_child();
       child_node; child_node = child_node->next_sibling()) {
    BoxGenerator child_box_generator(html_element->computed_style(),
                                     used_style_provider_,
                                     line_break_iterator_);
    child_node->Accept(&child_box_generator);

    Boxes child_boxes = child_box_generator.PassBoxes();
    for (Boxes::iterator child_box_iterator = child_boxes.begin();
         child_box_iterator != child_boxes.end(); ++child_box_iterator) {
      // Transfer the ownership of the child box from |ScopedVector|
      // to |scoped_ptr|.
      scoped_ptr<Box> child_box(*child_box_iterator);
      *child_box_iterator = NULL;

      // When an inline box contains an in-flow block-level box, the inline box
      // (and its inline ancestors within the same block container box*) are
      // broken around the block-level box, splitting the inline box into two
      // boxes (even if either side is empty), one on each side of
      // the block-level box. The line boxes before the break and after
      // the break are enclosed in anonymous block boxes, and the block-level
      // box becomes a sibling of those anonymous boxes.
      //   http://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level
      //
      // * CSS 2.1 says "the same line box" but line boxes are not real boxes
      //   in Cobalt, see |LineBox| for details.
      ContainerBox* last_container_box =
          base::polymorphic_downcast<ContainerBox*>(boxes_.back());

      if (!last_container_box->TryAddChild(&child_box)) {
        boxes_.push_back(child_box.release());

        scoped_ptr<ContainerBox> next_container_box =
            last_container_box->TrySplitAtEnd();
        DCHECK(next_container_box);
        boxes_.push_back(next_container_box.release());
      }
    }
  }
}

void BoxGenerator::Visit(dom::Comment* /*comment*/) {}

void BoxGenerator::Visit(dom::Document* /*document*/) { NOTREACHED(); }

// Split the text node into non-breakable segments at soft wrap opportunities
// (http://www.w3.org/TR/css-text-3/#soft-wrap-opportunity) according to
// the Unicode line breaking algorithm (http://www.unicode.org/reports/tr14/).
// Each non-breakable segment (a word or a part of the word) becomes a text box.
// An actual line breaking happens during the layout, at the edge of text boxes.
//
// TODO(***REMOVED***): Measure the performance of the algorithm on Chinese
//               and Japanese where almost every character presents a soft wrap
//               opportunity.
void BoxGenerator::Visit(dom::Text* text) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style =
      GetComputedStyleOfAnonymousBox(parent_computed_style_);

  // Phase I: Collapsing and Transformation
  //   http://www.w3.org/TR/css3-text/#white-space-phase-1
  // TODO(***REMOVED***): Implement "white-space: pre".
  //               http://www.w3.org/TR/css3-text/#white-space
  std::string collapsed_text = text->text();
  CollapseWhiteSpace(&collapsed_text);

  // TODO(***REMOVED***): Implement "white-space: nowrap".
  //               http://www.w3.org/TR/css3-text/#white-space
  icu::UnicodeString collapsed_text_as_unicode_string =
      icu::UnicodeString::fromUTF8(collapsed_text);
  line_break_iterator_->setText(collapsed_text_as_unicode_string);

  for (int32 segment_end = line_break_iterator_->first(), segment_start = 0;
       segment_end != icu::BreakIterator::DONE;
       segment_end = line_break_iterator_->next()) {
    if (segment_end == 0) {
      continue;
    }

    icu::UnicodeString non_breakable_segment =
        collapsed_text_as_unicode_string.tempSubStringBetween(segment_start,
                                                              segment_end);
    segment_start = segment_end;

    // TODO(***REMOVED***): Transform the letter case:
    //               http://www.w3.org/TR/css-text-3/#text-transform-property

    // TODO(***REMOVED***): Implement "white-space: pre".
    //               http://www.w3.org/TR/css3-text/#white-space
    std::string trimmed_non_breakable_segment;
    bool has_leading_white_space;
    bool has_trailing_white_space;
    non_breakable_segment.toUTF8String(trimmed_non_breakable_segment);
    TrimWhiteSpace(&trimmed_non_breakable_segment, &has_leading_white_space,
                   &has_trailing_white_space);

    // TODO(***REMOVED***): Determine which transitions to propagate to the text box,
    //               instead of none at all.
    boxes_.push_back(
        new TextBox(computed_style, cssom::TransitionSet::EmptyTransitionSet(),
                    trimmed_non_breakable_segment, has_leading_white_space,
                    has_trailing_white_space, used_style_provider_));
  }
}

BoxGenerator::Boxes BoxGenerator::PassBoxes() { return boxes_.Pass(); }

}  // namespace layout
}  // namespace cobalt
