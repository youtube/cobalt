// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_SELECTOR_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_SELECTOR_PARSER_H_

#include <memory>
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_nesting_type.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenStream;
class CSSParserObserver;
class CSSSelectorList;
class Node;
class StyleRule;
class StyleSheetContents;

// CSSSelectorParser parses a CSS selector (or a comma-separated list of them)
// into a set of CSSSelector objects, in the order and format the rest of the
// code expects (see CSSSelector for details). After this, the selector array
// produced will typically be copied into a StyleRule or a CSSSelectorList,
// by means of CSSSelectorList::AdoptSelectorVector().
//
// In order to be light on memory allocation, CSSSelectorParser uses a scheme
// akin to an arena; as we parse the simple selectors that make up a compound
// (and in turn, a full complex selector), they are being pushed onto a
// HeapVector<CSSSelector>. Then, the required reorderings will be done
// in-place. When we're done parsing, the correct data is pulled out of the
// vector (by the caller), whose memory can then be reused with no further
// allocations for parsing the next selectors. (When doing so, it is important
// to use resize(0) instead of clear(), as clear() deallocates the backing.)
//
// Sometimes, we may need to parse sub-lists of selectors before we're done
// parsing the entire list; e.g. if we have something like div:is(.a, .b):focus
// (or for that matter, just :not(.a)). If so, we can still use the same vector;
// we just notice how many elements were added by the parent, parse the
// sub-list, and then clean up after us when we're done using the vector (the
// sub-list is made into a CSSSelectorList with its own heap allocation). In
// this aspect, we use it a bit like a stack. ResetVectorAfterScope is a helper
// that makes sure we never leave anything we shouldn't, especially in error
// situations.
class CORE_EXPORT CSSSelectorParser {
  STACK_ALLOCATED();

 public:
  // Both ParseSelector() and ConsumeSelector() return an empty list
  // on error. The HeapVector is used for allocating CSSSelectors;
  // the return value points into a slice at the end of the vector
  // (existing elements on the vector are untouched; the output is
  // appended to its end). In other words, if you add new elements
  // to the vector or similar, the span may be invalidated.
  static base::span<CSSSelector> ParseSelector(
      CSSParserTokenRange,
      const CSSParserContext*,
      CSSNestingType,
      const StyleRule* parent_rule_for_nesting,
      StyleSheetContents*,
      HeapVector<CSSSelector>&);
  static base::span<CSSSelector> ConsumeSelector(
      CSSParserTokenStream&,
      const CSSParserContext*,
      CSSNestingType,
      const StyleRule* parent_rule_for_nesting,
      StyleSheetContents*,
      CSSParserObserver*,
      HeapVector<CSSSelector>&);

  static bool ConsumeANPlusB(CSSParserTokenRange&, std::pair<int, int>&);
  CSSSelectorList* ConsumeNthChildOfSelectors(CSSParserTokenRange&);

  static bool SupportsComplexSelector(CSSParserTokenRange,
                                      const CSSParserContext*);

  static CSSSelector::PseudoType ParsePseudoType(const AtomicString&,
                                                 bool has_arguments,
                                                 const Document*);
  static PseudoId ParsePseudoElement(const String&, const Node*);
  // Returns the argument of a parameterized pseudo-element. For example, for
  // '::highlight(foo)' it returns 'foo'.
  static AtomicString ParsePseudoElementArgument(const String&);

  // https://drafts.csswg.org/css-cascade-6/#typedef-scope-start
  // https://drafts.csswg.org/css-cascade-6/#typedef-scope-end
  //
  // Parse errors are signalled by returning absl::nullopt. Empty spans are
  // normal and expected, since <scope-start> / <scope-end> are forgiving
  // selector lists.
  static absl::optional<base::span<CSSSelector>> ParseScopeBoundary(
      CSSParserTokenRange,
      const CSSParserContext*,
      CSSNestingType,
      const StyleRule* parent_rule_for_nesting,
      StyleSheetContents*,
      HeapVector<CSSSelector>&);

 private:
  CSSSelectorParser(const CSSParserContext*,
                    CSSNestingType,
                    const StyleRule* parent_rule_for_nesting,
                    StyleSheetContents*,
                    HeapVector<CSSSelector>&);

  // These will all consume trailing comments if successful.

  // in_nested_style_rule is true if we're at the top level of a nested
  // style rule, which means:
  //
  //  - If the rule starts with a combinator (e.g. “> .a”), we will prepend
  //    an implicit & (parent selector).
  //  - If the selector parses but is not nest-containing
  //    (this cannot happen in the previous situation, of course),
  //    we will also prepend an implicit &, making a descendant selector
  //    (so e.g. “.a” becomes “& .a”.)
  base::span<CSSSelector> ConsumeComplexSelectorList(CSSParserTokenRange& range,
                                                     bool in_nested_style_rule);
  base::span<CSSSelector> ConsumeComplexSelectorList(
      CSSParserTokenStream& range,
      CSSParserObserver* observer,
      bool in_nested_style_rule);
  CSSSelectorList* ConsumeCompoundSelectorList(CSSParserTokenRange&);
  // Consumes a complex selector list if inside_compound_pseudo_ is false,
  // otherwise consumes a compound selector list.
  CSSSelectorList* ConsumeNestedSelectorList(CSSParserTokenRange&);
  CSSSelectorList* ConsumeForgivingNestedSelectorList(CSSParserTokenRange&);
  // https://drafts.csswg.org/selectors/#typedef-forgiving-selector-list
  absl::optional<base::span<CSSSelector>> ConsumeForgivingComplexSelectorList(
      CSSParserTokenRange&,
      bool in_nested_style_rule);
  CSSSelectorList* ConsumeForgivingCompoundSelectorList(CSSParserTokenRange&);
  // https://drafts.csswg.org/selectors/#typedef-relative-selector-list
  CSSSelectorList* ConsumeForgivingRelativeSelectorList(CSSParserTokenRange&);
  CSSSelectorList* ConsumeRelativeSelectorList(CSSParserTokenRange&);
  void AddPlaceholderSelectorIfNeeded(const CSSParserTokenRange& argument);

  base::span<CSSSelector> ConsumeNestedRelativeSelector(
      CSSParserTokenRange& range);
  base::span<CSSSelector> ConsumeRelativeSelector(CSSParserTokenRange&);
  base::span<CSSSelector> ConsumeComplexSelector(
      CSSParserTokenRange& range,
      bool in_nested_style_rule,
      bool first_in_complex_selector_list);

  // ConsumePartialComplexSelector() method provides the common logic of
  // consuming a complex selector and consuming a relative selector.
  //
  // After consuming the left-most combinator of a relative selector, we can
  // consume the remaining selectors with the common logic.
  // For example, after consuming the left-most combinator '~' of the relative
  // selector '~ .a ~ .b', we can consume remaining selectors '.a ~ .b'
  // with this method.
  //
  // After consuming the left-most compound selector and a combinator of a
  // complex selector, we can also use this method to consume the remaining
  // selectors of the complex selector.
  //
  // Returns false if parse error.
  bool ConsumePartialComplexSelector(
      CSSParserTokenRange&,
      CSSSelector::RelationType& /* current combinator */,
      unsigned /* previous compound flags */,
      bool in_nested_style_rule);

  bool ConsumeName(CSSParserTokenRange&,
                   AtomicString& name,
                   AtomicString& namespace_prefix);

  // These will return true iff the selector is valid;
  // otherwise, the vector will be pushed onto output_.
  bool ConsumeId(CSSParserTokenRange&);
  bool ConsumeClass(CSSParserTokenRange&);
  bool ConsumeAttribute(CSSParserTokenRange&);
  bool ConsumePseudo(CSSParserTokenRange&);
  bool ConsumeNestingParent(CSSParserTokenRange& range);
  // This doesn't include element names, since they're handled specially
  bool ConsumeSimpleSelector(CSSParserTokenRange&);

  // Returns an empty range on error.
  base::span<CSSSelector> ConsumeCompoundSelector(CSSParserTokenRange&,
                                                  bool in_nested_style_rule);

  bool PeekIsCombinator(CSSParserTokenRange& range);
  CSSSelector::RelationType ConsumeCombinator(CSSParserTokenRange&);
  CSSSelector::MatchType ConsumeAttributeMatch(CSSParserTokenRange&);
  CSSSelector::AttributeMatchType ConsumeAttributeFlags(CSSParserTokenRange&);

  const AtomicString& DefaultNamespace() const;
  const AtomicString& DetermineNamespace(const AtomicString& prefix);
  void PrependTypeSelectorIfNeeded(const AtomicString& namespace_prefix,
                                   bool has_element_name,
                                   const AtomicString& element_name,
                                   wtf_size_t start_index_of_compound_selector);
  void SplitCompoundAtImplicitShadowCrossingCombinator(
      base::span<CSSSelector> compound_selector);
  void RecordUsageAndDeprecations(base::span<CSSSelector>);
  static bool ContainsUnknownWebkitPseudoElements(
      base::span<CSSSelector> selectors);

  void SetInSupportsParsing() { in_supports_parsing_ = true; }

  const CSSParserContext* context_;
  CSSNestingType nesting_type_;
  const StyleRule* parent_rule_for_nesting_;
  const StyleSheetContents* style_sheet_;

  bool failed_parsing_ = false;
  bool disallow_pseudo_elements_ = false;
  // If we're inside a pseudo class that only accepts compound selectors,
  // for example :host, inner :is()/:where() pseudo classes are also only
  // allowed to contain compound selectors.
  bool inside_compound_pseudo_ = false;
  // When parsing a compound which includes a pseudo-element, the simple
  // selectors permitted to follow that pseudo-element may be restricted.
  // If this is the case, then restricting_pseudo_element_ will be set to the
  // PseudoType of the pseudo-element causing the restriction.
  CSSSelector::PseudoType restricting_pseudo_element_ =
      CSSSelector::kPseudoUnknown;
  // If we're _resisting_ the default namespace, it means that we are inside
  // a nested selector (:is(), :where(), etc) where we should _consider_
  // ignoring the default namespace (depending on circumstance). See the
  // relevant spec text [1] regarding default namespaces for information about
  // those circumstances.
  //
  // [1] https://drafts.csswg.org/selectors/#matches
  bool resist_default_namespace_ = false;
  // While this flag is true, the default namespace is ignored. In other words,
  // the default namespace is '*' while this flag is true.
  bool ignore_default_namespace_ = false;

  // The 'found_pseudo_in_has_argument_' flag is true when we found any pseudo
  // in :has() argument while parsing.
  bool found_pseudo_in_has_argument_ = false;
  bool is_inside_has_argument_ = false;

  // The 'found_complex_logical_combinations_in_has_argument_' flag is true when
  // we found any logical combinations (:is(), :where(), :not()) containing
  // complex selector in :has() argument while parsing.
  bool found_complex_logical_combinations_in_has_argument_ = false;
  bool is_inside_logical_combination_in_has_argument_ = false;

  bool in_supports_parsing_ = false;

  // See the comment on ParseSelector(); when we allocate a CSSSelector,
  // it is on this vector (which we effectively use as an arena).
  HeapVector<CSSSelector>& output_;

  class DisallowPseudoElementsScope {
    STACK_ALLOCATED();

   public:
    explicit DisallowPseudoElementsScope(CSSSelectorParser* parser)
        : parser_(parser), was_disallowed_(parser_->disallow_pseudo_elements_) {
      parser_->disallow_pseudo_elements_ = true;
    }
    DisallowPseudoElementsScope(const DisallowPseudoElementsScope&) = delete;
    DisallowPseudoElementsScope& operator=(const DisallowPseudoElementsScope&) =
        delete;

    ~DisallowPseudoElementsScope() {
      parser_->disallow_pseudo_elements_ = was_disallowed_;
    }

   private:
    CSSSelectorParser* parser_;
    bool was_disallowed_;
  };

  // A RAII-style class that can do two things:
  //
  //  - When it's destroyed, remove any leftover elements from the vector
  //    (typically output_, our working area) that were not there when we
  //    started. This is especially useful in error handling.
  //
  //  - Return a list of what those elements are; they can then either be
  //    stored away somewhere (e.g. a CSSSelectorList) or committed so that
  //    they remain after destruction instead.
  class ResetVectorAfterScope {
    STACK_ALLOCATED();

   public:
    explicit ResetVectorAfterScope(HeapVector<CSSSelector>& vector)
        : vector_(vector), initial_size_(vector.size()) {}

    ~ResetVectorAfterScope() {
      DCHECK_GE(vector_.size(), initial_size_);
      if (!committed_) {
        vector_.resize(initial_size_);
      }
    }

    base::span<CSSSelector> AddedElements() {
      DCHECK_GE(vector_.size(), initial_size_);
      return {vector_.begin() + initial_size_, vector_.end()};
    }

    // Make sure the added elements are left on the vector after
    // destruction, contrary to common behavior. This is used after
    // a successful parse where we intend to actually return the
    // given elements. Returns AddedElements() for convenience.
    base::span<CSSSelector> CommitAddedElements() {
      committed_ = true;
      return AddedElements();
    }

   private:
    HeapVector<CSSSelector>& vector_;
    const wtf_size_t initial_size_;
    bool committed_ = false;
  };
};

// If we are in nesting context, semicolons abort selector parsing
// (so that e.g. “//color: red; font-size: 10px;” stops at the first
// semicolon instead of eating the entire rest of the block -- the
// standard chooses to parse pretty much everything except an ident
// as a qualified rule and thus a selector). However, at the top level,
// due to web-compat reasons, semicolons should _not_ do so,
// and instead keep consuming the selector up until the block.
//
// This function only deals with semicolons, not other things that would
// abort selector parsing (such as EOF).
static inline bool AbortsNestedSelectorParsing(const CSSParserToken& token,
                                               bool in_nested_style_rule) {
  return in_nested_style_rule && token.GetType() == kSemicolonToken;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_SELECTOR_PARSER_H_
