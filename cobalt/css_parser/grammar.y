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

// List of tokens in ascending order of precedence.
// Precedence is used to resolve shift-reduce conflicts.
// See https://www.gnu.org/software/bison/manual/html_node/Non-Operators.html
%nonassoc kLowestPrecedenceToken
%left kUnimportantToken
%nonassoc ':'
%nonassoc '.'
%nonassoc '['
%nonassoc '*'
%nonassoc error
%left '|'

// Number of shift-reduce conflicts resolved in favor of shift.
%expect 29

//
// Rules and their types.
//

// A top-level rule.
%start stylesheet

%union { cssom::CSSStyleRule* rule; }
%type <rule> rule ruleset valid_rule
%destructor { $$->Release(); } <rule>

%union { cssom::CSSStyleDeclaration* style_declaration; }
%type <style_declaration> decl_list declaration_list
%destructor { $$->Release(); } <style_declaration>

%union { PropertyDeclaration* property_declaration; }
%type <property_declaration> declaration
%destructor { delete $$; } <property_declaration>

%type <string> property

%union { cssom::PropertyValue* property_value; }
%type <property_value> expr term unary_term valid_expr
%destructor { $$->Release(); } <property_value>

%union { cssom::CSSStyleRule::Selectors* selectors; }
%type <selectors> selector_list
%destructor { delete $$; } <selectors>

%union { cssom::Selector* selector; }
%type <selector> selector simple_selector selector_with_trailing_whitespace
                 element_name
%destructor { delete $$; } <selector>

%%

// TODO(***REMOVED***): Generate warnings for unsupported CSS syntax.
// TODO(***REMOVED***): Copy YYERROR invocations from WebKit.
// TODO(***REMOVED***): Remove useless empty rules.
// TODO(***REMOVED***): Generate AST.

stylesheet:
  maybe_space maybe_charset maybe_sgml rule_list ;

// For expressions that require at least one whitespace to be present,
// like the + and - operators in calc expressions.
space:
    kWhitespaceToken
  | space kWhitespaceToken
  ;

maybe_space:
    /* empty */ %prec kUnimportantToken
  | maybe_space kWhitespaceToken
  ;

maybe_sgml:
    /* empty */
  | maybe_sgml kSgmlCommentDelimiterToken
  | maybe_sgml kWhitespaceToken
  ;

closing_brace:
    '}'
  | %prec kLowestPrecedenceToken kEndOfFileToken
  ;

closing_parenthesis:
    ')'
  | %prec kLowestPrecedenceToken kEndOfFileToken
  ;

rule_list:
    /* empty */
  | rule_list rule maybe_sgml {
    if ($2) {
      parser_impl->style_sheet().AppendRule(MakeScopedRefPtrAndRelease($2));
    }
  }
  ;

valid_rule:
    ruleset
  | media { $$ = NULL; }
  | page { $$ = NULL; }
  | font_face { $$ = NULL; }
  | keyframes { $$ = NULL; }
  | namespace { $$ = NULL; }
  | import { $$ = NULL; }
  | region { $$ = NULL; }
  | supports { $$ = NULL; }
  | viewport { $$ = NULL; }
  ;

rule:
    valid_rule
  | ignored_charset { $$ = NULL; }
  | invalid_rule { $$ = NULL; }
  | invalid_at { $$ = NULL; }
  ;

block_rule_list:
    /* empty */
  | block_rule_list block_rule maybe_sgml
  ;

block_valid_rule_list:
    /* empty */
  | block_valid_rule_list block_valid_rule maybe_sgml
  ;

block_valid_rule:
    ruleset
  | page
  | font_face
  | media
  | keyframes
  | supports
  | viewport
  ;

block_rule:
    block_valid_rule
  | invalid_rule
  | invalid_at
  | namespace
  | import
  | region
  ;

at_import_header_end_maybe_space:
  maybe_space ;

before_import_rule:
  /* empty */ ;

/* TODO(***REMOVED***): Find unsupported rules by looking for unsupported tokens
                 and returning unsupported_syntax with the location of those
                 tokens. */

import:
    before_import_rule kImportToken at_import_header_end_maybe_space
      string_or_uri maybe_space maybe_media_list ';'
  | before_import_rule kImportToken at_import_header_end_maybe_space
      string_or_uri maybe_space maybe_media_list kEndOfFileToken
  | before_import_rule kImportToken at_import_header_end_maybe_space
      string_or_uri maybe_space maybe_media_list invalid_block
  | before_import_rule kImportToken error ';'
  | before_import_rule kImportToken error invalid_block
  ;

namespace:
    kNamespaceToken maybe_space maybe_ns_prefix string_or_uri maybe_space ';'
  | kNamespaceToken maybe_space maybe_ns_prefix string_or_uri maybe_space
      invalid_block
  | kNamespaceToken error invalid_block
  | kNamespaceToken error ';'
  ;

maybe_ns_prefix:
    /* empty */
  | kIdentifierToken maybe_space
  ;

string_or_uri:
    kStringToken
  | kUriToken
  ;

maybe_media_value:
    /*empty*/
  | ':' maybe_space expr maybe_space {
    scoped_refptr<cssom::PropertyValue> property_value =
        MakeScopedRefPtrAndRelease($3);
  }
  ;

media_query_exp:
    maybe_media_restrictor maybe_space '(' maybe_space kIdentifierToken
      maybe_space maybe_media_value ')' maybe_space {
  }
  ;

media_query_exp_list:
    media_query_exp
  | media_query_exp_list maybe_space kMediaAndToken maybe_space media_query_exp
  ;

maybe_and_media_query_exp_list:
    /*empty*/
  | kMediaAndToken maybe_space media_query_exp_list
  ;

maybe_media_restrictor:
    /*empty*/
  | kMediaOnlyToken
  | kMediaNotToken
  ;

media_query:
    media_query_exp_list
  | maybe_media_restrictor maybe_space kIdentifierToken maybe_space
      maybe_and_media_query_exp_list {
  }
  ;

maybe_media_list:
    /* empty */
  | media_list
  ;

media_list:
    media_query
  | media_list ',' maybe_space media_query
  | media_list error
  ;

at_rule_body_start:
  /* empty */ ;

before_media_rule:
  /* empty */ ;

at_rule_header_end_maybe_space:
  maybe_space ;

media:
    before_media_rule kMediaToken maybe_space media_list at_rule_header_end '{'
      at_rule_body_start maybe_space block_rule_list save_block
  | before_media_rule kMediaToken at_rule_header_end_maybe_space '{'
      at_rule_body_start maybe_space block_rule_list save_block
  | before_media_rule kMediaToken at_rule_header_end_maybe_space ';'
  ;

supports:
    before_supports_rule kSupportsToken maybe_space supports_condition
      at_supports_rule_header_end '{' at_rule_body_start maybe_space
          block_rule_list save_block
  | before_supports_rule kSupportsToken supports_error
  ;

supports_error:
    error ';'
  | error invalid_block
  ;

before_supports_rule:
  /* empty */ ;

at_supports_rule_header_end:
  /* empty */ ;

supports_condition:
    supports_condition_in_parens
  | supports_negation
  | supports_conjunction
  | supports_disjunction
  ;

supports_negation:
  kSupportsNotToken maybe_space supports_condition_in_parens ;

supports_conjunction:
    supports_condition_in_parens kSupportsAndToken maybe_space
      supports_condition_in_parens
  | supports_conjunction kSupportsAndToken maybe_space
      supports_condition_in_parens
  ;

supports_disjunction:
    supports_condition_in_parens kSupportsOrToken maybe_space
      supports_condition_in_parens
  | supports_disjunction kSupportsOrToken maybe_space
      supports_condition_in_parens
  ;

supports_condition_in_parens:
    '(' maybe_space supports_condition ')' maybe_space
  | supports_declaration_condition
  | '(' error ')'
  ;

supports_declaration_condition:
  '(' maybe_space property ':' maybe_space expr priority ')' maybe_space {
    scoped_refptr<cssom::PropertyValue> property_value =
        MakeScopedRefPtrAndRelease($6);
  }
  ;

before_keyframes_rule:
  /* empty */ ;

keyframes:
  before_keyframes_rule kWebkitKeyframesToken maybe_space keyframe_name
    at_rule_header_end_maybe_space '{' at_rule_body_start maybe_space
      keyframes_rule closing_brace ;

keyframe_name:
    kIdentifierToken
  | kStringToken
  ;

keyframes_rule:
    /* empty */
  | keyframes_rule keyframe_rule maybe_space
  ;

keyframe_rule:
  key_list maybe_space '{' maybe_space declaration_list closing_brace {
    scoped_refptr<cssom::CSSStyleDeclaration> style =
        MakeScopedRefPtrAndRelease($5);
  }
  ;

key_list:
    key
  | key_list maybe_space ',' maybe_space key
  ;

key:
    maybe_unary_operator kPercentageToken
  | kIdentifierToken
  | error
  ;

before_page_rule:
  /* empty */ ;

page:
    before_page_rule kPageToken maybe_space page_selector
      at_rule_header_end_maybe_space '{' at_rule_body_start
        maybe_space_before_declaration declarations_and_margins closing_brace
  | before_page_rule kPageToken error invalid_block
  | before_page_rule kPageToken error ';'
  ;

page_selector:
    /* empty */
  | kIdentifierToken
  | kIdentifierToken pseudo_page
  | pseudo_page
  ;

declarations_and_margins:
    declaration_list {
    scoped_refptr<cssom::CSSStyleDeclaration> style =
        MakeScopedRefPtrAndRelease($1);
  }
  | declarations_and_margins margin_box maybe_space declaration_list {
    scoped_refptr<cssom::CSSStyleDeclaration> style =
        MakeScopedRefPtrAndRelease($4);
  }
  ;

margin_box:
  margin_sym maybe_space '{' maybe_space declaration_list closing_brace {
    scoped_refptr<cssom::CSSStyleDeclaration> style =
        MakeScopedRefPtrAndRelease($5);
  }
  ;

margin_sym:
    kTopLeftCornerToken
  | kTopLeftToken
  | kTopCenterToken
  | kTopRightToken
  | kTopRightCornerToken
  | kBottomLeftCornerToken
  | kBottomLeftToken
  | kBottomCenterToken
  | kBottomRightToken
  | kBottomRightCornerToken
  | kLeftTopToken
  | kLeftMiddleToken
  | kLeftBottomToken
  | kRightTopToken
  | kRightMiddleToken
  | kRightBottomToken
  ;

before_font_face_rule:
  /* empty */ ;

font_face:
    before_font_face_rule kFontFaceToken at_rule_header_end_maybe_space '{'
      at_rule_body_start maybe_space_before_declaration declaration_list
        closing_brace {
    scoped_refptr<cssom::CSSStyleDeclaration> style =
        MakeScopedRefPtrAndRelease($7);
  }
  | before_font_face_rule kFontFaceToken error invalid_block
  | before_font_face_rule kFontFaceToken error ';'
  ;

before_viewport_rule:
  /* empty */ ;

viewport:
    before_viewport_rule kWebkitViewportToken at_rule_header_end_maybe_space
      '{' at_rule_body_start maybe_space_before_declaration declaration_list
        closing_brace {
    scoped_refptr<cssom::CSSStyleDeclaration> style =
        MakeScopedRefPtrAndRelease($7);
  }
  | before_viewport_rule kWebkitViewportToken error invalid_block
  | before_viewport_rule kWebkitViewportToken error ';'
  ;

before_region_rule:
  /* empty */ ;

region:
  before_region_rule kWebkitRegionToken maybe_space selector_list
    at_rule_header_end '{' at_rule_body_start maybe_space block_valid_rule_list
      save_block {
    scoped_ptr<cssom::CSSStyleRule::Selectors> selectors($4);
  }
  ;

combinator:
    '+' maybe_space
  | '~' maybe_space
  | '>' maybe_space
  ;

maybe_unary_operator:
    /* empty */
  | unary_operator
  ;

unary_operator:
    '-'
  | '+'
  ;

maybe_space_before_declaration:
  maybe_space ;

before_selector_list:
  /* empty */ ;

at_rule_header_end:
  /* empty */ ;

at_selector_end:
  /* empty */ ;

ruleset:
  before_selector_list selector_list at_selector_end at_rule_header_end '{'
    at_rule_body_start maybe_space_before_declaration declaration_list
      closing_brace {
    scoped_ptr<cssom::CSSStyleRule::Selectors> selectors($2);
    scoped_refptr<cssom::CSSStyleDeclaration> style =
        MakeScopedRefPtrAndRelease($8);
    $$ = AddRef(new cssom::CSSStyleRule(selectors->Pass(), style));
  }
  ;

before_selector_group_item:
  /* empty */ ;

selector_list:
    selector %prec kUnimportantToken {
    $$ = new cssom::CSSStyleRule::Selectors();
    if ($1) {
      $$->push_back($1);
    }
  }
  | selector_list at_selector_end ',' maybe_space before_selector_group_item
      selector %prec kUnimportantToken {
    $$ = $1;
    if ($6) {
      $$->push_back($6);
    }
  }
  | selector_list error {
    $$ = $1;
    $$->clear();
  }
  ;

selector_with_trailing_whitespace:
  selector kWhitespaceToken ;

selector:
    simple_selector
  | selector_with_trailing_whitespace
  | selector_with_trailing_whitespace simple_selector {
    scoped_ptr<cssom::Selector> selector_with_trailing_whitespace($1);
    scoped_ptr<cssom::Selector> simple_selector($2);
    // TODO(***REMOVED***): Implement selector combinators.
    $$ = NULL;
  }
  | selector combinator simple_selector {
    scoped_ptr<cssom::Selector> selector($1);
    scoped_ptr<cssom::Selector> simple_selector($3);
    // TODO(***REMOVED***): Implement selector combinators.
    $$ = NULL;
  }
  | selector error {
    scoped_ptr<cssom::Selector> selector($1);
    $$ = NULL;
  }
  ;

namespace_selector:
    '|'
  | '*' '|'
  | kIdentifierToken '|'
  ;

simple_selector:
    element_name
  | element_name specifier_list {
    scoped_ptr<cssom::Selector> element_name($1);
    // TODO(***REMOVED***): Implement specifiers.
    $$ = NULL;
  }
  | specifier_list { $$ = NULL; }
  | namespace_selector element_name {
    scoped_ptr<cssom::Selector> element_name($2);
    $$ = NULL;
  }
  | namespace_selector element_name specifier_list {
    scoped_ptr<cssom::Selector> element_name($2);
    $$ = NULL;
  }
  | namespace_selector specifier_list { $$ = NULL; }
  ;

simple_selector_list:
    simple_selector %prec kUnimportantToken
  | simple_selector_list maybe_space ',' maybe_space simple_selector %prec kUnimportantToken {
    scoped_ptr<cssom::Selector> simple_selector($5);
  }
  | simple_selector_list error
  ;

element_name:
    kIdentifierToken { $$ = new cssom::TypeSelector($1.ToString()); }
  | '*' {
    // TODO(***REMOVED***): Implement universal selector.
    $$ = NULL;
  }
  ;

specifier_list:
    specifier
  | specifier_list specifier
  | specifier_list error
  ;

specifier:
    kIdSelectorToken
  | kHexToken
  | class
  | attrib
  | pseudo
  ;

class:
    '.' kIdentifierToken {
  }
  ;

attrib:
    '[' maybe_space kIdentifierToken maybe_space ']' {
  }
  | '[' maybe_space kIdentifierToken maybe_space match maybe_space
      ident_or_string maybe_space ']' {
  }
  | '[' maybe_space namespace_selector kIdentifierToken maybe_space ']' {
  }
  | '[' maybe_space namespace_selector kIdentifierToken maybe_space match
      maybe_space ident_or_string maybe_space ']' {
  }
  ;

match:
    '='
  | kIncludesToken
  | kDashMatchToken
  | kBeginsWithToken
  | kEndsWithToken
  | kContainsToken
  ;

ident_or_string:
    kIdentifierToken
  | kStringToken
  ;

pseudo_page:
    ':' kIdentifierToken {
  }
  ;

pseudo:
    ':' kIdentifierToken {
  }
  | ':' ':' kIdentifierToken {
  }
  // used by ::cue(:past/:future)
  | ':' ':' kCueFunctionToken maybe_space simple_selector_list maybe_space ')'
  // use by :-webkit-any.
  // We may support generic selectors here  but we use simple_selector_list
  // for now to match -moz-any.
  // See http://lists.w3.org/Archives/Public/www-style/2010Sep/0566.html
  // for some related discussion with respect to :not.
  | ':' kAnyFunctionToken maybe_space simple_selector_list maybe_space ')'
  // used by :nth-*(ax+b)
  | ':' nth_function maybe_space kNthToken maybe_space ')' {
  }
  // used by :nth-*
  | ':' nth_function maybe_space maybe_unary_operator kIntegerToken
      maybe_space ')'
  // used by :nth-*(odd/even) and :lang
  | ':' nth_function maybe_space kIdentifierToken maybe_space ')' {
  }
  // used by :not
  | ':' kNotFunctionToken maybe_space simple_selector maybe_space ')' {
    scoped_ptr<cssom::Selector> simple_selector($4);
  }
  ;

nth_function:
    kNthChildFunctionToken
  | kNthOfTypeFunctionToken
  | kNthLastChildFunctionToken
  | kNthLastOfTypeFunctionToken
  ;

// TODO(***REMOVED***): Log warnings for properties that failed to parse.

declaration_list:
    /* empty */ {
    $$ = AddRef(new cssom::CSSStyleDeclaration());
  }
  | declaration {
    $$ = AddRef(new cssom::CSSStyleDeclaration());
    parser_impl->SetPropertyValueOrLogWarning($$, make_scoped_ptr($1), @1);
  }
  | decl_list declaration {
    $$ = $1;
    parser_impl->SetPropertyValueOrLogWarning($1, make_scoped_ptr($2), @2);
  }
  | decl_list
  | decl_list_recovery {
    $$ = AddRef(new cssom::CSSStyleDeclaration());
  }
  | decl_list decl_list_recovery
  ;

decl_list:
    declaration ';' maybe_space {
    $$ = AddRef(new cssom::CSSStyleDeclaration());
    parser_impl->SetPropertyValueOrLogWarning($$, make_scoped_ptr($1), @1);
  }
  | decl_list_recovery ';' maybe_space {
    $$ = AddRef(new cssom::CSSStyleDeclaration());
  }
  | decl_list declaration ';' maybe_space {
    parser_impl->SetPropertyValueOrLogWarning($1, make_scoped_ptr($2), @2);
    $$ = $1;
  }
  | decl_list decl_list_recovery ';' maybe_space
  ;

decl_list_recovery:
  error error_recovery ;

declaration:
    property ':' maybe_space expr priority {
    $$ = new PropertyDeclaration($1, MakeScopedRefPtrAndRelease($4));
  }
  | property declaration_recovery {
    $$ = NULL;
  }
  | property ':' maybe_space expr priority declaration_recovery {
    $$ = new PropertyDeclaration($1, MakeScopedRefPtrAndRelease($4));
  }
  | kImportantToken maybe_space declaration_recovery {
    $$ = NULL;
  }
  | property ':' maybe_space declaration_recovery {
    $$ = NULL;
  }
  ;

declaration_recovery:
  error error_recovery ;

property:
  kIdentifierToken maybe_space ;

priority:
    /* empty */
  | kImportantToken maybe_space
  ;

ident_list:
    kIdentifierToken maybe_space
  | ident_list kIdentifierToken maybe_space {
  }
  ;

track_names_list:
    '(' maybe_space closing_parenthesis
  | '(' maybe_space ident_list closing_parenthesis
  | '(' maybe_space expr_recovery closing_parenthesis
  ;

expr:
    valid_expr
  | valid_expr expr_recovery
  ;

valid_expr:
    term
  | valid_expr operator term {
    scoped_refptr<cssom::PropertyValue> expression =
        MakeScopedRefPtrAndRelease($1);
    scoped_refptr<cssom::PropertyValue> term = MakeScopedRefPtrAndRelease($3);
    // TODO(***REMOVED***): Handle multi-term expressions.
    $$ = NULL;
  }
  ;

expr_recovery:
  error error_recovery ;

operator:
    /* empty */
  | '/' maybe_space
  | ',' maybe_space
  ;

term:
    unary_term maybe_space
  | unary_operator unary_term maybe_space {
    scoped_refptr<cssom::PropertyValue> property_value =
        MakeScopedRefPtrAndRelease($2);
    // TODO(***REMOVED***): Implement negative values.
    $$ = NULL;
  }
  | kStringToken maybe_space {
    $$ = AddRef(new cssom::StringValue($1.ToString()));
  }
  | kIdentifierToken maybe_space {
    $$ = AddRef(new cssom::StringValue($1.ToString()));
  }
  | kInvalidDimensionToken maybe_space { $$ = NULL; }
  | unary_operator kInvalidDimensionToken maybe_space { $$ = NULL; }
  | kUriToken maybe_space { $$ = NULL; }
  | kUnicodeRangeToken maybe_space { $$ = NULL; }
  | kHexToken maybe_space {
    char* value_end(const_cast<char*>($1.end));
    uint32_t rgb = std::strtoul($1.begin, &value_end, 16);
    DCHECK_EQ(value_end, $1.end);

    $$ = AddRef(new cssom::RGBAColorValue((rgb << 8) | 0xff));
  }
  | '#' maybe_space /* Handle error case: "color: #;". */ { $$ = NULL; }
  // According to the specs a function can have a unary_operator in front
  // but there are no known cases where this makes sense.
  | function maybe_space { $$ = NULL; }
  | calc_function maybe_space { $$ = NULL; }
  | min_or_max_function maybe_space { $$ = NULL; }
  | '%' maybe_space /* Handle width: "%;". */ { $$ = NULL; }
  | track_names_list maybe_space { $$ = NULL; }
  ;

// TODO(***REMOVED***): Implement rest of units.
unary_term:
    kIntegerToken { $$ = NULL; }
  | kRealToken { $$ = NULL; }
  | kPercentageToken { $$ = NULL; }
  | kPixelsToken {
    $$ = AddRef(new cssom::LengthValue($1, cssom::kPixelsUnit));
  }
  | kCentimetersToken { $$ = NULL; }
  | kMillimetersToken { $$ = NULL; }
  | kInchesToken { $$ = NULL; }
  | kPointsToken { $$ = NULL; }
  | kPicasToken { $$ = NULL; }
  | kDegreesToken { $$ = NULL; }
  | kRadiansToken { $$ = NULL; }
  | kGradiansToken { $$ = NULL; }
  | kTurnsToken { $$ = NULL; }
  | kMillisecondsToken { $$ = NULL; }
  | kSecondsToken { $$ = NULL; }
  | kHertzToken { $$ = NULL; }
  | kKilohertzToken { $$ = NULL; }
  | kFontSizesAkaEmToken { $$ = NULL; }
  | kXHeightsAkaExToken { $$ = NULL; }
  | kRootElementFontSizesAkaRemToken { $$ = NULL; }
  | kZeroGlyphWidthsAkaChToken { $$ = NULL; }
  | kViewportWidthPercentsAkaVwToken { $$ = NULL; }
  | kViewportHeightPercentsAkaVhToken { $$ = NULL; }
  | kViewportSmallerSizePercentsAkaVminToken { $$ = NULL; }
  | kViewportLargerSizePercentsAkaVmaxToken { $$ = NULL; }
  | kDotsPerPixelToken { $$ = NULL; }
  | kDotsPerInchToken { $$ = NULL; }
  | kDotsPerCentimeterToken { $$ = NULL; }
  | kFractionsToken { $$ = NULL; }
  ;

function:
    kInvalidFunctionToken maybe_space expr closing_parenthesis {
    scoped_refptr<cssom::PropertyValue> property_value =
        MakeScopedRefPtrAndRelease($3);
  }
  | kInvalidFunctionToken maybe_space closing_parenthesis
  | kInvalidFunctionToken maybe_space expr_recovery closing_parenthesis
  ;

calc_func_term:
    unary_term
  | unary_operator unary_term {
    scoped_refptr<cssom::PropertyValue> property_value =
        MakeScopedRefPtrAndRelease($2);
  }
  ;

// The grammar requires spaces around binary ‘+’ and ‘-’ operators.
// The '*' and '/' operators do not require spaces.
// http://www.w3.org/TR/css3-values/#calc-syntax
calc_func_operator:
    space '+' space
  | space '-' space
  | calc_maybe_space '*' maybe_space
  | calc_maybe_space '/' maybe_space
  ;

calc_maybe_space:
    /* empty */
  | kWhitespaceToken
  ;

calc_func_paren_expr:
  '(' maybe_space calc_func_expr calc_maybe_space closing_parenthesis ;

calc_func_expr:
    valid_calc_func_expr
  | valid_calc_func_expr expr_recovery
  ;

valid_calc_func_expr:
    calc_func_term
  | calc_func_expr calc_func_operator calc_func_term
  | calc_func_expr calc_func_operator calc_func_paren_expr
  | calc_func_paren_expr
  ;

calc_func_expr_list:
    calc_func_expr calc_maybe_space
  | calc_func_expr_list ',' maybe_space calc_func_expr calc_maybe_space
  ;

calc_function:
    kCalcFunctionToken maybe_space calc_func_expr calc_maybe_space
      closing_parenthesis
  | kCalcFunctionToken maybe_space expr_recovery closing_parenthesis
  ;

min_or_max:
    kMinFunctionToken
  | kMaxFunctionToken
  ;

min_or_max_function:
    min_or_max maybe_space calc_func_expr_list closing_parenthesis
  | min_or_max maybe_space expr_recovery closing_parenthesis
  ;

// Error handling rules.

save_block:
    closing_brace
  | error closing_brace
  ;

invalid_at:
    kInvalidAtToken error invalid_block {
    parser_impl->LogWarning(@1, "invalid token " + $1.ToString());
  }
  | kInvalidAtToken error ';' {
    parser_impl->LogWarning(@1, "invalid token " + $1.ToString());
  }
  ;

invalid_rule:
    error invalid_block {
    parser_impl->LogWarning(@1, "invalid rule");
  }
  ;

invalid_block:
  '{' error_recovery closing_brace ;

invalid_square_brackets_block:
    '[' error_recovery ']'
  | '[' error_recovery kEndOfFileToken
  ;

invalid_parentheses_block:
  opening_parenthesis error_recovery closing_parenthesis ;

opening_parenthesis:
    '('
  | kInvalidFunctionToken
  | kCalcFunctionToken
  | kMinFunctionToken
  | kMaxFunctionToken
  | kAnyFunctionToken
  | kNotFunctionToken
  | kCueFunctionToken
  ;

error_recovery:
    /* empty */
  | error_recovery error
  | error_recovery invalid_block
  | error_recovery invalid_square_brackets_block
  | error_recovery invalid_parentheses_block
  ;

//
// Rules below, while recognized, are not supported by Cobalt.
//

maybe_charset:
    /* empty */
  | charset
  ;

charset:
    kCharsetToken maybe_space kStringToken maybe_space ';' {
    parser_impl->LogWarning(@1, "@charset is not supported");
  }
  | kCharsetToken error invalid_block {
    parser_impl->LogWarning(@1, "@charset is not supported");
  }
  | kCharsetToken error ';' {
    parser_impl->LogWarning(@1, "@charset is not supported");
  }
  ;

// Ignore any @charset rule not at the beginning of the style sheet.
ignored_charset:
    kCharsetToken maybe_space kStringToken maybe_space ';' {
    parser_impl->LogWarning(@1, "@charset is not supported");
  }
  | kCharsetToken maybe_space ';' {
    parser_impl->LogWarning(@1, "@charset is not supported");
  }
  ;
