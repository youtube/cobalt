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

#include "base/bind.h"
#include "cobalt/cssom/character_classification.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_media_element.h"
#include "cobalt/dom/text.h"
#include "cobalt/layout/block_formatting_block_container_box.h"
#include "cobalt/layout/computed_style.h"
#include "cobalt/layout/html_elements.h"
#include "cobalt/layout/inline_container_box.h"
#include "cobalt/layout/replaced_box.h"
#include "cobalt/layout/text_box.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/render_tree/image.h"
#include "media/base/shell_video_frame_provider.h"
#include "third_party/icu/public/common/unicode/schriter.h"
#include "third_party/icu/public/common/unicode/unistr.h"

namespace cobalt {
namespace layout {

using ::media::ShellVideoFrameProvider;
using ::media::VideoFrame;

namespace {

scoped_refptr<render_tree::Image> GetVideoFrame(
    ShellVideoFrameProvider* frame_provider) {
  scoped_refptr<VideoFrame> video_frame = frame_provider->GetCurrentFrame();
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
    const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet,
    const UsedStyleProvider* used_style_provider)
    : parent_computed_style_(parent_computed_style),
      user_agent_style_sheet_(user_agent_style_sheet),
      used_style_provider_(used_style_provider) {}

namespace {

class ContainerBoxGenerator : public cssom::NotReachedPropertyValueVisitor {
 public:
  ContainerBoxGenerator(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions)
      : computed_style_(computed_style), transitions_(transitions) {}

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;

  scoped_ptr<ContainerBox> PassContainerBox() { return container_box_.Pass(); }

 private:
  const scoped_refptr<const cssom::CSSStyleDeclarationData> computed_style_;
  const cssom::TransitionSet* transitions_;

  scoped_ptr<ContainerBox> container_box_;
};

void ContainerBoxGenerator::VisitKeyword(cssom::KeywordValue* keyword) {
  // See http://www.w3.org/TR/CSS21/visuren.html#display-prop.
  switch (keyword->value()) {
    // Generate a block-level block container box.
    case cssom::KeywordValue::kBlock:
      container_box_ =
          make_scoped_ptr(new BlockLevelBlockContainerBox(computed_style_,
                                                          transitions_));
      break;
    // Generate one or more inline boxes. Note that more inline boxes may be
    // generated when the original inline box is split due to participation
    // in the formatting context.
    case cssom::KeywordValue::kInline:
      container_box_ = make_scoped_ptr(new InlineContainerBox(computed_style_,
                                                              transitions_));
      break;
    // Generate an inline-level block container box. The inside of
    // an inline-block is formatted as a block box, and the element itself
    // is formatted as an atomic inline-level box.
    case cssom::KeywordValue::kInlineBlock:
      container_box_ =
          make_scoped_ptr(new InlineLevelBlockContainerBox(computed_style_,
                                                           transitions_));
      break;
    // The element generates no boxes and has no effect on layout.
    case cssom::KeywordValue::kNone:
      // Leave |container_box_| NULL.
      break;
    case cssom::KeywordValue::kAuto:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kNormal:
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
  DCHECK_NE(scoped_refptr<dom::HTMLElement>(), html_element);
  UpdateComputedStyleOf(html_element, parent_computed_style_,
                        user_agent_style_sheet_);

  ContainerBoxGenerator container_box_generator(html_element->computed_style(),
                                                html_element->transitions());
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

  scoped_refptr<dom::HTMLMediaElement> media_element =
      html_element->AsHTMLMediaElement();
  if (media_element && media_element->GetVideoFrameProvider()) {
    scoped_ptr<Box> replaced_box = make_scoped_ptr<Box>(new ReplacedBox(
        container_box_before_split->computed_style(),
        cssom::TransitionSet::EmptyTransitionSet(),
        base::Bind(GetVideoFrame, media_element->GetVideoFrameProvider())));
    bool added = container_box_before_split->TryAddChild(&replaced_box);
    DCHECK(added);
    boxes_.push_back(container_box_before_split.release());
    return;
  }

  boxes_.push_back(container_box_before_split.release());

  // Generate child boxes.
  for (scoped_refptr<dom::Node> child_node = html_element->first_child();
       child_node; child_node = child_node->next_sibling()) {
    BoxGenerator child_box_generator(html_element->computed_style(),
                                     user_agent_style_sheet_,
                                     used_style_provider_);
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
        DCHECK_NE(static_cast<ContainerBox*>(NULL), next_container_box.get());
        boxes_.push_back(next_container_box.release());
      }
    }
  }
}

void BoxGenerator::Visit(dom::Comment* /*comment*/) {}

void BoxGenerator::Visit(dom::Document* /*document*/) { NOTREACHED(); }

namespace {

bool IsWhiteSpace(UChar32 character) {
  return character <= std::numeric_limits<char>::max() &&
         cssom::IsWhiteSpace(static_cast<char>(character));
}

// Returns true if skipped at least one white space character.
bool SkipWhiteSpace(icu::StringCharacterIterator* text_iterator) {
  bool skipped_at_least_one = false;
  while (text_iterator->hasNext()) {
    if (!IsWhiteSpace(text_iterator->next32PostInc())) {
      text_iterator->previous32();
      break;
    }
    skipped_at_least_one = true;
  }
  return skipped_at_least_one;
}

// Returns true if appended at least one character that is not a white space.
bool AppendNonWhiteSpace(icu::StringCharacterIterator* text_iterator,
                         icu::UnicodeString* output_string) {
  bool appended_at_least_one = false;
  while (text_iterator->hasNext()) {
    UChar32 character = text_iterator->next32PostInc();
    if (IsWhiteSpace(character)) {
      text_iterator->previous32();
      break;
    }
    appended_at_least_one = true;
    output_string->append(character);
  }
  return appended_at_least_one;
}

// TODO(***REMOVED***): Write unit tests for a white space processing.

// Performs white space collapsing and transformation that correspond to
// the phase I of the white space processing. Also trims the white space in
// a preparation for the phase II which happens during a layout.
//   http://www.w3.org/TR/css3-text/#white-space-phase-1
//   http://www.w3.org/TR/css3-text/#white-space-phase-2
void CollapseAndTrimWhiteSpace(const std::string& text_in_utf8,
                               std::string* collapsed_and_trimmed_text_in_utf8,
                               bool* has_leading_white_space,
                               bool* has_trailing_white_space) {
  icu::UnicodeString text = icu::UnicodeString::fromUTF8(text_in_utf8);
  icu::StringCharacterIterator text_iterator(text);
  text_iterator.setToStart();

  icu::UnicodeString collapsed_and_trimmed_text;
  *has_leading_white_space = SkipWhiteSpace(&text_iterator);
  *has_trailing_white_space = false;

  while (true) {
    if (!AppendNonWhiteSpace(&text_iterator, &collapsed_and_trimmed_text)) {
      break;
    }

    // Per the specification, any space immediately following another
    // collapsible space is collapsed to have zero advance width. We approximate
    // this by replacing adjacent spaces with a single space.
    if (!SkipWhiteSpace(&text_iterator)) {
      break;
    }
    if (!text_iterator.hasNext()) {
      *has_trailing_white_space = true;
      break;
    }
    collapsed_and_trimmed_text.append(' ');
  }

  if (collapsed_and_trimmed_text.isEmpty()) {
    *has_trailing_white_space = *has_leading_white_space;
  }
  collapsed_and_trimmed_text.toUTF8String(*collapsed_and_trimmed_text_in_utf8);
}

}  // namespace

void BoxGenerator::Visit(dom::Text* text) {
  // TODO(***REMOVED***): Implement "white-space: pre".
  //               http://www.w3.org/TR/css3-text/#white-space
  std::string collapsed_and_trimmed_text;
  bool has_leading_white_space;
  bool has_trailing_white_space;
  CollapseAndTrimWhiteSpace(text->text(), &collapsed_and_trimmed_text,
                            &has_leading_white_space,
                            &has_trailing_white_space);
  DCHECK(!collapsed_and_trimmed_text.empty() ||
         (has_leading_white_space && has_trailing_white_space));
  // TODO(***REMOVED***): Transform the letter case:
  //               http://www.w3.org/TR/css-text-3/#text-transform-property

  // TODO(***REMOVED***): Consider caching the style of anonymous box and reuse it with
  // siblings.
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style =
      GetComputedStyleOfAnonymousBox(parent_computed_style_);

  scoped_refptr<render_tree::Font> used_font =
      used_style_provider_->GetUsedFont(computed_style->font_family(),
                                        computed_style->font_size());

  // TODO(***REMOVED***): Determine which transitions to propogate to the text box,
  //               instead of none at all.
  boxes_.push_back(
      new TextBox(computed_style, cssom::TransitionSet::EmptyTransitionSet(),
                  collapsed_and_trimmed_text, has_leading_white_space,
                  has_trailing_white_space, used_font));
}

}  // namespace layout
}  // namespace cobalt
