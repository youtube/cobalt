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
%token kBackgroundColorToken            // background-color
%token kColorToken                      // color
%token kFontFamilyToken                 // font-family
%token kFontSizeToken                   // font-size

// Property value tokens.
%token kInheritToken                    // inherit
%token kInitialToken                    // initial

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
%token kNotFunctionToken                // not(
%token kCalcFunctionToken               // calc(
%token kMinFunctionToken                // -webkit-min(
%token kMaxFunctionToken                // -webkit-max(
%token kCueFunctionToken                // cue(
%token kNthChildFunctionToken           // nth-child(
%token kNthOfTypeFunctionToken          // nth-of-type(
%token kNthLastChildFunctionToken       // nth-last-child(
%token kNthLastOfTypeFunctionToken      // nth-last-of-type(

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
%token <integer> kIntegerToken          // 123, for example

// Tokens with a floating point value.
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
%type <property_value> background_color color common_values font_family
                       font_family_name font_size foreground_color length
                       numeric_value
%destructor { $$->Release(); } <property_value>

%union { cssom::Selector* selector; }
%type <selector> class_selector complex_selector compound_selector id_selector
                 pseudo_class simple_selector type_selector universal_selector
%destructor { delete $$; } <selector>

%union { cssom::CSSStyleRule::Selectors* selectors; }
%type <selectors> selector_list
%destructor { delete $$; } <selectors>

%type <string> identifier

%union { cssom::CSSStyleDeclaration* style_declaration; }
%type <style_declaration> declaration_block declaration_list
%destructor { $$->Release(); } <style_declaration>

%%

maybe_whitespace:
    /* empty */
  | maybe_whitespace kWhitespaceToken
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
identifier:
    kIdentifierToken
  | kBackgroundColorToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBackgroundColorProperty);
  }
  | kColorToken {
    $$ = TrivialStringPiece::FromCString(cssom::kColorProperty);
  }
  | kFontFamilyToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFontFamilyProperty);
  }
  | kFontSizeToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFontSizeProperty);
  }
  | kInheritToken {
    $$ = TrivialStringPiece::FromCString(cssom::InheritedValue::kKeyword);
  }
  | kInitialToken {
    $$ = TrivialStringPiece::FromCString(cssom::InitialValue::kKeyword);
  }
  ;

// A type selector is the name of a document language element type written as
// an identifier.
//   http://dev.w3.org/csswg/selectors-4/#type-selector
type_selector:
    identifier {
    $$ = new cssom::TypeSelector($1.ToString());
  }
  ;

// The universal selector represents an element with any name.
//   http://dev.w3.org/csswg/selectors-4/#universal-selector
universal_selector: '*' { $$ = NULL; } ;  // TODO(***REMOVED***): Implement.

// The class selector represents an element belonging to the class identified
// by the identifier.
//   http://dev.w3.org/csswg/selectors-4/#class-selector
class_selector: '.' identifier { $$ = NULL; } ;  // TODO(***REMOVED***): Implement.

// An ID selector represents an element instance that has an identifier
// that matches the identifier in the ID selector.
//   http://dev.w3.org/csswg/selectors-4/#id-selector
id_selector: '#' identifier { $$ = NULL; } ;  // TODO(***REMOVED***): Implement.

// The pseudo-class concept is introduced to permit selection based
// on information that lies outside of the document tree or that can be awkward
// or impossible to express using the other simple selectors.
//   http://dev.w3.org/csswg/selectors-4/#pseudo_class
//
// TODO(***REMOVED***): Replace identifier with token.
pseudo_class: ':' identifier { $$ = NULL; } ;  // TODO(***REMOVED***): Implement.

// A simple selector represents an aspect of an element to be matched against.
//   http://dev.w3.org/csswg/selectors-4/#simple
simple_selector:
    type_selector
  | universal_selector
  | class_selector
  | id_selector
  | pseudo_class
  ;

// A compound selector is a sequence of simple selectors that are not separated
// by a combinator.
//   http://dev.w3.org/csswg/selectors-4/#compound
compound_selector:
    simple_selector
  | compound_selector simple_selector {
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
    compound_selector
  | complex_selector combinator compound_selector {
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

// Numeric data types.
//   http://www.w3.org/TR/css3-values/#numeric-types
numeric_value:
    kIntegerToken maybe_whitespace { $$ = NULL; }  // TODO(***REMOVED***): Implement.
  | kRealToken maybe_whitespace { $$ = NULL; }  // TODO(***REMOVED***): Implement.
  ;

// Distance units.
//   http://www.w3.org/TR/css3-values/#lengths
length:
    numeric_value {
    $$ = $1;  // TODO(***REMOVED***): Warn if not zero.
  }
  // Relative lengths.
  //   http://www.w3.org/TR/css3-values/#relative-lengths
  | kFontSizesAkaEmToken maybe_whitespace {
    $$ = AddRef(new cssom::LengthValue($1, cssom::kFontSizesAkaEmUnit));
  }
  // Absolute lengths.
  //   http://www.w3.org/TR/css3-values/#absolute-lengths
  | kPixelsToken maybe_whitespace {
    $$ = AddRef(new cssom::LengthValue($1, cssom::kPixelsUnit));
  }
  ;

colon: ':' maybe_whitespace ;

// All properties accept the CSS-wide keywords.
//   http://www.w3.org/TR/css3-values/#common-keywords
common_values:
    kInheritToken maybe_whitespace {
    $$ = AddRef(cssom::InheritedValue::GetInstance().get());
  }
  | kInitialToken maybe_whitespace {
    $$ = AddRef(cssom::InitialValue::GetInstance().get());
  }
  ;

color:
  // Hexadecimal notation.
  //   http://www.w3.org/TR/2011/REC-css3-color-20110607/#numerical
    kHexToken maybe_whitespace {
    char* value_end(const_cast<char*>($1.end));
    uint32_t rgb = std::strtoul($1.begin, &value_end, 16);
    DCHECK_EQ(value_end, $1.end);

    $$ = AddRef(new cssom::RGBAColorValue((rgb << 8) | 0xff));
  }
  ;

// Background color of an element drawn behind any background images.
//   http://www.w3.org/TR/css3-background/#the-background-color
background_color:
    color
  | common_values
  ;

// Foreground color of an element's text content.
//   http://www.w3.org/TR/css3-color/#foreground
foreground_color:
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
font_family:
    font_family_name
  | common_values
  ;

// Desired height of glyphs from the font.
// http://www.w3.org/TR/css3-fonts/#font-size-prop
font_size:
    length
  | common_values
  ;

maybe_important:
    /* empty */ { $$ = false; }
  | kImportantToken { $$ = true; }
  ;

// Consume a declaration.
//   http://www.w3.org/TR/css3-syntax/#consume-a-declaration0
maybe_declaration:
    /* empty */ { $$ = NULL; }
  | kBackgroundColorToken maybe_whitespace colon background_color
      maybe_important {
    $$ = new PropertyDeclaration(cssom::kBackgroundColorProperty,
                                 MakeScopedRefPtrAndRelease($4),
                                 $5);
  }
  | kColorToken maybe_whitespace colon foreground_color maybe_important {
    $$ = new PropertyDeclaration(cssom::kColorProperty,
                                 MakeScopedRefPtrAndRelease($4),
                                 $5);
  }
  | kFontFamilyToken maybe_whitespace colon font_family maybe_important {
    $$ = new PropertyDeclaration(cssom::kFontFamilyProperty,
                                 MakeScopedRefPtrAndRelease($4),
                                 $5);
  }
  | kFontSizeToken maybe_whitespace colon font_size maybe_important {
    $$ = new PropertyDeclaration(cssom::kFontSizeProperty,
                                 MakeScopedRefPtrAndRelease($4),
                                 $5);
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
