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
#include "cobalt/cssom/computed_style.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_video_element.h"
#include "cobalt/dom/text.h"
#include "cobalt/layout/block_formatting_block_container_box.h"
#include "cobalt/layout/block_level_replaced_box.h"
#include "cobalt/layout/inline_container_box.h"
#include "cobalt/layout/inline_level_replaced_box.h"
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
    icu::BreakIterator* line_break_iterator,
    scoped_refptr<Paragraph>* paragraph)
    : parent_computed_style_(parent_computed_style),
      used_style_provider_(used_style_provider),
      line_break_iterator_(line_break_iterator),
      paragraph_(paragraph) {}

BoxGenerator::~BoxGenerator() {}

void BoxGenerator::Visit(dom::Element* element) {
  scoped_refptr<dom::HTMLElement> html_element = element->AsHTMLElement();
  DCHECK(html_element);

  scoped_refptr<dom::HTMLVideoElement> video_element =
      html_element->AsHTMLVideoElement();
  if (video_element) {
    VisitVideoElement(video_element);
  } else {
    VisitNonReplacedElement(html_element);
  }
}

namespace {

class ReplacedBoxGenerator : public cssom::NotReachedPropertyValueVisitor {
 public:
  ReplacedBoxGenerator(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider,
      const ReplacedBox::ReplaceImageCB& replace_image_cb,
      const scoped_refptr<Paragraph>& paragraph, int32 text_position,
      const base::optional<float>& maybe_intrinsic_width,
      const base::optional<float>& maybe_intrinsic_height,
      const base::optional<float>& maybe_intrinsic_ratio)
      : computed_style_(computed_style),
        transitions_(transitions),
        used_style_provider_(used_style_provider),
        replace_image_cb_(replace_image_cb),
        paragraph_(paragraph),
        text_position_(text_position),
        maybe_intrinsic_width_(maybe_intrinsic_width),
        maybe_intrinsic_height_(maybe_intrinsic_height),
        maybe_intrinsic_ratio_(maybe_intrinsic_ratio) {}

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;

  scoped_ptr<ReplacedBox> PassReplacedBox() { return replaced_box_.Pass(); }

 private:
  const scoped_refptr<const cssom::CSSStyleDeclarationData> computed_style_;
  const cssom::TransitionSet* transitions_;
  const UsedStyleProvider* const used_style_provider_;
  const ReplacedBox::ReplaceImageCB replace_image_cb_;
  const scoped_refptr<Paragraph> paragraph_;
  const int32 text_position_;
  const base::optional<float> maybe_intrinsic_width_;
  const base::optional<float> maybe_intrinsic_height_;
  const base::optional<float> maybe_intrinsic_ratio_;

  scoped_ptr<ReplacedBox> replaced_box_;
};

void ReplacedBoxGenerator::VisitKeyword(cssom::KeywordValue* keyword) {
  // See http://www.w3.org/TR/CSS21/visuren.html#display-prop.
  switch (keyword->value()) {
    // Generate a block-level replaced box.
    case cssom::KeywordValue::kBlock:
      replaced_box_ = make_scoped_ptr(new BlockLevelReplacedBox(
          computed_style_, transitions_, used_style_provider_,
          replace_image_cb_, paragraph_, text_position_, maybe_intrinsic_width_,
          maybe_intrinsic_height_, maybe_intrinsic_ratio_));
      break;
    // Generate an inline-level replaced box. There is no need to distinguish
    // between inline replaced elements and inline-block replaced elements
    // because their widths, heights, and margins are calculated in the same
    // way.
    case cssom::KeywordValue::kInline:
    case cssom::KeywordValue::kInlineBlock:
      replaced_box_ = make_scoped_ptr(new InlineLevelReplacedBox(
          computed_style_, transitions_, used_style_provider_,
          replace_image_cb_, paragraph_, text_position_, maybe_intrinsic_width_,
          maybe_intrinsic_height_, maybe_intrinsic_ratio_));
      break;
    // The element generates no boxes and has no effect on layout.
    case cssom::KeywordValue::kNone:
      // Leave |replaced_box_| NULL.
      break;
    case cssom::KeywordValue::kAbsolute:
    case cssom::KeywordValue::kAuto:
    case cssom::KeywordValue::kBaseline:
    case cssom::KeywordValue::kBreakWord:
    case cssom::KeywordValue::kCenter:
    case cssom::KeywordValue::kClip:
    case cssom::KeywordValue::kContain:
    case cssom::KeywordValue::kCover:
    case cssom::KeywordValue::kCursive:
    case cssom::KeywordValue::kEllipsis:
    case cssom::KeywordValue::kFantasy:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kLeft:
    case cssom::KeywordValue::kMiddle:
    case cssom::KeywordValue::kMonospace:
    case cssom::KeywordValue::kNoRepeat:
    case cssom::KeywordValue::kNormal:
    case cssom::KeywordValue::kNoWrap:
    case cssom::KeywordValue::kPre:
    case cssom::KeywordValue::kRelative:
    case cssom::KeywordValue::kRepeat:
    case cssom::KeywordValue::kRight:
    case cssom::KeywordValue::kSansSerif:
    case cssom::KeywordValue::kSerif:
    case cssom::KeywordValue::kStatic:
    case cssom::KeywordValue::kTop:
    case cssom::KeywordValue::kUppercase:
    case cssom::KeywordValue::kVisible:
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

void BoxGenerator::VisitVideoElement(dom::HTMLVideoElement* video_element) {
  // For video elements, create a replaced box.

  // A replaced box is formatted as an atomic inline element. It is treated
  // directionally as a neutral character and its line breaking behavior is
  // equivalent to that of the Object Replacement Character.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-boxes
  //   http://www.w3.org/TR/CSS21/visuren.html#propdef-unicode-bidi
  //   http://www.w3.org/TR/css3-text/#line-break-details
  int32 text_position =
      (*paragraph_)->AppendCodePoint(
          Paragraph::kObjectReplacementCharacterCodePoint);

  // Unlike in Chromium, we do not set the intrinsic width, height, or ratio
  // based on the video frame. This allows to avoid relayout while playing
  // adaptive videos.
  ReplacedBoxGenerator replaced_box_generator(
      video_element->computed_style(), video_element->transitions(),
      used_style_provider_,
      base::Bind(GetVideoFrame, video_element->GetVideoFrameProvider()),
      *paragraph_, text_position, base::nullopt, base::nullopt, base::nullopt);
  video_element->computed_style()->display()->Accept(&replaced_box_generator);

  scoped_ptr<ReplacedBox> replaced_box =
      replaced_box_generator.PassReplacedBox();
  if (replaced_box == NULL) {
    // The element with "display: none" generates no boxes and has no effect
    // on layout. Descendant elements do not generate any boxes either.
    // This behavior cannot be overridden by setting the "display" property on
    // the descendants.
    //   http://www.w3.org/TR/CSS21/visuren.html#display-prop
    return;
  }

#ifdef COBALT_BOX_DUMP_ENABLED
  replaced_box->SetGeneratingNode(video_element);
#endif  // COBALT_BOX_DUMP_ENABLED

  boxes_.push_back(replaced_box.release());

  // The content of replaced elements is not considered in the CSS rendering
  // model.
  //   http://www.w3.org/TR/CSS21/conform.html#replaced-element
}

namespace {

class ContainerBoxGenerator : public cssom::NotReachedPropertyValueVisitor {
 public:
  enum CloseParagraph {
    kDoNotCloseParagraph,
    kCloseParagraph,
  };

  ContainerBoxGenerator(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
      const cssom::TransitionSet* transitions,
      const UsedStyleProvider* used_style_provider,
      icu::BreakIterator* line_break_iterator,
      scoped_refptr<Paragraph>* paragraph)
      : computed_style_(computed_style),
        transitions_(transitions),
        used_style_provider_(used_style_provider),
        line_break_iterator_(line_break_iterator),
        paragraph_(paragraph),
        paragraph_scoped_(false) {}
  ~ContainerBoxGenerator();

  void VisitKeyword(cssom::KeywordValue* keyword) OVERRIDE;

  scoped_ptr<ContainerBox> PassContainerBox() { return container_box_.Pass(); }

 private:
  void CreateScopedParagraph(CloseParagraph close_prior_paragraph);

  const scoped_refptr<const cssom::CSSStyleDeclarationData> computed_style_;
  const cssom::TransitionSet* transitions_;
  const UsedStyleProvider* const used_style_provider_;
  icu::BreakIterator* const line_break_iterator_;

  scoped_refptr<Paragraph>* paragraph_;
  scoped_refptr<Paragraph> prior_paragraph_;
  bool paragraph_scoped_;

  scoped_ptr<ContainerBox> container_box_;
};

ContainerBoxGenerator::~ContainerBoxGenerator() {
  if (paragraph_scoped_) {
    (*paragraph_)->Close();

    // If the prior paragraph was closed, then replace it with a new paragraph
    // that has the same direction as the previous one. Otherwise, restore the
    // prior one.
    if (prior_paragraph_ && prior_paragraph_->IsClosed()) {
      *paragraph_ = new Paragraph(line_break_iterator_,
                                  prior_paragraph_->GetBaseDirection());
    } else {
      *paragraph_ = prior_paragraph_;
    }
  }
}

void ContainerBoxGenerator::VisitKeyword(cssom::KeywordValue* keyword) {
  // See http://www.w3.org/TR/CSS21/visuren.html#display-prop.
  switch (keyword->value()) {
    // Generate a block-level block container box.
    case cssom::KeywordValue::kBlock:
      container_box_ = make_scoped_ptr(new BlockLevelBlockContainerBox(
          computed_style_, transitions_, used_style_provider_));

      // The block ends the current paragraph and begins a new one that ends
      // with the block, so close the current paragraph, and create a new
      // paragraph that will close when the container box generator is
      // destroyed.
      CreateScopedParagraph(kCloseParagraph);
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
    //   http://www.w3.org/TR/CSS21/visuren.html#inline-boxes
    case cssom::KeywordValue::kInlineBlock: {
      // An inline block is is treated directionally as a neutral character and
      // its line breaking behavior is equivalent to that of the Object
      // Replacement Character.
      //   http://www.w3.org/TR/CSS21/visuren.html#propdef-unicode-bidi
      //   http://www.w3.org/TR/css3-text/#line-break-details
      int32 text_position =
          (*paragraph_)->AppendCodePoint(
              Paragraph::kObjectReplacementCharacterCodePoint);

      container_box_ = make_scoped_ptr(new InlineLevelBlockContainerBox(
          computed_style_, transitions_, used_style_provider_, *paragraph_,
          text_position));

      // The inline block creates a new paragraph, which the old paragraph
      // flows around. Create a new paragraph, which will close with the end
      // of the inline block. However, do not close the old paragraph, because
      // it will continue once the scope of the inline block ends.
      CreateScopedParagraph(kDoNotCloseParagraph);
    } break;
    // The element generates no boxes and has no effect on layout.
    case cssom::KeywordValue::kNone:
      // Leave |container_box_| NULL.
      break;
    case cssom::KeywordValue::kAbsolute:
    case cssom::KeywordValue::kAuto:
    case cssom::KeywordValue::kBaseline:
    case cssom::KeywordValue::kBreakWord:
    case cssom::KeywordValue::kCenter:
    case cssom::KeywordValue::kContain:
    case cssom::KeywordValue::kCover:
    case cssom::KeywordValue::kCursive:
    case cssom::KeywordValue::kClip:
    case cssom::KeywordValue::kEllipsis:
    case cssom::KeywordValue::kFantasy:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kLeft:
    case cssom::KeywordValue::kMiddle:
    case cssom::KeywordValue::kMonospace:
    case cssom::KeywordValue::kNoRepeat:
    case cssom::KeywordValue::kNormal:
    case cssom::KeywordValue::kNoWrap:
    case cssom::KeywordValue::kPre:
    case cssom::KeywordValue::kRelative:
    case cssom::KeywordValue::kRepeat:
    case cssom::KeywordValue::kRight:
    case cssom::KeywordValue::kSansSerif:
    case cssom::KeywordValue::kSerif:
    case cssom::KeywordValue::kStatic:
    case cssom::KeywordValue::kTop:
    case cssom::KeywordValue::kUppercase:
    case cssom::KeywordValue::kVisible:
    default:
      NOTREACHED();
      break;
  }
}

void ContainerBoxGenerator::CreateScopedParagraph(
    CloseParagraph close_prior_paragraph) {
  DCHECK(!paragraph_scoped_);

  paragraph_scoped_ = true;
  prior_paragraph_ = *paragraph_;

  if (prior_paragraph_ && close_prior_paragraph == kCloseParagraph) {
    prior_paragraph_->Close();
  }

  // TODO(***REMOVED***): Use the container box's style to determine the correct base
  // direction for the paragraph.
  *paragraph_ =
      new Paragraph(line_break_iterator_, Paragraph::kLeftToRightBaseDirection);
}

}  // namespace

void BoxGenerator::AppendChildBoxToLine(Box* child_box_ptr) {
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

  scoped_ptr<Box> child_box(child_box_ptr);
  if (!last_container_box->TryAddChild(&child_box)) {
    boxes_.push_back(child_box.release());

    scoped_ptr<ContainerBox> next_container_box =
        last_container_box->TrySplitAtEnd();
    DCHECK(next_container_box);
    boxes_.push_back(next_container_box.release());
  }
}

namespace {

class ContentProvider : public cssom::NotReachedPropertyValueVisitor {
 public:
  ContentProvider() {}

  const std::string& content_string() const { return content_string_; }

  void VisitString(cssom::StringValue* string_value) OVERRIDE {
    content_string_ = string_value->value();
  }

 private:
  std::string content_string_;
};

}  // namespace

void BoxGenerator::AppendPseudoElementToLine(
    dom::HTMLElement* html_element,
    dom::PseudoElementType pseudo_element_type) {
  // Add boxes with generated content from :before or :after pseudo elements to
  // the line.
  //   http://www.w3.org/TR/CSS21/generate.html#before-after-content
  dom::PseudoElement* pseudo_element =
      html_element->pseudo_element(pseudo_element_type);
  if (pseudo_element) {
    ContainerBoxGenerator pseudo_element_box_generator(
        pseudo_element->computed_style(), pseudo_element->transitions(),
        used_style_provider_, line_break_iterator_, paragraph_);
    pseudo_element->computed_style()->display()->Accept(
        &pseudo_element_box_generator);
    scoped_ptr<ContainerBox> pseudo_element_box =
        pseudo_element_box_generator.PassContainerBox();
    // A pseudo element with "display: none" generates no boxes and has no
    // effect on layout.
    if (pseudo_element_box != NULL) {
      // Generate the box(es) to be added to the associated html element, using
      // the computed style of the pseudo element.

      // The generated content is a text node with the string value of the
      // 'content' property.
      ContentProvider content_provider;
      pseudo_element->computed_style()->content()->Accept(&content_provider);
      scoped_refptr<dom::Text> child_node(
          new dom::Text(html_element->owner_document(),
                        content_provider.content_string()));

      BoxGenerator child_box_generator(pseudo_element->computed_style(),
                                       used_style_provider_,
                                       line_break_iterator_, paragraph_);
      child_node->Accept(&child_box_generator);
      Boxes child_boxes = child_box_generator.PassBoxes();
      for (Boxes::iterator child_box_iterator = child_boxes.begin();
           child_box_iterator != child_boxes.end(); ++child_box_iterator) {
        scoped_ptr<Box> child_box(*child_box_iterator);
        *child_box_iterator = NULL;
        if (!pseudo_element_box->TryAddChild(&child_box)) {
          return;
        }
      }

      // Add the box(es) from the pseudo element to the associated element.
      AppendChildBoxToLine(pseudo_element_box.release());
    }
  }
}

void BoxGenerator::VisitNonReplacedElement(dom::HTMLElement* html_element) {
  ContainerBoxGenerator container_box_generator(
      html_element->computed_style(), html_element->transitions(),
      used_style_provider_, line_break_iterator_, paragraph_);
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

#ifdef COBALT_BOX_DUMP_ENABLED
  container_box_before_split->SetGeneratingNode(html_element);
#endif  // COBALT_BOX_DUMP_ENABLED

  boxes_.push_back(container_box_before_split.release());

  AppendPseudoElementToLine(html_element, dom::kBeforePseudoElementType);

  // Generate child boxes.
  for (scoped_refptr<dom::Node> child_node = html_element->first_child();
       child_node; child_node = child_node->next_sibling()) {
    BoxGenerator child_box_generator(html_element->computed_style(),
                                     used_style_provider_, line_break_iterator_,
                                     paragraph_);
    child_node->Accept(&child_box_generator);

    Boxes child_boxes = child_box_generator.PassBoxes();
    for (Boxes::iterator child_box_iterator = child_boxes.begin();
         child_box_iterator != child_boxes.end(); ++child_box_iterator) {
      // Transfer the ownership of the child box from |ScopedVector|
      // to AppendChildBoxToLine().
      Box* child_box_ptr = *child_box_iterator;
      *child_box_iterator = NULL;
      AppendChildBoxToLine(child_box_ptr);
    }
  }

  AppendPseudoElementToLine(html_element, dom::kAfterPseudoElementType);
}

void BoxGenerator::Visit(dom::Comment* /*comment*/) {}

void BoxGenerator::Visit(dom::Document* /*document*/) { NOTREACHED(); }

void BoxGenerator::Visit(dom::DocumentType* /*document_type*/) { NOTREACHED(); }

// Append the text from the text node to the text paragraph and create the
// node's initial text box. The text box has indices that map to the paragraph,
// which allows it to retrieve its underlying text. Initially, a single text box
// is created that encompasses the entire node.

// Prior to layout, the paragraph applies the Unicode bidirectional algorithm
// to its text (http://www.unicode.org/reports/tr9/) and causes the text boxes
// referencing it to split at level runs.
//
// During layout, the text boxes are potentially split further, as the paragraph
// determines line breaking locations for its text at soft wrap opportunities
// (http://www.w3.org/TR/css-text-3/#soft-wrap-opportunity) according to the
// Unicode line breaking algorithm (http://www.unicode.org/reports/tr14/).
void BoxGenerator::Visit(dom::Text* text) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style =
      GetComputedStyleOfAnonymousBox(parent_computed_style_);

  DCHECK(text);
  DCHECK(computed_style);

  const std::string& original_text = text->text();

  // Loop until the entire text is consumed. If the white-space property does
  // not have a value of "pre" then the entire text will be processed the first
  // time through the loop; otherwise, the text will be split at newline
  // sequences.
  size_t start_index = 0;
  while (start_index < original_text.size()) {
    size_t end_index;
    size_t newline_sequence_length;

    // Phase I: Segment Break Transformation Rules
    // http://www.w3.org/TR/css3-text/#line-break-transform
    bool generates_newline = false;
    if (computed_style->white_space() == cssom::KeywordValue::GetPre()) {
      generates_newline = FindNextNewlineSequence(
          original_text, start_index, &end_index, &newline_sequence_length);
    } else {
      end_index = original_text.size();
      newline_sequence_length = 0;
    }

    std::string modifiable_text =
        original_text.substr(start_index, end_index - start_index);

    // Phase I: Collapsing and Transformation
    //   http://www.w3.org/TR/css3-text/#white-space-phase-1
    if (computed_style->white_space() != cssom::KeywordValue::GetPre()) {
      CollapseWhiteSpace(&modifiable_text);
    }

    // TODO(***REMOVED***): Determine which transitions to propagate to the text box,
    //               instead of none at all.

    // TODO(***REMOVED***):  Include bidi markup in appended text.

    Paragraph::TextTransform transform;
    if (computed_style->text_transform() ==
        cssom::KeywordValue::GetUppercase()) {
      transform = Paragraph::kUppercaseTextTransform;
    } else {
      transform = Paragraph::kNoTextTransform;
    }

    DCHECK(*paragraph_);
    int32 text_start_position =
        (*paragraph_)->AppendUtf8String(modifiable_text, transform);
    int32 text_end_position = (*paragraph_)->GetTextEndPosition();

    boxes_.push_back(
        new TextBox(computed_style, cssom::TransitionSet::EmptyTransitionSet(),
                    used_style_provider_, *paragraph_, text_start_position,
                    text_end_position, generates_newline));

    // Newline sequences should be transformed into a preserved line feed.
    //   http://www.w3.org/TR/css3-text/#line-break-transform
    if (generates_newline) {
      (*paragraph_)->AppendCodePoint(Paragraph::kLineFeedCodePoint);
    }

    start_index = end_index + newline_sequence_length;
  }
}

BoxGenerator::Boxes BoxGenerator::PassBoxes() { return boxes_.Pass(); }

}  // namespace layout
}  // namespace cobalt
