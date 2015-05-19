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

// This file contains a definition of CSS grammar.

// A reentrant parser.
%pure_parser

// yyparse()'s first and only parameter.
%parse-param { ParserImpl* parser_impl }

%{
// TODO(***REMOVED***): Define custom YYLTYPE which only has one line-column pair.

// yylex()'s third parameter.
#define YYLEX_PARAM &(parser_impl->scanner())
%}

// Token values returned by a scanner.
%union TokenValue {
  TrivialStringPiece string;
  int integer;
  float real;
}

//
// Tokens returned by a scanner.
//

// Entry point tokens, injected by the parser in order to choose the path
// within a grammar, never appear in the source code.
%token kStyleSheetEntryPointToken
%token kStyleRuleEntryPointToken
%token kDeclarationListEntryPointToken
%token kPropertyValueEntryPointToken
%token kPropertyIntoStyleEntryPointToken

// Tokens without a value.
%token kEndOfFileToken 0                // null
%token kWhitespaceToken                 // tab, space, CR, LF
%token kSgmlCommentDelimiterToken       // <!-- -->
%token kCommentToken                    // /* */
%token kImportantToken                  // !important

// Property name tokens.
// WARNING: every time a new name token is introduced, it should be added
//          to |identifier_token| rule below.
%token kAllToken                              // all
%token kBackgroundToken                       // background
%token kBackgroundColorToken                  // background-color
%token kBackgroundImageToken                  // background-image
%token kBorderRadiusToken                     // border-radius
%token kColorToken                            // color
%token kDisplayToken                          // display
%token kFontFamilyToken                       // font-family
%token kFontSizeToken                         // font-size
%token kFontWeightToken                       // font-weight
%token kHeightToken                           // height
%token kLineHeightToken                       // line-height
%token kOpacityToken                          // opacity
%token kOverflowToken                         // overflow
%token kPositionToken                         // position
%token kTransformToken                        // transform
%token kTransitionToken                       // transition
%token kTransitionDelayToken                  // transition-delay
%token kTransitionDurationToken               // transition-duration
%token kTransitionPropertyToken               // transition-property
%token kTransitionTimingFunctionToken         // transition-timing-function
%token kWidthToken                            // width

// Property value tokens.
// WARNING: every time a new name token is introduced, it should be added
//          to |identifier_token| rule below.
%token kAbsoluteToken                   // absolute
%token kBlockToken                      // block
%token kBoldToken                       // bold
%token kEaseInOutToken                  // ease-in-out
%token kEaseInToken                     // ease-in
%token kEaseOutToken                    // ease-out
%token kEaseToken                       // ease
%token kEndToken                        // end
%token kHiddenToken                     // hidden
%token kInheritToken                    // inherit
%token kInitialToken                    // initial
%token kInlineBlockToken                // inline-block
%token kInlineToken                     // inline
%token kLinearToken                     // linear
%token kNoneToken                       // none
%token kNormalToken                     // normal
%token kRelativeToken                   // relative
%token kStartToken                      // start
%token kStaticToken                     // static
%token kStepEndToken                    // step-end
%token kStepStartToken                  // step-start
%token kVisibleToken                    // visible

// Pseudo-class name tokens.
// WARNING: every time a new name token is introduced, it should be added
//          to |identifier_token| rule below.
%token kEmptyToken                      // empty

// Attribute matching tokens.
%token kIncludesToken                   // ~=
%token kDashMatchToken                  // |=
%token kBeginsWithToken                 // ^=
%token kEndsWithToken                   // $=
%token kContainsToken                   // *=

// "Media query" mode tokens.
%token kMediaAndToken                   // and (in "media query" mode)
%token kMediaNotToken                   // not (in "media query" mode)
%token kMediaOnlyToken                  // only (in "media query" mode)

// "Supports" mode tokens.
%token kSupportsAndToken                // and (in "supports" mode)
%token kSupportsNotToken                // not (in "supports" mode)
%token kSupportsOrToken                 // or (in "supports" mode)

// @-tokens.
%token kImportToken                     // @import
%token kPageToken                       // @page
%token kMediaToken                      // @media
%token kFontFaceToken                   // @font-face
%token kCharsetToken                    // @charset
%token kNamespaceToken                  // @namespace
%token kSupportsToken                   // @supports
%token kWebkitKeyframesToken            // @-webkit-keyframes
%token kWebkitRegionToken               // @-webkit-region
%token kWebkitViewportToken             // @-webkit-viewport

// Paged media tokens.
%token kTopLeftCornerToken              // @top-left-corner
%token kTopLeftToken                    // @top-left
%token kTopCenterToken                  // @top-center
%token kTopRightToken                   // @top-right
%token kTopRightCornerToken             // @top-right-corner
%token kBottomLeftCornerToken           // @bottom-left-corner
%token kBottomLeftToken                 // @bottom-left
%token kBottomCenterToken               // @bottom-center
%token kBottomRightToken                // @bottom-right
%token kBottomRightCornerToken          // @bottom-right-corner
%token kLeftTopToken                    // @left-top
%token kLeftMiddleToken                 // @left-middle
%token kLeftBottomToken                 // @left-bottom
%token kRightTopToken                   // @right-top
%token kRightMiddleToken                // @right-middle
%token kRightBottomToken                // @right-bottom

// Function tokens.
%token kAnyFunctionToken                // -webkit-any(
%token kCalcFunctionToken               // calc(
%token kCubicBezierFunctionToken        // cubic-bezier(
%token kCueFunctionToken                // cue(
%token kMaxFunctionToken                // -webkit-max(
%token kMinFunctionToken                // -webkit-min(
%token kNotFunctionToken                // not(
%token kNthChildFunctionToken           // nth-child(
%token kNthLastChildFunctionToken       // nth-last-child(
%token kNthLastOfTypeFunctionToken      // nth-last-of-type(
%token kNthOfTypeFunctionToken          // nth-of-type(
%token kRotateFunctionToken             // rotate(
%token kScaleFunctionToken              // scale(
%token kStepsFunctionToken              // steps(
%token kTranslateXFunctionToken         // translateX(
%token kTranslateYFunctionToken         // translateY(
%token kTranslateZFunctionToken         // translateZ(
%token kRGBFunctionToken                // rgb(
%token kRGBAFunctionToken               // rgba(

// Tokens with a string value.
%token <string> kStringToken            // "...", '...'
%token <string> kIdentifierToken        // ...
%token <string> kNthToken               // an+b, where a, b - integers
%token <string> kHexToken               // #...
%token <string> kIdSelectorToken        // #...
%token <string> kUnicodeRangeToken      // u+..., U+...
%token <string> kUriToken               // url(...)
%token <string> kInvalidFunctionToken   // ...(
%token <string> kInvalidNumberToken     // ... (digits)
%token <string> kInvalidDimensionToken  // XXyy, where XX - number,
                                        //             yy - identifier
%token <string> kInvalidAtToken         // @...

// Tokens with an integer value.
// WARNING: Use |integer| rule if you want to handle the sign.
%token <integer> kIntegerToken          // 123, for example

// Tokens with a floating point value.
// WARNING: Remember to use |maybe_sign_token| rule with these tokens.
%token <real> kRealToken                                    // 1.23, for example
%token <real> kPercentageToken                              // ...%
%token <real> kRootElementFontSizesAkaRemToken              // ...rem
%token <real> kZeroGlyphWidthsAkaChToken                    // ...ch
%token <real> kFontSizesAkaEmToken                          // ...em
%token <real> kXHeightsAkaExToken                           // ...ex
%token <real> kPixelsToken                                  // ...px
%token <real> kCentimetersToken                             // ...cm
%token <real> kMillimetersToken                             // ...mm
%token <real> kInchesToken                                  // ...in
%token <real> kPointsToken                                  // ...pt
%token <real> kPicasToken                                   // ...pc
%token <real> kDegreesToken                                 // ...deg
%token <real> kRadiansToken                                 // ...rad
%token <real> kGradiansToken                                // ...grad
%token <real> kTurnsToken                                   // ...turn
%token <real> kMillisecondsToken                            // ...ms
%token <real> kSecondsToken                                 // ...s
%token <real> kHertzToken                                   // ...hz
%token <real> kKilohertzToken                               // ...khz
%token <real> kViewportWidthPercentsAkaVwToken              // ...vw
%token <real> kViewportHeightPercentsAkaVhToken             // ...vh
%token <real> kViewportSmallerSizePercentsAkaVminToken      // ...vmin
%token <real> kViewportLargerSizePercentsAkaVmaxToken       // ...vmax
%token <real> kDotsPerPixelToken                            // ...dppx
%token <real> kDotsPerInchToken                             // ...dpi
%token <real> kDotsPerCentimeterToken                       // ...dpcm
%token <real> kFractionsToken                               // ...fr

//
// Rules and their types, sorted by type name.
//

// A top-level rule.
%start entry_point

%union { bool important; }
%type <important> maybe_important

%type <integer> integer

%union { cssom::LengthValue* length; }
%type <length> length
%destructor { $$->Release(); } <length>

// base::TimeDelta's internal value.  One can construct a base::TimeDelta from
// this value using the function base::TimeDelta::FromInternalValue().  We use
// it instead of base::TimeDelta because base::TimeDelta does not have a
// trivial constructor and thus cannot be used in a union.
%union { int64 time; }
%type <time> time

%union { PropertyDeclaration* property_declaration; }
%type <property_declaration> maybe_declaration
%destructor { delete $$; } <property_declaration>

// To reduce the number of classes derived from cssom::PropertyValue, some
// semantic actions contain a value processing (such as opacity clamping
// or RGBA color resolution) that technically belongs to computed value
// resolution and as such should be done by layout engine. This is harmless
// as long as web app does not rely on literal preservation of property values
// exposed by cssom::CSSStyleDeclaration (semantics is always preserved).
%union { cssom::PropertyValue* property_value; }
%type <property_value> background_property_value background_color_property_value
                       background_image_property_value
                       border_radius_property_value color color_property_value
                       common_values display_property_value
                       final_background_layer font_family_property_value
                       font_family_name font_size_property_value
                       font_weight_property_value height_property_value
                       line_height_property_value opacity_property_value
                       overflow_property_value position_property_value
                       transition_delay_property_value transform_property_value
                       transition_duration_property_value
                       transition_property_property_value
                       transition_timing_function_property_value url
                       width_property_value
%destructor { $$->Release(); } <property_value>

%union { Transition* transition; }
%type <transition> transition_property_value
%destructor { delete $$; } <transition>

%type <real> alpha number angle

%union { cssom::CSSStyleSheet* style_sheet; }
%type <style_sheet> rule_list style_sheet
%destructor { $$->Release(); } <style_sheet>

%union { cssom::Selector* selector; }
%type <selector> class_selector_token id_selector_token pseudo_class_token
                 simple_selector_token type_selector_token
                 universal_selector_token
%destructor { delete $$; } <selector>

%union { cssom::CompoundSelector* compound_selector; }
%type <compound_selector> compound_selector_token
%destructor { delete $$; } <compound_selector>

%union { cssom::Combinator* combinator; }
%type <combinator> combinator
%destructor { delete $$; } <combinator>

%union { cssom::ComplexSelector* complex_selector; }
%type <complex_selector> complex_selector
%destructor { delete $$; } <complex_selector>

%union { cssom::Selectors* selectors; }
%type <selectors> selector_list
%destructor { delete $$; } <selectors>

%union { int sign; }
%type <sign> maybe_sign_token

%type <string> identifier_token
%type <string> animatable_property_token

%union { cssom::CSSStyleDeclarationData* style_declaration_data; }
%type <style_declaration_data> declaration_list
%destructor { $$->Release(); } <style_declaration_data>

%union { cssom::CSSStyleDeclaration* style_declaration; }
%type <style_declaration> declaration_block
%destructor { $$->Release(); } <style_declaration>

%union { cssom::CSSStyleRule* style_rule; }
%type <style_rule> at_rule qualified_rule rule style_rule
%destructor { $$->Release(); } <style_rule>

%union { cssom::TransformFunction* transform_function; }
%type <transform_function> scale_function_parameters transform_function
%destructor { delete $$; } <transform_function>

%union { cssom::TransformFunctionListValue::Builder* transform_functions; }
%type <transform_functions> transform_list
%destructor { delete $$; } <transform_functions>

%union { cssom::TimeListValue::Builder* time_list; }
%type <time_list> comma_separated_time_list
%destructor { delete $$; } <time_list>

%union { cssom::ConstStringListValue::Builder* property_name_list; }
%type <property_name_list> comma_separated_animatable_property_name_list
%destructor { delete $$; } <property_name_list>

%union {
  cssom::SteppingTimingFunction::ValueChangeLocation
      stepping_value_change_location;
}
%type <stepping_value_change_location> maybe_steps_start_or_end_parameter

%union { cssom::TimingFunction* timing_function; }
%type <timing_function> single_transition_timing_function
%destructor { $$->Release(); } <timing_function>

%union {
  cssom::TimingFunctionListValue::Builder*
      timing_function_list;
}
%type <timing_function_list>
    comma_separated_single_transition_timing_function_list
%destructor { delete $$; } <timing_function_list>

%union { SingleTransition* single_transition; }
%type <single_transition> single_transition single_non_empty_transition
%destructor { delete $$; } <single_transition>

%union { TransitionBuilder* transition_builder; }
%type <transition_builder> comma_separated_transition_list
%destructor { delete $$; } <transition_builder>

%%

// WARNING: Every rule, except the ones which end with "..._token", should
//          consume trailing whitespace.


// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Common rules used across the grammar.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

maybe_whitespace:
    /* empty */
  | maybe_whitespace kWhitespaceToken
  ;

errors:
    error
  | errors error
  ;


// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// @-rules.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

at_rule:
    kFontFaceToken declaration_block {
    scoped_refptr<cssom::CSSStyleDeclaration> style =
        MakeScopedRefPtrAndRelease($2);
    // TODO(***REMOVED***): Implement.
    parser_impl->LogWarning(@1, "@font-face is not implemented yet");
    $$ = NULL;
  }
  ;


// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Selectors.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

// Some identifiers such as property names or values are recognized
// specifically by the scanner. We are merging those identifiers back together
// to allow their use in selectors.
identifier_token:
    kIdentifierToken
  // Property names.
  | kAllToken {
    $$ = TrivialStringPiece::FromCString(cssom::kAllPropertyName);
  }
  | kBackgroundToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBackgroundPropertyName);
  }
  | kBackgroundColorToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBackgroundColorPropertyName);
  }
  | kBackgroundImageToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBackgroundImagePropertyName);
  }
  | kBorderRadiusToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBorderRadiusPropertyName);
  }
  | kColorToken {
    $$ = TrivialStringPiece::FromCString(cssom::kColorPropertyName);
  }
  | kDisplayToken {
    $$ = TrivialStringPiece::FromCString(cssom::kDisplayPropertyName);
  }
  | kFontFamilyToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFontFamilyPropertyName);
  }
  | kFontSizeToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFontSizePropertyName);
  }
  | kFontWeightToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFontWeightPropertyName);
  }
  | kHeightToken {
    $$ = TrivialStringPiece::FromCString(cssom::kHeightPropertyName);
  }
  | kLineHeightToken {
    $$ = TrivialStringPiece::FromCString(cssom::kLineHeightPropertyName);
  }
  | kOpacityToken {
    $$ = TrivialStringPiece::FromCString(cssom::kOpacityPropertyName);
  }
  | kOverflowToken {
    $$ = TrivialStringPiece::FromCString(cssom::kOverflowPropertyName);
  }
  | kPositionToken {
    $$ = TrivialStringPiece::FromCString(cssom::kPositionPropertyName);
  }
  | kTransformToken {
    $$ = TrivialStringPiece::FromCString(cssom::kTransformPropertyName);
  }
  | kTransitionToken {
    $$ =
        TrivialStringPiece::FromCString(cssom::kTransitionPropertyName);
  }
  | kTransitionDelayToken {
    $$ =
        TrivialStringPiece::FromCString(cssom::kTransitionDelayPropertyName);
  }
  | kTransitionDurationToken {
    $$ =
        TrivialStringPiece::FromCString(cssom::kTransitionDurationPropertyName);
  }
  | kTransitionPropertyToken {
    $$ =
        TrivialStringPiece::FromCString(cssom::kTransitionPropertyPropertyName);
  }
  | kTransitionTimingFunctionToken {
    $$ = TrivialStringPiece::FromCString(
             cssom::kTransitionTimingFunctionPropertyName);
  }
  | kWidthToken {
    $$ = TrivialStringPiece::FromCString(cssom::kWidthPropertyName);
  }
  // Property values.
  | kAbsoluteToken {
    $$ = TrivialStringPiece::FromCString(cssom::kAbsoluteKeywordName);
  }
  | kBlockToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBlockKeywordName);
  }
  | kBoldToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBoldKeywordName);
  }
  | kEaseToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEaseKeywordName);
  }
  | kEaseInToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEaseInKeywordName);
  }
  | kEaseInOutToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEaseInOutKeywordName);
  }
  | kEaseOutToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEaseOutKeywordName);
  }
  | kEndToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEndKeywordName);
  }
  | kHiddenToken {
    $$ = TrivialStringPiece::FromCString(cssom::kHiddenKeywordName);
  }
  | kInheritToken {
    $$ = TrivialStringPiece::FromCString(cssom::kInheritKeywordName);
  }
  | kInlineToken {
    $$ = TrivialStringPiece::FromCString(cssom::kInlineKeywordName);
  }
  | kInlineBlockToken {
    $$ = TrivialStringPiece::FromCString(cssom::kInlineBlockKeywordName);
  }
  | kInitialToken {
    $$ = TrivialStringPiece::FromCString(cssom::kInitialKeywordName);
  }
  | kLinearToken {
    $$ = TrivialStringPiece::FromCString(cssom::kLinearKeywordName);
  }
  | kNoneToken {
    $$ = TrivialStringPiece::FromCString(cssom::kNoneKeywordName);
  }
  | kNormalToken {
    $$ = TrivialStringPiece::FromCString(cssom::kNormalKeywordName);
  }
  | kRelativeToken {
    $$ = TrivialStringPiece::FromCString(cssom::kRelativeKeywordName);
  }
  | kStartToken {
    $$ = TrivialStringPiece::FromCString(cssom::kStartKeywordName);
  }
  | kStaticToken {
    $$ = TrivialStringPiece::FromCString(cssom::kStaticKeywordName);
  }
  | kStepEndToken {
    $$ = TrivialStringPiece::FromCString(cssom::kStepEndKeywordName);
  }
  | kStepStartToken {
    $$ = TrivialStringPiece::FromCString(cssom::kStepStartKeywordName);
  }
  | kVisibleToken {
    $$ = TrivialStringPiece::FromCString(cssom::kVisibleKeywordName);
  }
  // Pseudo-class names.
  | kEmptyToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEmptyPseudoClassName);
  }
  ;

// A type selector represents an instance of the element type in the document
// tree.
//   http://www.w3.org/TR/selectors4/#type-selector
type_selector_token:
    identifier_token {
    $$ = new cssom::TypeSelector($1.ToString());
  }
  ;

// The universal selector represents an element with any name.
//   http://www.w3.org/TR/selectors4/#universal-selector
universal_selector_token: '*' {
    parser_impl->LogWarning(@1, "universal selector is not implemented yet");
    $$ = NULL;  // TODO(***REMOVED***): Implement.
  }
  ;

// The class selector represents an element belonging to the class identified by
// the identifier.
//   http://www.w3.org/TR/selectors4/#class-selector
class_selector_token:
    '.' identifier_token {
    $$ = new cssom::ClassSelector($2.ToString());
  }
  ;

// An ID selector represents an element instance that has an identifier that
// matches the identifier in the ID selector.
//   http://www.w3.org/TR/selectors4/#id-selector
id_selector_token:
    kIdSelectorToken {
    $$ = new cssom::IdSelector($1.ToString());
  }
  | kHexToken {
    if (IsAsciiDigit(*$1.begin)) {
      YYERROR;
    } else {
      $$ = new cssom::IdSelector($1.ToString());
    }
  }
  ;

// The pseudo-class concept is introduced to permit selection based on
// information that lies outside of the document tree or that can be awkward or
// impossible to express using the other simple selectors.
//   http://www.w3.org/TR/selectors4/#pseudo-classes
pseudo_class_token:
  // The :empty pseudo-class represents an element that has no children. In
  // terms of the document tree, only element nodes and content nodes (such as
  // DOM text nodes, CDATA nodes, and entity references) whose data has a
  // non-zero length must be considered as affecting emptiness; comments,
  // processing instructions, and other nodes must not affect whether an element
  // is considered empty or not.
  //   http://www.w3.org/TR/selectors4/#empty-pseudo
    ':' kEmptyToken {
    $$ = new cssom::EmptyPseudoClass();
    break;
  }
  | ':' errors {
    parser_impl->LogWarning(@1, "unsupported pseudo-class");
    $$ = NULL;
  }
  ;

// A simple selector represents an aspect of an element to be matched against.
//   http://www.w3.org/TR/selectors4/#simple
simple_selector_token:
    type_selector_token
  | universal_selector_token
  | class_selector_token
  | id_selector_token
  | pseudo_class_token
  ;

// A compound selector is a chain of simple selectors that are not separated by
// a combinator.
//   http://www.w3.org/TR/selectors4/#compound
compound_selector_token:
    simple_selector_token {
    scoped_ptr<cssom::Selector> simple_selector($1);

    if (simple_selector) {
      $$ = new cssom::CompoundSelector();
      $$->AppendSelector(simple_selector.Pass());
    } else {
      $$ = NULL;
    }
  }
  | compound_selector_token simple_selector_token {
    scoped_ptr<cssom::CompoundSelector> compound_selector($1);
    scoped_ptr<cssom::Selector> simple_selector($2);

    if (compound_selector && simple_selector) {
      $$ = compound_selector.release();
      $$->AppendSelector(simple_selector.Pass());
    } else {
      $$ = NULL;
    }
  }
  ;

// A combinator is punctuation that represents a particular kind of relationship
// between the selectors on either side.
//   http://www.w3.org/TR/selectors4/#combinator
combinator:
    kWhitespaceToken {
    $$ = new cssom::DescendantCombinator();
  }
  | '>' maybe_whitespace {
    $$ = new cssom::ChildCombinator();
  }
  | '+' maybe_whitespace {
    $$ = new cssom::NextSiblingCombinator();
  }
  | '~' maybe_whitespace {
    $$ = new cssom::FollowingSiblingCombinator();
  }
  ;

// A complex selector is a chain of one or more compound selectors separated by
// combinators.
//   http://www.w3.org/TR/selectors4/#complex
complex_selector:
    compound_selector_token {
    scoped_ptr<cssom::CompoundSelector> compound_selector($1);

    if (compound_selector) {
      $$ = new cssom::ComplexSelector();
      $$->AppendSelector(compound_selector.Pass());
    } else {
      $$ = NULL;
    }
  }
  | complex_selector combinator compound_selector_token {
    scoped_ptr<cssom::ComplexSelector> complex_selector($1);
    scoped_ptr<cssom::Combinator> combinator($2);
    scoped_ptr<cssom::CompoundSelector> compound_selector($3);

    if (complex_selector && compound_selector) {
      $$ = complex_selector.release();
      $$->AppendCombinatorAndSelector(combinator.Pass(),
          compound_selector.Pass());
    } else {
      $$ = NULL;
    }
  }
  | complex_selector kWhitespaceToken
  ;

// A selector list is a comma-separated list of selectors.
//   http://www.w3.org/TR/selectors4/#selector-list
selector_list:
    complex_selector {
    scoped_ptr<cssom::ComplexSelector> complex_selector($1);

    if (complex_selector) {
      $$ = new cssom::Selectors();
      $$->push_back(complex_selector.release());
    } else {
      $$ = NULL;
    }
  }
  | selector_list comma complex_selector {
    scoped_ptr<cssom::Selectors> selector_list($1);
    scoped_ptr<cssom::ComplexSelector> complex_selector($3);

    if (selector_list && complex_selector) {
      $$ = selector_list.release();
      $$->push_back(complex_selector.release());
    } else {
      $$ = NULL;
    }
  }
  ;


// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Common rules used in property values.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

// The scanner that we adopted from WebKit was built with assumption that sign
// is handled in the grammar. Practically this means that tokens of <number>
// and <real> types has to be prepended with this rule.
//   http://www.w3.org/TR/css3-syntax/#consume-a-number
maybe_sign_token:
    /* empty */ { $$ = 1; }
  | '+' { $$ = 1; }
  | '-' { $$ = -1; }
  ;

// An integer is one or more decimal digits "0" through "9". The first digit
// of an integer may be immediately preceded by "-" or "+" to indicate the sign.
//   http://www.w3.org/TR/css3-values/#integers
integer:
    maybe_sign_token kIntegerToken maybe_whitespace {
    $$ = $1 * $2;
  }
  ;

// A number is either an |integer| or zero or more decimal digits followed
// by a dot followed by one or more decimal digits.
//   http://www.w3.org/TR/css3-values/#numbers
number:
    integer { $$ = $1; }
  | maybe_sign_token kRealToken maybe_whitespace { $$ = $1 * $2; }
  ;

// Opacity.
//   http://www.w3.org/TR/css3-color/#alphavaluedt
alpha:
    number {
    // Any values outside the range 0.0 (fully transparent)
    // to 1.0 (fully opaque) will be clamped to this range.
    $$ = ClampToRange(0.0f, 1.0f, $1);
  }
  ;

// Distance units.
//   http://www.w3.org/TR/css3-values/#lengths
length:
    number {
    if ($1 == 0) {
      $$ = AddRef(new cssom::LengthValue(0, cssom::kPixelsUnit));
    } else {
      parser_impl->LogWarning(
          @1, "non-zero length is not allowed without unit identifier");
      $$ = NULL;
    }
  }
  // Relative lengths.
  //   http://www.w3.org/TR/css3-values/#relative-lengths
  | maybe_sign_token kFontSizesAkaEmToken maybe_whitespace {
    $$ = AddRef(new cssom::LengthValue($1 * $2, cssom::kFontSizesAkaEmUnit));
  }
  // Absolute lengths.
  //   http://www.w3.org/TR/css3-values/#absolute-lengths
  | maybe_sign_token kPixelsToken maybe_whitespace {
    $$ = AddRef(new cssom::LengthValue($1 * $2, cssom::kPixelsUnit));
  }
  ;

// Angle units (returned synthetic value will always be in radians).
//   http://www.w3.org/TR/css3-values/#angles
angle:
    maybe_sign_token kDegreesToken maybe_whitespace {
    $$ = $1 * $2 * (2 * static_cast<float>(M_PI) / 360.0f);
  }
  | maybe_sign_token kGradiansToken maybe_whitespace {
    $$ = $1 * $2 * (2 * static_cast<float>(M_PI) / 400.0f);
  }
  | maybe_sign_token kRadiansToken maybe_whitespace {
    $$ = $1 * $2;
  }
  | maybe_sign_token kTurnsToken maybe_whitespace {
    $$ = $1 * $2 * 2 * static_cast<float>(M_PI);
  }
  ;

// Time units (used by animations and transitions).
//   http://www.w3.org/TR/css3-values/#time
time:
    number {
    $$ = base::TimeDelta::FromMilliseconds(
             static_cast<int64>($1 * base::Time::kMillisecondsPerSecond)).
             ToInternalValue();
    if ($1 != 0) {
      parser_impl->LogWarning(
          @1, "non-zero time is not allowed without unit identifier");
    }
  }
  | maybe_sign_token kSecondsToken maybe_whitespace {
    $$ = base::TimeDelta::FromMilliseconds(
             static_cast<int64>($1 * $2 * base::Time::kMillisecondsPerSecond)).
             ToInternalValue();
  }
  | maybe_sign_token kMillisecondsToken maybe_whitespace {
    $$ = base::TimeDelta::FromMilliseconds(static_cast<int64>($1 * $2)).
             ToInternalValue();
  }

colon: ':' maybe_whitespace ;
comma: ',' maybe_whitespace ;

// All properties accept the CSS-wide keywords.
//   http://www.w3.org/TR/css3-values/#common-keywords
common_values:
    kInheritToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetInherit().get());
  }
  | kInitialToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetInitial().get());
  }
  | errors {
    // If a user agent does not support a particular value, it should ignore
    // that value when parsing style sheets, as if that value was an illegal
    // value.
    //   http://www.w3.org/TR/CSS21/syndata.html#unsupported-values
    //
    // User agents must ignore a declaration with an illegal value.
    //   http://www.w3.org/TR/CSS21/syndata.html#illegalvalues
    parser_impl->LogWarning(@1, "unsupported value");
    $$ = NULL;
  }
  ;

color:
  // Hexadecimal notation.
  //   http://www.w3.org/TR/css3-color/#numerical
    kHexToken maybe_whitespace {
    switch ($1.size()) {
      // The three-digit RGB notation (#rgb) is converted into six-digit
      // form (#rrggbb) by replicating digits.
      case 3: {
        uint32_t rgb = ParseHexToken($1);
        uint32_t r = (rgb & 0xf00) >> 8;
        uint32_t g = (rgb & 0x0f0) >> 4;
        uint32_t b = (rgb & 0x00f);
        $$ = AddRef(new cssom::RGBAColorValue((r << 28) | (r << 24) |
                                              (g << 20) | (g << 16) |
                                              (b << 12) | (b << 8) |
                                              0xff));
        break;
      }
      case 6: {
        uint32_t rgb = ParseHexToken($1);
        $$ = AddRef(new cssom::RGBAColorValue((rgb << 8) | 0xff));
        break;
      }
      default:
        parser_impl->LogWarning(@1, "invalid color value");
        $$ = NULL;
        break;
    }
  }
  // RGB color model.
  //   http://www.w3.org/TR/css3-color/#rgb-color
  | kRGBFunctionToken maybe_whitespace integer comma
      integer comma integer ')' maybe_whitespace {
    uint8_t r = ClampToRange(0, 255, $3);
    uint8_t g = ClampToRange(0, 255, $5);
    uint8_t b = ClampToRange(0, 255, $7);
    $$ = AddRef(new cssom::RGBAColorValue(r, g, b, 0xff));
  }
  // RGB color model with opacity.
  //   http://www.w3.org/TR/css3-color/#rgba-color
  | kRGBAFunctionToken maybe_whitespace integer comma integer comma integer
      comma alpha ')' maybe_whitespace {
    uint8_t r = ClampToRange(0, 255, $3);
    uint8_t g = ClampToRange(0, 255, $5);
    uint8_t b = ClampToRange(0, 255, $7);
    float a = $9;  // Already clamped.
    $$ = AddRef(
        new cssom::RGBAColorValue(r, g, b, static_cast<uint8_t>(a * 0xff)));
  }
  ;

url:
    kUriToken maybe_whitespace {
    $$ = AddRef(new cssom::URLValue($1.ToString()));
  }
  ;


// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Property values.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

// Only final background layer allows to set the background color.
//   http://www.w3.org/TR/css3-background/#ltfinal-bg-layergt
final_background_layer: color ;

// Shorthand property for setting most background properties at the same place.
//   http://www.w3.org/TR/css3-background/#the-background
background_property_value:
    final_background_layer
  | common_values
  ;

// Background color of an element drawn behind any background images.
//   http://www.w3.org/TR/css3-background/#the-background-color
background_color_property_value:
    color
  | common_values
  ;

// Images are drawn with the first specified one on top (closest to the user)
// and each subsequent image behind the previous one.
//   http://www.w3.org/TR/css3-background/#the-background-image
background_image_property_value:
    kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | url
  | common_values
  ;

// The radii of a quarter ellipse that defines the shape of the corner
// of the outer border edge.
//   http://www.w3.org/TR/css3-background/#the-border-radius
border_radius_property_value:
    length { $$ = $1; }
  | common_values
  ;

// Foreground color of an element's text content.
//   http://www.w3.org/TR/css3-color/#foreground
color_property_value:
    color
  | common_values
  ;

// Controls the generation of boxes.
//   http://www.w3.org/TR/CSS21/visuren.html#display-prop
display_property_value:
    kBlockToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetBlock().get());
  }
  | kInlineToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetInline().get());
  }
  | kInlineBlockToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetInlineBlock().get());
  }
  | kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | common_values
  ;

// Font family names other than generic families must be given quoted
// as strings.
//   http://www.w3.org/TR/css3-fonts/#family-name-value
font_family_name:
    kStringToken maybe_whitespace {
    $$ = AddRef(new cssom::StringValue($1.ToString()));
  }
  ;

// Prioritized list of font family names.
//   http://www.w3.org/TR/css3-fonts/#font-family-prop
//
// TODO(***REMOVED***): Support multiple family names.
font_family_property_value:
    font_family_name
  | common_values
  ;

// Desired height of glyphs from the font.
//   http://www.w3.org/TR/css3-fonts/#font-size-prop
font_size_property_value:
    length { $$ = $1; }
  | common_values
  ;

// The weight of glyphs in the font, their degree of blackness
// or stroke thickness.
//   http://www.w3.org/TR/css3-fonts/#font-weight-prop
font_weight_property_value:
    kNormalToken maybe_whitespace {
    $$ = AddRef(cssom::FontWeightValue::GetNormalAka400().get());
  }
  | kBoldToken maybe_whitespace {
    $$ = AddRef(cssom::FontWeightValue::GetBoldAka700().get());
  }
  | common_values
  ;

// Specifies the content height of boxes.
//   http://www.w3.org/TR/CSS21/visudet.html#the-height-property
height_property_value:
    length { $$ = $1; }  // TODO(***REMOVED***): Reject negative heights.
  | common_values
  ;

// Specifies the minimal height of line boxes within the element.
//   http://www.w3.org/TR/CSS21/visudet.html#line-height
line_height_property_value:
    kNormalToken maybe_whitespace  {
    $$ = AddRef(cssom::KeywordValue::GetNormal().get());
  }
  | length { $$ = $1; }  // TODO(***REMOVED***): Reject negative heights.
  | common_values
  ;

// Specifies how to blend the element (including its descendants)
// into the current composite rendering.
//   http://www.w3.org/TR/css3-color/#transparency
opacity_property_value:
    alpha { $$ = AddRef(new cssom::NumberValue($1)); }
  | common_values
  ;

// Specifies whether content of a block container element is clipped when it
// overflows the element's box.
//   http://www.w3.org/TR/CSS2/visufx.html#overflow
overflow_property_value:
    kHiddenToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetHidden().get());
  }
  | kVisibleToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetVisible().get());
  }
  | common_values
  ;

// Determines which of the positioning algorithms is used to calculate
// the position of a box.
//   http://www.w3.org/TR/CSS21/visuren.html#choose-position
position_property_value:
    kAbsoluteToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetAbsolute().get());
  }
  | kRelativeToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetRelative().get());
  }
  | kStaticToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetStatic().get());
  }
  | common_values
  ;

// If the second parameter is not provided, it takes a value equal to the first.
//   http://www.w3.org/TR/css3-transforms/#funcdef-scale
scale_function_parameters:
    number {
    $$ = new cssom::ScaleFunction($1, $1);
  }
  | number comma number {
    $$ = new cssom::ScaleFunction($1, $3);
  }
  ;

// The set of allowed transform functions.
//   http://www.w3.org/TR/css3-transforms/#transform-functions
transform_function:
  // Specifies a 2D rotation around the z-axis.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-rotate
    kRotateFunctionToken maybe_whitespace angle ')'
      maybe_whitespace {
    $$ = new cssom::RotateFunction($3);
  }
  // Specifies a 2D scale operation by the scaling vector.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-scale
  | kScaleFunctionToken maybe_whitespace scale_function_parameters ')'
      maybe_whitespace {
    $$ = $3;
  }
  // Specifies a translation by the given amount in the X direction.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-translatex
  | kTranslateXFunctionToken maybe_whitespace length ')'
      maybe_whitespace {
    $$ = new cssom::TranslateXFunction($3);
  }
  // Specifies a translation by the given amount in the Y direction.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-translatey
  | kTranslateYFunctionToken maybe_whitespace length ')'
      maybe_whitespace {
    $$ = new cssom::TranslateYFunction($3);
  }
  // Specifies a 3D translation by the vector [0,0,z] with the given amount
  // in the Z direction.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-translatez
  | kTranslateZFunctionToken maybe_whitespace length ')'
      maybe_whitespace {
    $$ = new cssom::TranslateZFunction($3);
  }
  ;

// One or more transform functions separated by whitespace.
//   http://www.w3.org/TR/css3-transforms/#typedef-transform-list
transform_list:
    transform_function {
    $$ = new cssom::TransformFunctionListValue::Builder();
    $$->push_back($1);
  }
  | transform_list transform_function {
    $$ = $1;
    if ($$) { $$->push_back($2); }
  }
  | transform_list error {
    scoped_ptr<cssom::TransformFunctionListValue::Builder>
        transform_functions($1);
    if (transform_functions) {
      parser_impl->LogWarning(@2, "invalid transform function");
    }
    $$ = NULL;
  }
  ;

// A transformation that is applied to the coordinate system an element
// renders in.
//   http://www.w3.org/TR/css3-transforms/#transform-property
transform_property_value:
    kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | transform_list {
    scoped_ptr<cssom::TransformFunctionListValue::Builder>
        transform_functions($1);
    $$ = transform_functions
        ? AddRef(new cssom::TransformFunctionListValue(
              transform_functions->Pass()))
        : NULL;
  }
  | common_values
  ;

// One ore more time values separated by commas.
//   http://www.w3.org/TR/css3-transitions/#transition-duration
comma_separated_time_list:
    time {
    $$ = new cssom::TimeListValue::Builder();
    $$->push_back(base::TimeDelta::FromInternalValue($1));
  }
  | comma_separated_time_list comma time {
    $$ = $1;
    $$->push_back(base::TimeDelta::FromInternalValue($3));
  }
  ;

// Parse a list of time values for transition delay.
//   http://www.w3.org/TR/css3-transitions/#transition-delay
transition_delay_property_value:
    comma_separated_time_list {
    scoped_ptr<cssom::ListValue<base::TimeDelta>::Builder> time_list($1);
    $$ = time_list
         ? AddRef(new cssom::TimeListValue(time_list.Pass()))
         : NULL;
  }
  | common_values
  ;

// Parse a list of time values for transition duration.
//   http://www.w3.org/TR/css3-transitions/#transition-duration
transition_duration_property_value:
    comma_separated_time_list {
    scoped_ptr<cssom::TimeListValue::Builder> time_list($1);
    $$ = time_list
         ? AddRef(new cssom::TimeListValue(time_list.Pass()))
         : NULL;
  }
  | common_values
  ;

maybe_steps_start_or_end_parameter:
    /* empty */ {
    // Default value is 'end'.
    $$ = cssom::SteppingTimingFunction::kEnd;
  }
  | comma kStartToken maybe_whitespace {
    $$ = cssom::SteppingTimingFunction::kStart;
  }
  | comma kEndToken maybe_whitespace {
    $$ = cssom::SteppingTimingFunction::kEnd;
  }
  ;

single_transition_timing_function:
    kCubicBezierFunctionToken maybe_whitespace
    number comma number comma number comma number ')' {
    float p1_x = $3;
    float p1_y = $5;
    float p2_x = $7;
    float p2_y = $9;

    if (p1_x < 0 || p1_x > 1 || p2_x < 0 || p2_x > 1) {
      parser_impl->LogError(
          @1,
          "cubic-bezier control point x values must be in the range [0, 1].");
      // In the case of an error, return the ease function as a default.
      $$ = AddRef(cssom::TimingFunction::GetEase().get());
    } else {
      $$ = AddRef(new cssom::CubicBezierTimingFunction(p1_x, p1_y, p2_x, p2_y));
    }
  }
  | kStepsFunctionToken maybe_whitespace kIntegerToken maybe_whitespace
    maybe_steps_start_or_end_parameter ')' {
    int number_of_steps = $3;
    if (number_of_steps <= 0) {
      parser_impl->LogError(
          @1,
          "The steps() number of steps parameter must be greater than 0.");
      number_of_steps = 1;
    }
    $$ = AddRef(new cssom::SteppingTimingFunction(number_of_steps, $5));
  }
  | kEaseInOutToken {
    $$ = AddRef(cssom::TimingFunction::GetEaseInOut().get());
  }
  | kEaseInToken {
    $$ = AddRef(cssom::TimingFunction::GetEaseIn().get());
  }
  | kEaseOutToken {
    $$ = AddRef(cssom::TimingFunction::GetEaseOut().get());
  }
  | kEaseToken {
    $$ = AddRef(cssom::TimingFunction::GetEase().get());
  }
  | kLinearToken {
    $$ = AddRef(cssom::TimingFunction::GetLinear().get());
  }
  | kStepEndToken {
    $$ = AddRef(cssom::TimingFunction::GetStepEnd().get());
  }
  | kStepStartToken {
    $$ = AddRef(cssom::TimingFunction::GetStepStart().get());
  }
  ;

comma_separated_single_transition_timing_function_list:
    single_transition_timing_function {
    $$ = new cssom::TimingFunctionListValue::Builder();
    $$->push_back(MakeScopedRefPtrAndRelease($1));
  }
  | comma_separated_single_transition_timing_function_list comma
    single_transition_timing_function {
    $$ = $1;
    $$->push_back(MakeScopedRefPtrAndRelease($3));
  }
  ;

transition_timing_function_property_value:
    comma_separated_single_transition_timing_function_list {
    scoped_ptr<cssom::TimingFunctionListValue::Builder>
        timing_function_list($1);
    $$ = timing_function_list
         ? AddRef(new cssom::TimingFunctionListValue(
               timing_function_list.Pass()))
         : NULL;
  }
  | common_values
  ;

// One or more property names separated by commas.
//   http://www.w3.org/TR/css3-transitions/#transition-property
// TODO(***REMOVED***): Support error handling of strings being used in the list
//               of property names that are not animatable properties.  In this
//               case, a NULL value should be inserted in the list.  This is
//               difficult/messy without reduction priorities which are not
//               available in Bison 2.
comma_separated_animatable_property_name_list:
    animatable_property_token maybe_whitespace {
    $$ = new cssom::ConstStringListValue::Builder();
    $$->push_back($1.begin);
  }
  | comma_separated_animatable_property_name_list comma
    animatable_property_token maybe_whitespace {
    $$ = $1;
    $$->push_back($3.begin);
  }

// Parse a list of references to property names.
//   http://www.w3.org/TR/css3-transitions/#transition-property
transition_property_property_value:
    kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | comma_separated_animatable_property_name_list {
    scoped_ptr<cssom::ConstStringListValue::Builder>
        property_name_list($1);
    $$ = property_name_list
         ? AddRef(new cssom::ConstStringListValue(property_name_list.Pass()))
         : NULL;
  }
  | common_values
  ;

// single_transition_element represents a component of a single transition.
// It uses $0 to access its parent's SingleTransition object and build it, so
// it should always be used to the right of a single_transition object.
single_transition_element:
    animatable_property_token maybe_whitespace {
    if (!$<single_transition>0->property) {
      $<single_transition>0->property = $1.begin;
    } else {
      parser_impl->LogError(
          @1, "transition-property value declared twice in transition.");
    }
  }
  | time {
    if (!$<single_transition>0->duration) {
      // The first time encountered sets the duration.
      $<single_transition>0->duration = base::TimeDelta::FromInternalValue($1);
    } else if (!$<single_transition>0->delay) {
      // The second time encountered sets the delay.
      $<single_transition>0->delay = base::TimeDelta::FromInternalValue($1);
    } else {
      parser_impl->LogError(
          @1, "Too many time values declared twice in transition.");
    }
  }
  | single_transition_timing_function maybe_whitespace {
    if (!$<single_transition>0->timing_function) {
      $<single_transition>0->timing_function = MakeScopedRefPtrAndRelease($1);
    } else {
      parser_impl->LogError(
          @1, "transition-timing-function value declared twice in transition.");
    }
  }
  ;

single_transition:
    /* empty */ {
    // Initialize the result, to be filled in by single_transition_element
    $$ = new SingleTransition();
  }
  | single_transition single_transition_element {
    // Propogate the list from our parent single_transition.
    // single_transition_element will have already taken care of adding itself
    // to the list via $0.
    $$ = $1;
  }
  ;

single_non_empty_transition:
    single_transition single_transition_element {
    $$ = $1;
  }
  ;

comma_separated_transition_list:
    single_non_empty_transition {
    scoped_ptr<SingleTransition> single_transition($1);
    scoped_ptr<TransitionBuilder> transition_builder(new TransitionBuilder());

    single_transition->ReplaceNullWithInitialValues();

    transition_builder->property_list_builder->push_back(
        *single_transition->property);
    transition_builder->duration_list_builder->push_back(
        *single_transition->duration);
    transition_builder->timing_function_list_builder->push_back(
        single_transition->timing_function);
    transition_builder->delay_list_builder->push_back(
        *single_transition->delay);

    $$ = transition_builder.release();
  }
  | comma_separated_transition_list comma single_non_empty_transition {
    scoped_ptr<SingleTransition> single_transition($3);

    single_transition->ReplaceNullWithInitialValues();

    $$ = $1;
    $$->property_list_builder->push_back(*single_transition->property);
    $$->duration_list_builder->push_back(*single_transition->duration);
    $$->timing_function_list_builder->push_back(
        single_transition->timing_function);
    $$->delay_list_builder->push_back(*single_transition->delay);
  }
  ;

// Transition shorthand property.
//   http://www.w3.org/TR/css3-transitions/#transition
transition_property_value:
    kNoneToken maybe_whitespace {
    $$ = new Transition();
  }
  | comma_separated_transition_list {
    scoped_ptr<TransitionBuilder> transition_builder($1);
    scoped_ptr<Transition> transition(new Transition());

    transition->property_list = new cssom::ConstStringListValue(
        transition_builder->property_list_builder.Pass());
    transition->duration_list = new cssom::TimeListValue(
        transition_builder->duration_list_builder.Pass());
    transition->timing_function_list = new cssom::TimingFunctionListValue(
        transition_builder->timing_function_list_builder.Pass());
    transition->delay_list = new cssom::TimeListValue(
        transition_builder->delay_list_builder.Pass());

    $$ = transition.release();
  }
  | common_values {
    // Replicate the common value into each of the properties that transition
    // is a shorthand for.
    scoped_ptr<Transition> transition(new Transition());
    transition->property_list = $1;
    transition->duration_list = $1;
    transition->timing_function_list = $1;
    transition->delay_list = $1;
    $$ = transition.release();
  }
  ;

// Specifies the content width of boxes.
//   http://www.w3.org/TR/CSS21/visudet.html#the-width-property
width_property_value:
    length { $$ = $1; }  // TODO(***REMOVED***): Reject negative widths.
  | common_values
  ;

maybe_important:
    /* empty */ { $$ = false; }
  | kImportantToken maybe_whitespace { $$ = true; }
  ;

// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Animatable properties.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

// Some property name identifiers can be specified as animations, either by
// CSS animations or CSS transitions.  We explicitly list them here for use
// in animatable property lists.
// Note that for Cobalt we modify this list to contain only the properties that
// are supported for animations in Cobalt.
//   http://www.w3.org/TR/css3-transitions/#animatable-css
animatable_property_token:
    kAllToken {
    $$ = TrivialStringPiece::FromCString(cssom::kAllPropertyName);
  }
  | kBackgroundColorToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBackgroundColorPropertyName);
  }
  | kBorderRadiusToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBorderRadiusPropertyName);
  }
  | kColorToken {
    $$ = TrivialStringPiece::FromCString(cssom::kColorPropertyName);
  }
  | kOpacityToken {
    $$ = TrivialStringPiece::FromCString(cssom::kOpacityPropertyName);
  }
  | kTransformToken {
    $$ = TrivialStringPiece::FromCString(cssom::kTransformPropertyName);
  }
  ;


// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Declarations.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

// Consume a declaration.
//   http://www.w3.org/TR/css3-syntax/#consume-a-declaration0
maybe_declaration:
    /* empty */ { $$ = NULL; }
  | kBackgroundToken maybe_whitespace colon background_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBackgroundPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBackgroundColorToken maybe_whitespace colon background_color_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBackgroundColorPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBackgroundImageToken maybe_whitespace colon background_image_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBackgroundImagePropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBorderRadiusToken maybe_whitespace colon border_radius_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBorderRadiusPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kColorToken maybe_whitespace colon color_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kColorPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kDisplayToken maybe_whitespace colon display_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kDisplayPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kFontFamilyToken maybe_whitespace colon font_family_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kFontFamilyPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kFontSizeToken maybe_whitespace colon font_size_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kFontSizePropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kFontWeightToken maybe_whitespace colon font_weight_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kFontWeightPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kHeightToken maybe_whitespace colon height_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kHeightPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kLineHeightToken maybe_whitespace colon line_height_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kLineHeightPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kOpacityToken maybe_whitespace colon opacity_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kOpacityPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kOverflowToken maybe_whitespace colon overflow_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kOverflowPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kPositionToken maybe_whitespace colon position_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kPositionPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTransformToken maybe_whitespace colon transform_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTransformPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTransitionToken maybe_whitespace colon
      transition_property_value maybe_important {
    scoped_ptr<Transition> transition($4);
    DCHECK_NE(static_cast<Transition*>(NULL), transition.get());

    scoped_ptr<PropertyDeclaration> property_declaration(
        new PropertyDeclaration($5));

    // Unpack the transition shorthand property values.
    property_declaration->property_values.push_back(
        PropertyDeclaration::NameValuePair(
            cssom::kTransitionPropertyPropertyName,
            transition->property_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::NameValuePair(
            cssom::kTransitionDurationPropertyName,
            transition->duration_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::NameValuePair(
            cssom::kTransitionTimingFunctionPropertyName,
            transition->timing_function_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::NameValuePair(
            cssom::kTransitionDelayPropertyName,
            transition->delay_list));

    $$ = property_declaration.release();
  }
  | kTransitionDelayToken maybe_whitespace colon
      transition_delay_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTransitionDelayPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTransitionDurationToken maybe_whitespace colon
      transition_duration_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTransitionDurationPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTransitionPropertyToken maybe_whitespace colon
      transition_property_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTransitionPropertyPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTransitionTimingFunctionToken maybe_whitespace colon
      transition_timing_function_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(
                      cssom::kTransitionTimingFunctionPropertyName,
                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kWidthToken maybe_whitespace colon width_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kWidthPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kIdentifierToken maybe_whitespace colon errors {
    std::string property_name = $1.ToString();
    DCHECK_GT(property_name.size(), 0);

    // Do not warn about non-standard or non-WebKit properties.
    if (property_name[0] != '-' ||
        StartsWithASCII(property_name, "-webkit-", false)) {
      parser_impl->LogWarning(@1, "unsupported property " + property_name);
    }
    $$ = NULL;
  }
  | errors {
    parser_impl->LogWarning(@1, "invalid declaration");
    $$ = NULL;
  }
  ;

semicolon: ';' maybe_whitespace ;

// Consume a list of declarations.
//   http://www.w3.org/TR/css3-syntax/#consume-a-list-of-declarations0
declaration_list:
    maybe_declaration {
    $$ = AddRef(new cssom::CSSStyleDeclarationData());

    scoped_ptr<PropertyDeclaration> property_declaration($1);
    if (property_declaration) {
      property_declaration->ApplyToStyle($$);
    }
  }
  | declaration_list semicolon maybe_declaration {
    $$ = $1;

    scoped_ptr<PropertyDeclaration> property_declaration($3);
    if (property_declaration) {
      property_declaration->ApplyToStyle($$);
    }
  }
  ;

declaration_block:
    '{' maybe_whitespace declaration_list '}' maybe_whitespace {
    $$ = AddRef(new cssom::CSSStyleDeclaration($3, parser_impl->css_parser()));
  }
  ;


// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Rules.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

// A style rule is a qualified rule that associates a selector list with a list
// of property declarations.
//   http://www.w3.org/TR/css3-syntax/#style-rule
style_rule:
    selector_list declaration_block {
    scoped_ptr<cssom::Selectors> selectors($1);
    scoped_refptr<cssom::CSSStyleDeclaration> style =
        MakeScopedRefPtrAndRelease($2);

    if (selectors) {
      $$ = AddRef(new cssom::CSSStyleRule(selectors->Pass(), style));
    } else {
      $$ = NULL;
    }
  }
  ;

// To parse a CSS stylesheet, interpret all of the resulting top-level qualified
// rules as style rules.
//   http://www.w3.org/TR/css3-syntax/#css-stylesheets
qualified_rule: style_rule ;

invalid_rule:
    error ';' maybe_whitespace { parser_impl->LogWarning(@1, "invalid rule"); }
  | error declaration_block {
    scoped_refptr<cssom::CSSStyleDeclaration> unused_style =
        MakeScopedRefPtrAndRelease($2);
    parser_impl->LogWarning(@1, "invalid rule");
  }
  ;

// Consume a list of rules.
//   http://www.w3.org/TR/css3-syntax/#consume-a-list-of-rules
rule:
    kSgmlCommentDelimiterToken maybe_whitespace { $$ = NULL; }
  | at_rule
  | qualified_rule
  | invalid_rule { $$ = NULL; }
  ;

rule_list:
    /* empty */ {
    $$ = AddRef(new cssom::CSSStyleSheet(parser_impl->css_parser()));
  }
  | rule_list rule {
    $$ = $1;
    scoped_refptr<cssom::CSSStyleRule> style_rule =
        MakeScopedRefPtrAndRelease($2);
    if (style_rule) {
      $$->AppendCSSStyleRule(style_rule);
    }
  }
  ;


// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Entry points.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

// To parse a stylesheet, consume a list of rules.
//   http://www.w3.org/TR/css3-syntax/#parse-a-stylesheet
style_sheet: rule_list ;

// Parser entry points.
//   http://dev.w3.org/csswg/css-syntax/#parser-entry-points
entry_point:
  // Parses the entire stylesheet.
    kStyleSheetEntryPointToken maybe_whitespace style_sheet {
    scoped_refptr<cssom::CSSStyleSheet> style_sheet =
        MakeScopedRefPtrAndRelease($3);
    parser_impl->set_style_sheet(style_sheet);
  }
  // Parses the style rule.
  | kStyleRuleEntryPointToken maybe_whitespace style_rule {
    scoped_refptr<cssom::CSSStyleRule> style_rule =
        MakeScopedRefPtrAndRelease($3);
    parser_impl->set_style_rule(style_rule);
  }
  // Parses the contents of a HTMLElement.style attribute.
  | kDeclarationListEntryPointToken maybe_whitespace declaration_list {
    scoped_refptr<cssom::CSSStyleDeclarationData> declaration_list =
        MakeScopedRefPtrAndRelease($3);
    parser_impl->set_declaration_list(declaration_list);
  }
  // Parses a single non-shorthand property value.
  | kPropertyValueEntryPointToken maybe_whitespace maybe_declaration {
    scoped_ptr<PropertyDeclaration> property_declaration($3);
    if (property_declaration != NULL) {
      DCHECK_EQ(1, property_declaration->property_values.size()) <<
          "Cannot parse shorthand properties as single property values.";
      if (property_declaration->important) {
        parser_impl->LogError(@1,
                              "!important is not allowed in property value");
      } else {
        parser_impl->set_property_value(
            property_declaration->property_values[0].value);
      }
    }
  }
  // Parses the property value and correspondingly sets the values of a passed
  // in CSSStyleDeclarationData.
  // This is Cobalt's equivalent of a "list of component values".
  | kPropertyIntoStyleEntryPointToken maybe_whitespace maybe_declaration {
    scoped_ptr<PropertyDeclaration> property_declaration($3);
    if (property_declaration != NULL) {
      if (property_declaration->important) {
        parser_impl->LogError(
            @1, "!important is not allowed when setting single property value");
      } else {
        DCHECK_NE(static_cast<cssom::CSSStyleDeclarationData*>(NULL),
                  parser_impl->into_declaration_list());
        property_declaration->ApplyToStyle(
            parser_impl->into_declaration_list());
      }
    }
  }
  ;
