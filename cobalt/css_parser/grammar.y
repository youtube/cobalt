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
  double real;
}

//
// Tokens returned by a scanner.
//

// Tokens without a value.
%token kEndOfFileToken 0                // null
%token kWhitespaceToken                 // tab, space, CR, LF
%token kSgmlCommentDelimiterToken       // <!-- -->
%token kCommentToken                    // /* */
%token kImportantToken                  // !important

// Property name tokens.
// WARNING: every time a new name token is introduced, it should be added
//          to |identifier_token| rule below.
%token kBackgroundColorToken            // background-color
%token kColorToken                      // color
%token kFontFamilyToken                 // font-family
%token kFontSizeToken                   // font-size
%token kOpacityToken                    // opacity
%token kOverflowToken                   // overflow
%token kTransformToken                  // transform

// Property value tokens.
// WARNING: every time a new name token is introduced, it should be added
//          to |identifier_token| rule below.
%token kHiddenToken                     // hidden
%token kInheritToken                    // inherit
%token kInitialToken                    // initial
%token kNoneToken                       // none
%token kVisibleToken                    // visible

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
%token kCueFunctionToken                // cue(
%token kMaxFunctionToken                // -webkit-max(
%token kMinFunctionToken                // -webkit-min(
%token kNotFunctionToken                // not(
%token kNthChildFunctionToken           // nth-child(
%token kNthLastChildFunctionToken       // nth-last-child(
%token kNthLastOfTypeFunctionToken      // nth-last-of-type(
%token kNthOfTypeFunctionToken          // nth-of-type(
%token kScaleFunctionToken              // scale(
%token kTranslateXFunctionToken         // translateX(
%token kTranslateYFunctionToken         // translateY(
%token kTranslateZFunctionToken         // translateZ(

// Tokens with a string value.
%token <string> kStringToken            // "...", '...'
%token <string> kIdentifierToken        // ...
%token <string> kNthToken               // an+b, where a, b - integers
%token <string> kHexToken               // #...
%token <string> kIdSelectorToken        // #...
%token <string> kUnicodeRangeToken      // u+..., U+...
%token <string> kUriToken               // url(...)
%token <string> kInvalidFunctionToken   // ...(
%token <string> kInvalidDimensionToken  // XXyy, where XX - number,
                                        //             yy - identifier
%token <string> kInvalidAtToken         // @...

// Tokens with an integer value.
// WARNING: Remember to use |maybe_sign_token| rule with this token.
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
// Rules and their types.
//

// A top-level rule.
%start stylesheet

%union { bool important; }
%type <important> maybe_important

%union { PropertyDeclaration* property_declaration; }
%type <property_declaration> maybe_declaration
%destructor { delete $$; } <property_declaration>

%union { cssom::PropertyValue* property_value; }
%type <property_value> background_color_property_value color
                       color_property_value common_values
                       font_family_property_value font_family_name
                       font_size_property_value length number
                       opacity_property_value overflow_property_value
                       transform_property_value
%destructor { $$->Release(); } <property_value>

%union { cssom::Selector* selector; }
%type <selector> class_selector_token complex_selector compound_selector_token
                 id_selector_token pseudo_class_token simple_selector_token
                 type_selector_token universal_selector_token
%destructor { delete $$; } <selector>

%union { cssom::CSSStyleRule::Selectors* selectors; }
%type <selectors> selector_list
%destructor { delete $$; } <selectors>

%union { int sign; }
%type <sign> maybe_sign_token

%type <string> identifier_token

%union { cssom::CSSStyleDeclaration* style_declaration; }
%type <style_declaration> declaration_block declaration_list
%destructor { $$->Release(); } <style_declaration>

%union { cssom::TransformFunction* transform_function; }
%type <transform_function> scale_function_parameters transform_function
%destructor { delete $$; } <transform_function>

%union { cssom::TransformListValue::TransformFunctions* transform_functions; }
%type <transform_functions> transform_list
%destructor { delete $$; } <transform_functions>

%%

// WARNING: Every rule, except the ones which end with "..._token", should
//          consume trailing whitespace.

maybe_whitespace:
    /* empty */
  | maybe_whitespace kWhitespaceToken
  ;

errors:
    error
  | errors error
  ;

at_rule:
    kFontFaceToken declaration_block {
    scoped_refptr<cssom::CSSStyleDeclaration> style =
        MakeScopedRefPtrAndRelease($2);
    // TODO(***REMOVED***): Implement.
  }
  ;

// Some identifiers such as property names or values are recognized
// specifically by the scanner. We are merging those identifiers back together
// to allow their use in selectors.
identifier_token:
    kIdentifierToken
  // Property names.
  | kBackgroundColorToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBackgroundColorPropertyName);
  }
  | kColorToken {
    $$ = TrivialStringPiece::FromCString(cssom::kColorPropertyName);
  }
  | kFontFamilyToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFontFamilyPropertyName);
  }
  | kFontSizeToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFontSizePropertyName);
  }
  | kOpacityToken {
    $$ = TrivialStringPiece::FromCString(cssom::kOpacityPropertyName);
  }
  | kOverflowToken {
    $$ = TrivialStringPiece::FromCString(cssom::kOverflowPropertyName);
  }
  | kTransformToken {
    $$ = TrivialStringPiece::FromCString(cssom::kTransformPropertyName);
  }
  // Property values.
  | kHiddenToken {
    $$ = TrivialStringPiece::FromCString(cssom::kHiddenKeywordName);
  }
  | kInheritToken {
    $$ = TrivialStringPiece::FromCString(cssom::kInheritKeywordName);
  }
  | kInitialToken {
    $$ = TrivialStringPiece::FromCString(cssom::kInitialKeywordName);
  }
  | kNoneToken {
    $$ = TrivialStringPiece::FromCString(cssom::kNoneKeywordName);
  }
  | kVisibleToken {
    $$ = TrivialStringPiece::FromCString(cssom::kVisibleKeywordName);
  }
  ;

// A type selector is the name of a document language element type written as
// an identifier.
//   http://dev.w3.org/csswg/selectors-4/#type-selector
type_selector_token:
    identifier_token {
    $$ = new cssom::TypeSelector($1.ToString());
  }
  ;

// The universal selector represents an element with any name.
//   http://dev.w3.org/csswg/selectors-4/#universal-selector
universal_selector_token: '*' { $$ = NULL; } ;  // TODO(***REMOVED***): Implement.

// The class selector represents an element belonging to the class identified
// by the identifier.
//   http://dev.w3.org/csswg/selectors-4/#class-selector
class_selector_token:
    '.' identifier_token {
    $$ = NULL;  // TODO(***REMOVED***): Implement.
  }
  ;

// An ID selector represents an element instance that has an identifier
// that matches the identifier in the ID selector.
//   http://dev.w3.org/csswg/selectors-4/#id-selector
id_selector_token:
    kIdSelectorToken { $$ = NULL; }  // TODO(***REMOVED***): Implement.
  | kHexToken { $$ = NULL; }  // TODO(***REMOVED***): Implement, check it doesn't start
                              //               with number.
  ;

// The pseudo-class concept is introduced to permit selection based
// on information that lies outside of the document tree or that can be awkward
// or impossible to express using the other simple selectors.
//   http://dev.w3.org/csswg/selectors-4/#pseudo_class
//
// TODO(***REMOVED***): Replace identifier with token.
pseudo_class_token:
    ':' identifier_token {
    $$ = NULL;  // TODO(***REMOVED***): Implement.
  }
  ;

// A simple selector represents an aspect of an element to be matched against.
//   http://dev.w3.org/csswg/selectors-4/#simple
simple_selector_token:
    type_selector_token
  | universal_selector_token
  | class_selector_token
  | id_selector_token
  | pseudo_class_token
  ;

// A compound selector is a sequence of simple selectors that are not separated
// by a combinator.
//   http://dev.w3.org/csswg/selectors-4/#compound
compound_selector_token:
    simple_selector_token
  | compound_selector_token simple_selector_token {
    scoped_ptr<cssom::Selector> simple_selector($2);
    $$ = $1;  // TODO(***REMOVED***): Implement.
  }
  ;

// A combinator represents a particular kind of relationship between the
// elements matched by the compound selectors on either side.
//   http://dev.w3.org/csswg/selectors-4/#combinator
combinator:
    kWhitespaceToken
  | '>' maybe_whitespace
  | '+' maybe_whitespace
  ;

// A complex selector is a sequence of one or more compound selectors separated
// by combinators.
//   http://dev.w3.org/csswg/selectors-4/#complex
complex_selector:
    compound_selector_token
  | complex_selector combinator compound_selector_token {
    scoped_ptr<cssom::Selector> compound_selector($3);
    $$ = $1;  // TODO(***REMOVED***): Implement.
  }
  | complex_selector kWhitespaceToken
  ;

// A selector list is a comma-separated list of selectors.
//   http://dev.w3.org/csswg/selectors-4/#grouping
selector_list:
    complex_selector {
    $$ = new cssom::CSSStyleRule::Selectors();
    $$->push_back($1);
  }
  | selector_list ',' maybe_whitespace complex_selector {
    $$ = $1;
    $$->push_back($4);
  }
  ;

// The scanner that we adopted from WebKit was built with assumption that sign
// is handled in the grammar. Practically this means that tokens of <number>
// and <real> types has to be prepended with this rule.
//   http://www.w3.org/TR/css3-syntax/#consume-a-number
maybe_sign_token:
    /* empty */ { $$ = 1; }
  | '+' { $$ = 1; }
  | '-' { $$ = -1; }
  ;

// Numeric data types.
//   http://www.w3.org/TR/css3-values/#numeric-types
number:
    maybe_sign_token kIntegerToken maybe_whitespace {
    $$ = AddRef(new cssom::NumberValue($1 * $2));
  }
  | maybe_sign_token kRealToken maybe_whitespace {
    $$ = AddRef(new cssom::NumberValue($1 * $2));
  }
  ;

// Distance units.
//   http://www.w3.org/TR/css3-values/#lengths
length:
    number {
    scoped_refptr<cssom::NumberValue> number = MakeScopedRefPtrAndRelease(
        base::polymorphic_downcast<cssom::NumberValue*>($1));
    if (number->value() == 0) {
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

colon: ':' maybe_whitespace ;

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
  ;

// Background color of an element drawn behind any background images.
//   http://www.w3.org/TR/css3-background/#the-background-color
background_color_property_value:
    color
  | common_values
  ;

// Foreground color of an element's text content.
//   http://www.w3.org/TR/css3-color/#foreground
color_property_value:
    color
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
    length
  | common_values
  ;

// Specifies how to blend the element (including its descendants)
// into the current composite rendering.
//   http://www.w3.org/TR/css3-color/#transparency
opacity_property_value:
    number
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

// If the second parameter is not provided, it takes a value equal to the first.
//   http://www.w3.org/TR/css3-transforms/#funcdef-scale
scale_function_parameters:
    number {
    $$ = new cssom::ScaleFunction($1, $1);
  }
  | number ',' maybe_whitespace number {
    $$ = new cssom::ScaleFunction($1, $4);
  }
  ;

// The set of allowed transform functions.
//   http://www.w3.org/TR/css3-transforms/#transform-functions
transform_function:
  // Specifies a 2D scale operation by the scaling vector.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-scale
    kScaleFunctionToken maybe_whitespace scale_function_parameters ')'
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
    $$ = new cssom::TransformListValue::TransformFunctions();
    $$->push_back($1);
  }
  | transform_list transform_function {
    $$ = $1;
    if ($$) { $$->push_back($2); }
  }
  | transform_list error {
    scoped_ptr<cssom::TransformListValue::TransformFunctions>
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
    scoped_ptr<cssom::TransformListValue::TransformFunctions>
        transform_functions($1);
    $$ = transform_functions
        ? AddRef(new cssom::TransformListValue(transform_functions->Pass()))
        : NULL;
  }
  | common_values
  ;

maybe_important:
    /* empty */ { $$ = false; }
  | kImportantToken maybe_whitespace { $$ = true; }
  ;

// Consume a declaration.
//   http://www.w3.org/TR/css3-syntax/#consume-a-declaration0
maybe_declaration:
    /* empty */ { $$ = NULL; }
  | kBackgroundColorToken maybe_whitespace colon background_color_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBackgroundColorPropertyName,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kColorToken maybe_whitespace colon color_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kColorPropertyName,
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
  | kTransformToken maybe_whitespace colon transform_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTransformPropertyName,
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
  ;

semicolon: ';' maybe_whitespace ;

// Consume a list of declarations.
//   http://www.w3.org/TR/css3-syntax/#consume-a-list-of-declarations0
declaration_list:
    maybe_declaration {
    $$ = AddRef(new cssom::CSSStyleDeclaration());

    scoped_ptr<PropertyDeclaration> property($1);
    if (property) {
      $$->SetPropertyValue(property->name, property->value);
      // TODO(***REMOVED***): Set property importance.
      DCHECK_NE(scoped_refptr<cssom::PropertyValue>(),
                $$->GetPropertyValue(property->name));
    }
  }
  | declaration_list semicolon maybe_declaration {
    $$ = $1;

    scoped_ptr<PropertyDeclaration> property($3);
    if (property) {
      $$->SetPropertyValue(property->name, property->value);
      // TODO(***REMOVED***): Set property importance.
      DCHECK_NE(scoped_refptr<cssom::PropertyValue>(),
                $$->GetPropertyValue(property->name));
    }
  }
  ;

declaration_block:
    '{' maybe_whitespace declaration_list '}' maybe_whitespace {
    $$ = $3;
  }
  ;

// A style rule is a qualified rule that associates a selector list with a list
// of property declarations.
//   http://www.w3.org/TR/css3-syntax/#style-rule
style_rule:
    selector_list declaration_block {
    scoped_ptr<cssom::CSSStyleRule::Selectors> selectors($1);
    scoped_refptr<cssom::CSSStyleDeclaration> style =
        MakeScopedRefPtrAndRelease($2);
    scoped_refptr<cssom::CSSStyleRule> style_rule =
        new cssom::CSSStyleRule(selectors->Pass(), style);
    parser_impl->style_sheet().AppendRule(style_rule);
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
    kSgmlCommentDelimiterToken maybe_whitespace
  | at_rule
  | qualified_rule
  | invalid_rule
  ;

rule_list:
    /* empty */
  | rule_list rule
  ;

// To parse a stylesheet, consume a list of rules.
//   http://www.w3.org/TR/css3-syntax/#parse-a-stylesheet
stylesheet: maybe_whitespace rule_list ;
