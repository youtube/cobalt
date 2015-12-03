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
// Specify how the location of an action should be calculated in terms
// of its children.
#define YYLLOC_DEFAULT(Current, Rhs, N)          \
  if (N) {                                       \
    Current.first_line   = Rhs[1].first_line;    \
    Current.first_column = Rhs[1].first_column;  \
    Current.line_start   = Rhs[1].line_start;    \
  } else {                                       \
    Current.first_line   = Rhs[0].first_line;    \
    Current.first_column = Rhs[0].first_column;  \
    Current.line_start   = Rhs[0].line_start;    \
  }

// yylex()'s third parameter.
#define YYLEX_PARAM &(parser_impl->scanner())
%}

// Token values returned by a scanner.
%union TokenValue {
  float real;
  int integer;
  TrivialIntPair integer_pair;
  TrivialStringPiece string;
}

//
// Tokens returned by a scanner.
//

// Entry point tokens, injected by the parser in order to choose the path
// within a grammar, never appear in the source code.
%token kMediaListEntryPointToken
%token kMediaQueryEntryPointToken
%token kStyleSheetEntryPointToken
%token kRuleEntryPointToken
%token kStyleDeclarationListEntryPointToken
%token kFontFaceDeclarationListEntryPointToken
%token kPropertyValueEntryPointToken
%token kPropertyIntoDeclarationDataEntryPointToken

// Tokens without a value.
%token kEndOfFileToken 0                // null
%token kWhitespaceToken                 // tab, space, CR, LF
%token kSgmlCommentDelimiterToken       // <!-- -->
%token kCommentToken                    // /* */
%token kImportantToken                  // !important

// Property name tokens.
// WARNING: Every time a new name token is introduced, it should be added
//          to |identifier_token| rule below.
%token kAllToken                              // all
%token kAnimationDelayToken                   // animation-delay
%token kAnimationDirectionToken               // animation-direction
%token kAnimationDurationToken                // animation-duration
%token kAnimationFillModeToken                // animation-fill-mode
%token kAnimationIterationCountToken          // animation-iteration-count
%token kAnimationNameToken                    // animation-name
%token kAnimationTimingFunctionToken          // animation-timing-function
%token kAnimationToken                        // animation
%token kBackgroundColorToken                  // background-color
%token kBackgroundImageToken                  // background-image
%token kBackgroundPositionToken               // background-position
%token kBackgroundRepeatToken                 // background-repeat
%token kBackgroundSizeToken                   // background-size
%token kBackgroundToken                       // background
%token kBorderToken                           // border
%token kBorderColorToken                      // border-color
%token kBorderRadiusToken                     // border-radius
%token kBorderStyleToken                      // border-style
%token kBorderWidthToken                      // border-width
%token kBottomToken                           // bottom
%token kColorToken                            // color
%token kContentToken                          // content
%token kDisplayToken                          // display
%token kFontFamilyToken                       // font-family
%token kFontSizeToken                         // font-size
%token kFontStyleToken                        // font-style
%token kFontWeightToken                       // font-weight
%token kHeightToken                           // height
%token kLeftToken                             // left
%token kLineHeightToken                       // line-height
%token kMarginBottomToken                     // margin-bottom
%token kMarginLeftToken                       // margin-left
%token kMarginRightToken                      // margin-right
%token kMarginToken                           // margin
%token kMarginTopToken                        // margin-top
%token kMaxHeightToken                        // max-height
%token kMaxWidthToken                         // max-width
%token kMinHeightToken                        // min-height
%token kMinWidthToken                         // min-width
%token kOpacityToken                          // opacity
%token kOverflowToken                         // overflow
%token kOverflowWrapToken                     // overflow-wrap
%token kPaddingBottomToken                    // padding-bottom
%token kPaddingLeftToken                      // padding-left
%token kPaddingRightToken                     // padding-right
%token kPaddingToken                          // padding
%token kPaddingTopToken                       // padding-top
%token kPositionToken                         // position
%token kRightToken                            // right
%token kSrcToken                              // src
%token kTabSizeToken                          // tab-size
%token kTextAlignToken                        // text-align
%token kTextIndentToken                       // text-indent
%token kTextOverflowToken                     // text-overflow
%token kTextTransformToken                    // text-transform
%token kTopToken                              // top
%token kTransformToken                        // transform
%token kTransitionDelayToken                  // transition-delay
%token kTransitionDurationToken               // transition-duration
%token kTransitionPropertyToken               // transition-property
%token kTransitionTimingFunctionToken         // transition-timing-function
%token kTransitionToken                       // transition
%token kUnicodeRangePropertyToken             // unicode-range
%token kVerticalAlignToken                    // vertical-align
%token kVisibilityToken                       // visibility
%token kWhiteSpacePropertyToken               // white-space
%token kWidthToken                            // width
%token kZIndexToken                           // z-index

// Property value tokens.
// WARNING: Every time a new name token is introduced, it should be added
//          to |identifier_token| rule below.
%token kAbsoluteToken                   // absolute
%token kAlternateToken                  // alternate
%token kAlternateReverseToken           // alternate-reverse
%token kAquaToken                       // aqua
%token kAutoToken                       // auto
%token kBackwardsToken                  // backwards
%token kBaselineToken                   // baseline
%token kBlackToken                      // black
%token kBlockToken                      // block
%token kBlueToken                       // blue
%token kBoldToken                       // bold
%token kBothToken                       // both
%token kBreakWordToken                  // break-word
%token kCenterToken                     // center
%token kClipToken                       // clip
%token kContainToken                    // contain
%token kCoverToken                      // cover
%token kCursiveToken                    // cursive
%token kEaseInOutToken                  // ease-in-out
%token kEaseInToken                     // ease-in
%token kEaseOutToken                    // ease-out
%token kEaseToken                       // ease
%token kEllipsisToken                   // ellipsis
%token kEndToken                        // end
%token kFantasyToken                    // fantasy
%token kFixedToken                      // fixed
%token kForwardsToken                   // forwards
%token kFromToken                       // from
%token kFuchsiaToken                    // fuchsia
%token kGrayToken                       // gray
%token kGreenToken                      // green
%token kHiddenToken                     // hidden
%token kInfiniteToken                   // infinite
%token kInheritToken                    // inherit
%token kInitialToken                    // initial
%token kInlineBlockToken                // inline-block
%token kInlineToken                     // inline
%token kItalicToken                     // italic
%token kLimeToken                       // lime
%token kLinearToken                     // linear
// %token kLeftToken                    // left - also property name token
%token kMaroonToken                     // maroon
%token kMiddleToken                     // middle
%token kMonospaceToken                  // monospace
%token kNavyToken                       // navy
%token kNoneToken                       // none
%token kNoRepeatToken                   // no-repeat
%token kNormalToken                     // normal
%token kNoWrapToken                     // nowrap
%token kObliqueToken                    // oblique
%token kOliveToken                      // olive
%token kPreToken                        // pre
%token kPurpleToken                     // purple
%token kRedToken                        // red
%token kRepeatToken                     // repeat
%token kRepeatXToken                    // repeat-x
%token kRepeatYToken                    // repeat-y
%token kRelativeToken                   // relative
%token kReverseToken                    // reverse
// %token kRightToken                   // right - also property name token
%token kSansSerifToken                  // sans-serif
%token kSerifToken                      // serif
%token kSilverToken                     // silver
%token kSolidToken                      // solid
%token kStartToken                      // start
%token kStaticToken                     // static
%token kStepEndToken                    // step-end
%token kStepStartToken                  // step-start
%token kTealToken                       // teal
%token kToToken                         // to
// %token kTopToken                     // top - also property name token
%token kTransparentToken                // transparent
%token kUppercaseToken                  // uppercase
%token kVisibleToken                    // visible
%token kWhiteToken                      // white
%token kYellowToken                     // yellow

// Pseudo-class name tokens.
// WARNING: Every time a new name token is introduced, it should be added
//          to |identifier_token| rule below.
%token kActiveToken                     // active
%token kEmptyToken                      // empty
%token kFocusToken                      // focus
%token kHoverToken                      // hover

// Pseudo-element name tokens.
// WARNING: Every time a new name token is introduced, it should be added
//          to |identifier_token| rule below.
%token kAfterToken                      // after
%token kBeforeToken                     // before

// Attribute matching tokens.
%token kIncludesToken                   // ~=
%token kDashMatchToken                  // |=
%token kBeginsWithToken                 // ^=
%token kEndsWithToken                   // $=
%token kContainsToken                   // *=

// "Media query" mode: Operator tokens.
%token kMediaAndToken                   // and
%token kMediaNotToken                   // not
%token kMediaOnlyToken                  // only
%token kMediaMinimumToken               // min-
%token kMediaMaximumToken               // max-

// "Media query" mode: Media type tokens.
// WARNING: Every time a new media type token is introduced, it should be added
//          to |media_type_known| rule below.
%token kAllMediaTypeToken              // all
%token kTVMediaTypeToken               // tv
%token kScreenMediaTypeToken           // screen

// "Media query" mode: Media feature type tokens. These tokens represent the
// value types of media features. The integer values of these tokens represent
// enum MediaFeatureName.
// WARNING: Every time a new media feature type token is introduced, it should
//          be added to |media_type_unknown| rule below.

%token <integer> kLengthMediaFeatureTypeToken             // ...px, ...em, etc.
%token <integer> kOrientationMediaFeatureTypeToken        // portrait, landscape
%token <integer> kRatioMediaFeatureTypeToken              // ... / ...
%token <integer> kNonNegativeIntegerMediaFeatureTypeToken // 0, 1, 2, 3, ...
%token <integer> kResolutionMediaFeatureTypeToken         // ...dpi, ...dpcm
%token <integer> kScanMediaFeatureTypeToken               // progressive,
                                                          // interlace
%token <integer> kZeroOrOneMediaFeatureTypeToken          // 0, 1

// "Media query" mode: Media feature value tokens.
%token kInterlaceMediaFeatureKeywordValueToken   // interlace
%token kLandscapeMediaFeatureKeywordValueToken   // landscape
%token kPortraitMediaFeatureKeywordValueToken    // portrait
%token kProgressiveMediaFeatureKeywordValueToken // progressive

// "Supports" mode tokens.
%token kSupportsAndToken                // and (in "supports" mode)
%token kSupportsNotToken                // not (in "supports" mode)
%token kSupportsOrToken                 // or (in "supports" mode)

// @-tokens.
%token kImportToken                     // @import
%token kKeyframesToken                  // @keyframes
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
%token kFormatFunctionToken             // format(
%token kLinearGradientFunctionToken     // linear-gradient(
%token kLocalFunctionToken              // local(
%token kMatrixFunctionToken             // matrix(
%token kMaxFunctionToken                // -webkit-max(
%token kMinFunctionToken                // -webkit-min(
%token kNotFunctionToken                // not(
%token kNthChildFunctionToken           // nth-child(
%token kNthLastChildFunctionToken       // nth-last-child(
%token kNthLastOfTypeFunctionToken      // nth-last-of-type(
%token kNthOfTypeFunctionToken          // nth-of-type(
%token kRotateFunctionToken             // rotate(
%token kScaleFunctionToken              // scale(
%token kScaleXFunctionToken             // scaleX(
%token kScaleYFunctionToken             // scaleY(
%token kStepsFunctionToken              // steps(
%token kTranslateFunctionToken          // translate(
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

// Tokens with an integer pair value
%token <integer_pair> kUnicodeRangeToken                    // u+..., U+...

//
// Rules and their types, sorted by type name.
//

// A top-level rule.
%start entry_point

%union { bool important; }
%type <important> maybe_important

%type <integer> integer non_negative_integer positive_integer zero_or_one

%union { cssom::RGBAColorValue* color; }
%type <color> color
%destructor { $$->Release(); } <color>

%union { cssom::ColorStop* color_stop; }
%type <color_stop> color_stop
%destructor { delete $$; } <color_stop>

%union { cssom::LinearGradientValue::ColorStopList* color_stop_list; }
%type <color_stop_list> comma_separated_color_stop_list
%destructor { delete $$; } <color_stop_list>

%union { cssom::PercentageValue* percentage; }
%type <percentage> percentage positive_percentage
%destructor { $$->Release(); } <percentage>

%union { cssom::LengthValue* length; }
%type <length> length positive_length
%destructor { $$->Release(); } <length>

%union { cssom::RatioValue* ratio; }
%type <ratio> ratio
%destructor { $$->Release(); } <ratio>

%union { cssom::ResolutionValue* resolution; }
%type <resolution> resolution
%destructor { $$->Release(); } <resolution>

%union { cssom::StringValue* string_value; }
%type <string_value> font_family_name_identifier_list
                     font_family_string_name
                     font_family_specific_name
                     font_family_specific_name_no_single_identifier
%destructor { $$->Release(); } <string_value>

// base::TimeDelta's internal value.  One can construct a base::TimeDelta from
// this value using the function base::TimeDelta::FromInternalValue().  We use
// it instead of base::TimeDelta because base::TimeDelta does not have a
// trivial constructor and thus cannot be used in a union.
%union { int64 time; }
%type <time> time time_with_units_required

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
%type <property_value> animation_delay_property_value
                       animation_direction_list_element
                       animation_direction_property_value
                       animation_duration_property_value
                       animation_fill_mode_list_element
                       animation_fill_mode_property_value
                       animation_iteration_count_list_element
                       animation_iteration_count_property_value
                       animation_name_list_element
                       animation_name_property_value
                       animation_timing_function_property_value
                       auto
                       background_color_property_value
                       background_image_property_list_element
                       background_image_property_value
                       background_position_property_list_element
                       background_position_property_value
                       background_repeat_element
                       background_repeat_property_value
                       background_size_property_list_element
                       background_size_property_value
                       border_color_property_value
                       border_radius_property_value
                       border_style_property_value
                       border_width_property_value
                       color_property_value
                       common_values
                       common_values_without_errors
                       content_property_value
                       display_property_value
                       font_face_local_src
                       font_face_src_list_element
                       font_face_src_property_value
                       font_face_url_src
                       font_family_name
                       font_family_property_value
                       font_size_property_value
                       font_style_property_value
                       font_weight_property_value
                       height_property_value
                       length_percent_property_value
                       line_height_property_value
                       line_style
                       linear_gradient_params
                       margin_side_property_value
                       margin_width
                       max_height_property_value
                       max_width_property_value
                       maybe_background_size_property_value
                       offset_property_value
                       opacity_property_value
                       orientation_media_feature_keyword_value
                       overflow_property_value
                       overflow_wrap_property_value
                       padding_side_property_value
                       position_property_value
                       positive_length_percent_property_value
                       scan_media_feature_keyword_value
                       tab_size_property_value
                       text_align_property_value
                       text_indent_property_value
                       text_overflow_property_value
                       text_transform_property_value
                       time_list_property_value
                       timing_function_list_property_value
                       transform_property_value
                       transition_delay_property_value
                       transition_duration_property_value
                       transition_property_property_value
                       transition_timing_function_property_value
                       unicode_range_property_value
                       url
                       vertical_align_property_value
                       visibility_property_value
                       white_space_property_value
                       width_property_value
                       z_index_property_value
%destructor { $$->Release(); } <property_value>

%union { MarginOrPaddingShorthand* margin_or_padding_shorthand; }
%type <margin_or_padding_shorthand> margin_property_value padding_property_value
%destructor { delete $$; } <margin_or_padding_shorthand>

%union { SingleAnimationShorthand* single_animation; }
%type <single_animation> single_animation single_non_empty_animation
%destructor { delete $$; } <single_animation>

%union { AnimationShorthandBuilder* animation_builder; }
%type <animation_builder> comma_separated_animation_list
%destructor { delete $$; } <animation_builder>

%union { AnimationShorthand* animation; }
%type <animation> animation_property_value
%destructor { delete $$; } <animation>

%union { TransitionShorthand* transition; }
%type <transition> transition_property_value
%destructor { delete $$; } <transition>

%type <real> alpha angle non_negative_number number

%union { cssom::CSSStyleSheet* style_sheet; }
%type <style_sheet> style_sheet
%destructor { $$->Release(); } <style_sheet>

%union { cssom::CSSRuleList* rule_list; }
%type <rule_list> rule_list rule_list_block
%destructor { $$->Release(); } <rule_list>

%type <string> unsupported_pseudo_class_token

%union { cssom::SimpleSelector* simple_selector; }
%type <simple_selector> class_selector_token
                        id_selector_token
                        pseudo_class_token
                        pseudo_element_token
                        simple_selector_token
                        type_selector_token
                        universal_selector_token
%destructor { delete $$; } <simple_selector>

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

%union { cssom::LinearGradientValue::SideOrCorner side_or_corner; }
%type <side_or_corner> side side_or_corner

%union { int sign; }
%type <sign> maybe_sign_token

%type <string> identifier_token

%union { cssom::PropertyKey property_key; }
%type <property_key> animatable_property_token

%union { cssom::CSSStyleDeclarationData* style_declaration_data; }
%type <style_declaration_data> style_declaration_list
%destructor { $$->Release(); } <style_declaration_data>

%union { cssom::CSSStyleDeclaration* style_declaration; }
%type <style_declaration> style_declaration_block
%destructor { $$->Release(); } <style_declaration>

%union { cssom::CSSFontFaceRule* font_face_rule; }
%type <font_face_rule> at_font_face_rule
%destructor { $$->Release(); } <font_face_rule>

%union { cssom::CSSKeyframeRule* keyframe_rule; }
%type <keyframe_rule> keyframe_rule
%destructor { $$->Release(); } <keyframe_rule>

%union { cssom::CSSKeyframesRule* keyframes_rule; }
%type <keyframes_rule> at_keyframes_rule
%destructor { $$->Release(); } <keyframes_rule>

%union { cssom::CSSRuleList* keyframe_rule_list; }
%type <keyframe_rule_list> keyframe_rule_list
%destructor { $$->Release(); } <keyframe_rule_list>

%union { float keyframe_offset; }
%type <keyframe_offset> keyframe_offset

%union { std::vector<float>* keyframe_selector; }
%type <keyframe_selector> keyframe_selector;
%destructor { delete $$; } <keyframe_selector>

%union { cssom::CSSFontFaceDeclarationData* font_face_declaration_data; }
%type <font_face_declaration_data> font_face_declaration_list
%destructor { $$->Release(); } <font_face_declaration_data>

%union { cssom::CSSMediaRule* media_rule; }
%type <media_rule> at_media_rule
%destructor { $$->Release(); } <media_rule>

%union { cssom::MediaList* media_list; }
%type <media_list> media_list
%destructor { $$->Release(); } <media_list>

%union { cssom::MediaQuery* media_query; }
%type <media_query> media_query
%destructor { $$->Release(); } <media_query>

%union { bool evaluated_media_type; }
%type <evaluated_media_type> evaluated_media_type media_type_specified

%union { cssom::MediaFeatures* media_features; }
%type <media_features> media_feature_list
%destructor { delete $$; } <media_features>

%union { cssom::MediaFeature* media_feature; }
%type <media_feature> media_feature media_feature_block
                      media_feature_with_value media_feature_without_value
                      media_feature_allowing_operator_with_value

%destructor { $$->Release(); } <media_feature>

%union { cssom::MediaFeatureOperator media_feature_operator; }
%type <media_feature_operator> media_feature_operator

%union { cssom::CSSStyleRule* style_rule; }
%type <style_rule> qualified_rule style_rule
%destructor { $$->Release(); } <style_rule>

%union { cssom::CSSRule* css_rule; }
%type <css_rule> rule
%destructor { $$->Release(); } <css_rule>

%union { cssom::PropertyListValue::Builder* property_list; }
%type <property_list> background_size_property_list
                      comma_separated_animation_direction_list
                      comma_separated_animation_fill_mode_list
                      comma_separated_animation_iteration_count_list
                      comma_separated_animation_name_list
                      comma_separated_background_image_list
                      comma_separated_font_face_src_list
                      comma_separated_font_family_name_list
                      comma_separated_unicode_range_list
                      validated_background_position_property
%destructor { delete $$; } <property_list>

%union { cssom::TransformFunction* transform_function; }
%type <transform_function> scale_function_parameters
%destructor { delete $$; } <transform_function>

%union { cssom::TransformFunctionListValue::Builder* transform_functions; }
%type <transform_functions> transform_list
%destructor { delete $$; } <transform_functions>

%union { cssom::TimeListValue::Builder* time_list; }
%type <time_list> comma_separated_time_list
%destructor { delete $$; } <time_list>

%union { cssom::PropertyKeyListValue::Builder* property_name_list; }
%type <property_name_list> comma_separated_animatable_property_name_list
%destructor { delete $$; } <property_name_list>

%union {
  cssom::SteppingTimingFunction::ValueChangeLocation
      stepping_value_change_location;
}
%type <stepping_value_change_location> maybe_steps_start_or_end_parameter

%union { cssom::TimingFunction* timing_function; }
%type <timing_function> single_timing_function
%destructor { $$->Release(); } <timing_function>

%union {
  cssom::TimingFunctionListValue::Builder*
      timing_function_list;
}
%type <timing_function_list>
    comma_separated_single_timing_function_list
%destructor { delete $$; } <timing_function_list>

%union { SingleTransitionShorthand* single_transition; }
%type <single_transition> single_transition single_non_empty_transition
%destructor { delete $$; } <single_transition>

%union { TransitionShorthandBuilder* transition_builder; }
%type <transition_builder> comma_separated_transition_list
%destructor { delete $$; } <transition_builder>

%union { BackgroundShorthandLayer* background_shorthand_layer; }
%type <background_shorthand_layer>
    background_position_and_size_shorthand_property_value
    background_repeat_shorthand_property_value
    background_position_and_repeat_combination
    final_background_layer_without_position_and_repeat
    final_background_layer
    background_property_value
%destructor { delete $$; } <background_shorthand_layer>

%union { BackgroundPositionInfo* background_position_info; }
%type <background_position_info> background_position_property_list
%destructor { delete $$; } <background_position_info>

%union { BorderShorthand* border_shorthand; }
%type <border_shorthand> border_property_value border_property_list
%destructor { delete $$; } <border_shorthand>

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

// The @font-face rule consists of the @font-face at-keyword followed by a block
// of descriptor declarations.
//   http://www.w3.org/TR/css3-fonts/#font-face-rule
at_font_face_rule:
    kFontFaceToken maybe_whitespace '{' maybe_whitespace
        font_face_declaration_list '}' maybe_whitespace {
    scoped_refptr<cssom::CSSFontFaceRule>
        font_face_rule(
            new cssom::CSSFontFaceRule(MakeScopedRefPtrAndRelease($5)));
    if (font_face_rule->IsValid()) {
      $$ = AddRef(font_face_rule.get());
    } else {
      parser_impl->LogWarning(@1, "invalid font-face");
      $$ = NULL;
    }
  }
  ;

// The @media rule is a conditional group rule whose condition is a media query.
// It consists of the at-keyword '@media' followed by a (possibly empty) media
// query list, followed by a group rule body. The condition of the rule is the
// result of the media query.
//   http://www.w3.org/TR/css3-conditional/#at-media
at_media_rule:
  // @media expr {}
    kMediaToken maybe_whitespace media_list rule_list_block {
    $$ = AddRef(new cssom::CSSMediaRule(MakeScopedRefPtrAndRelease($3),
                                        MakeScopedRefPtrAndRelease($4)));
  }
  ;

// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Media Rule, Media Query, Media Type
//   http://www.w3.org/TR/cssom/#media-queries
//   http://www.w3.org/TR/css3-mediaqueries/
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

// Follow the syntax defined here:
//   http://www.w3.org/TR/css3-mediaqueries/#syntax

// Orientation value, landscale or portrait
//   http://www.w3.org/TR/css3-mediaqueries/#orientation
orientation_media_feature_keyword_value:
    kLandscapeMediaFeatureKeywordValueToken maybe_whitespace {
    $$ = AddRef(cssom::MediaFeatureKeywordValue::GetLandscape().get());
  }
  | kPortraitMediaFeatureKeywordValueToken maybe_whitespace {
    $$ = AddRef(cssom::MediaFeatureKeywordValue::GetPortrait().get());
  }
  ;

// Scan value, interlace or progressive
//   http://www.w3.org/TR/css3-mediaqueries/#scan
scan_media_feature_keyword_value:
    kInterlaceMediaFeatureKeywordValueToken maybe_whitespace {
    $$ = AddRef(cssom::MediaFeatureKeywordValue::GetInterlace().get());
  }
  | kProgressiveMediaFeatureKeywordValueToken maybe_whitespace {
    $$ = AddRef(cssom::MediaFeatureKeywordValue::GetProgressive().get());
  }
  ;

// 'min-' or 'max-' prefix for media feature name.
media_feature_operator:
    kMediaMinimumToken { $$ = cssom::kMinimum; }
  | kMediaMaximumToken { $$ = cssom::kMaximum; }
  ;

// Media feature: 'feature' only.
// (feature) will evaluate to true if (feature:x) will evaluate to true for a
// value x other than zero or zero followed by a unit identifier.
//   http://www.w3.org/TR/css3-mediaqueries/#media1
media_feature_without_value:
    kLengthMediaFeatureTypeToken maybe_whitespace {
    $$ = AddRef(new cssom::MediaFeature($1));
  }
  | kOrientationMediaFeatureTypeToken maybe_whitespace {
    $$ = AddRef(new cssom::MediaFeature($1));
  }
  | kRatioMediaFeatureTypeToken maybe_whitespace {
    $$ = AddRef(new cssom::MediaFeature($1));
  }
  | kNonNegativeIntegerMediaFeatureTypeToken maybe_whitespace {
    $$ = AddRef(new cssom::MediaFeature($1));
  }
  | kResolutionMediaFeatureTypeToken maybe_whitespace {
    $$ = AddRef(new cssom::MediaFeature($1));
  }
  | kScanMediaFeatureTypeToken maybe_whitespace {
    $$ = AddRef(new cssom::MediaFeature($1));
  }
  | kZeroOrOneMediaFeatureTypeToken maybe_whitespace {
    $$ = AddRef(new cssom::MediaFeature($1));
  }
  ;

// Media feature: 'feature:value' for features that don't allow prefixes.
media_feature_with_value:
    kOrientationMediaFeatureTypeToken maybe_whitespace colon
    orientation_media_feature_keyword_value {
    $$ = AddRef(new cssom::MediaFeature($1, $4));
  }
  | kScanMediaFeatureTypeToken maybe_whitespace colon
    scan_media_feature_keyword_value {
    $$ = AddRef(new cssom::MediaFeature($1, $4));
  }
  | kZeroOrOneMediaFeatureTypeToken maybe_whitespace colon zero_or_one {
    $$ = AddRef(new cssom::MediaFeature($1,
                                        AddRef(new cssom::NumberValue($4))));
  }
  ;

// Media feature: 'feature:value' for features that allow min/max prefixes.
media_feature_allowing_operator_with_value:
    kLengthMediaFeatureTypeToken maybe_whitespace colon length {
    $$ = AddRef(new cssom::MediaFeature($1, MakeScopedRefPtrAndRelease($4)));
  }
  | kNonNegativeIntegerMediaFeatureTypeToken maybe_whitespace colon
    non_negative_integer {
    $$ = AddRef(new cssom::MediaFeature($1,
                                        AddRef(new cssom::NumberValue($4))));
  }
  | kRatioMediaFeatureTypeToken maybe_whitespace colon ratio {
    $$ = AddRef(new cssom::MediaFeature($1, MakeScopedRefPtrAndRelease($4)));
  }
  | kResolutionMediaFeatureTypeToken maybe_whitespace colon resolution {
    $$ = AddRef(new cssom::MediaFeature($1, MakeScopedRefPtrAndRelease($4)));
  }
  ;

// Media feature: 'feature' or 'feature:value' or 'min-feature:value' or
// 'max-feature:value'
//   http://www.w3.org/TR/css3-mediaqueries/#media1
media_feature:
    media_feature_without_value
  | media_feature_with_value
  | media_feature_allowing_operator_with_value
  | media_feature_operator media_feature_allowing_operator_with_value {
    $$ = $2;
    $$->set_operator($1);
  }
  ;

// Media feature: '(name:value)'
media_feature_block:
    '(' maybe_whitespace media_feature ')' maybe_whitespace {
    $$ = $3;
  }
  ;

// Media feature list: '(name:value) [ and (name:value) ]'
media_feature_list:
  // (name:value)
    media_feature_block {
    $$ = new cssom::MediaFeatures();
    $$->push_back(MakeScopedRefPtrAndRelease($1));
  }
  // ... and (name:value)
  | media_feature_list kMediaAndToken maybe_whitespace media_feature_block {
    $$ = $1;
    $$->push_back(MakeScopedRefPtrAndRelease($4));
  }
  ;

// All tokens representing unknown media types.
media_type_unknown:
    kIdentifierToken
  // "Media query" mode operator names.
  | kMediaAndToken
  | kMediaMinimumToken
  | kMediaMaximumToken
  // "Media query" mode media feature tokens
  | kLengthMediaFeatureTypeToken
  | kOrientationMediaFeatureTypeToken
  | kRatioMediaFeatureTypeToken
  | kNonNegativeIntegerMediaFeatureTypeToken
  | kResolutionMediaFeatureTypeToken
  | kScanMediaFeatureTypeToken
  | kZeroOrOneMediaFeatureTypeToken
  ;

// All tokens representing media types that are true on this platform.
media_type_known:
    kAllMediaTypeToken
  | kTVMediaTypeToken
  | kScreenMediaTypeToken
  ;

// Returns true for known specified media types, otherwise false.
media_type_specified:
    media_type_unknown {
    $$ = false;
  }
  | media_type_known {
    $$ = true;
  }
  ;
// The names chosen for CSS media types reflect target devices for which the
// relevant properties make sense
//   http://www.w3.org/TR/CSS21/media.html#media-types
//   http://www.w3.org/TR/css3-mediaqueries/#media0
evaluated_media_type:
  // @media [type]...
    media_type_specified {
    $$ = $1;
  }
  // @media not [type]...
  | kMediaNotToken kWhitespaceToken media_type_specified {
    $$ = !$3;
  }
  // @media only [type]...
  | kMediaOnlyToken kWhitespaceToken media_type_specified {
    $$ = $3;
  }
  // @media only not ... (special case)
  | kMediaOnlyToken kWhitespaceToken kMediaNotToken {
    $$ = false;
  }
  // @media not only ... (special case)
  | kMediaNotToken kWhitespaceToken kMediaOnlyToken {
    $$ = true;
  }
  ;

// A media query consists of a media type and zero or more expressions that
// check for the conditions of particular media features.
//   http://www.w3.org/TR/cssom/#media-queries
//   http://www.w3.org/TR/css3-mediaqueries/#media0
media_query:
  // @media  {}
    /* empty */ {
    $$ = AddRef(new cssom::MediaQuery(true));
  }
  // @media (name:value)... {}
  | media_feature_list {
    scoped_ptr<cssom::MediaFeatures> media_features($1);
    $$ = AddRef(new cssom::MediaQuery(true, media_features.Pass()));
  }
  // @media mediatype {}
  | evaluated_media_type maybe_whitespace {
    $$ = AddRef(new cssom::MediaQuery($1));
  }
  // @media mediatype and (name:value)... {}
  | evaluated_media_type maybe_whitespace kMediaAndToken maybe_whitespace
    media_feature_list {
    scoped_ptr<cssom::MediaFeatures> media_features($5);
    $$ = AddRef(new cssom::MediaQuery($1, media_features.Pass()));
  }
  // When an unknown media feature, an unknown media feature value, a malformed
  // media query, or unexpected tokens is found, the media query must be
  // represented as 'not all'.
  //   http://www.w3.org/TR/css3-mediaqueries/#error-handling
  | errors {
    $$ = AddRef(new cssom::MediaQuery(true));
  }
  ;

// Several media queries can be combined in a media query list.
//   http://www.w3.org/TR/cssom/#medialist
//   http://www.w3.org/TR/css3-mediaqueries/#media0
media_list:
    media_query {
    $$ = AddRef(new cssom::MediaList(parser_impl->css_parser()));
    $$->Append(MakeScopedRefPtrAndRelease($1));
  }
  | media_list comma media_query {
    $$ = $1;
    $$->Append(MakeScopedRefPtrAndRelease($3));
  }
  ;

// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Keyframe Rule and Keyframes Rule
//   http://www.w3.org/TR/2013/WD-css3-animations-20130219/#CSSKeyframeRule-interface
//   http://www.w3.org/TR/2013/WD-css3-animations-20130219/#CSSKeyframesRule-interface
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

at_keyframes_rule:
    kKeyframesToken maybe_whitespace kIdentifierToken maybe_whitespace '{'
    maybe_whitespace keyframe_rule_list '}' maybe_whitespace {
    $$ = AddRef(new cssom::CSSKeyframesRule($3.ToString(),
                                            MakeScopedRefPtrAndRelease($7)));
  }
  ;

keyframe_rule_list:
    keyframe_rule {
    $$ = AddRef(new cssom::CSSRuleList());
    $$->AppendCSSRule(MakeScopedRefPtrAndRelease($1));
  }
  | keyframe_rule_list keyframe_rule {
    $$ = $1;
    $$->AppendCSSRule(MakeScopedRefPtrAndRelease($2));
  }
  ;

keyframe_rule:
    keyframe_selector style_declaration_block {
    scoped_ptr<std::vector<float> > offsets($1);

    $$ = AddRef(new cssom::CSSKeyframeRule(
        *offsets, MakeScopedRefPtrAndRelease($2)));
  }
  ;

keyframe_selector:
    keyframe_offset {
    $$ = new std::vector<float>(1, $1);
  }
  | keyframe_selector ',' maybe_whitespace keyframe_offset {
    $$ = $1;
    $$->push_back($4);
  }
  ;

keyframe_offset:
    kFromToken maybe_whitespace {
    $$ = 0.0f;
  }
  | kToToken maybe_whitespace {
    $$ = 1.0f;
  }
  | kPercentageToken maybe_whitespace {
    $$ = $1 / 100.0f;
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
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kAllProperty));
  }
  | kAnimationDelayToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kAnimationDelayProperty));
  }
  | kAnimationDirectionToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kAnimationDirectionProperty));
  }
  | kAnimationDurationToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kAnimationDurationProperty));
  }
  | kAnimationFillModeToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kAnimationFillModeProperty));
  }
  | kAnimationIterationCountToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kAnimationIterationCountProperty));
  }
  | kAnimationNameToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kAnimationNameProperty));
  }
  | kAnimationTimingFunctionToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kAnimationTimingFunctionProperty));
  }
  | kAnimationToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kAnimationProperty));
  }
  | kBackgroundColorToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBackgroundColorProperty));
  }
  | kBackgroundImageToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBackgroundImageProperty));
  }
  | kBackgroundPositionToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBackgroundPositionProperty));
  }
  | kBackgroundRepeatToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBackgroundRepeatProperty));
  }
  | kBackgroundSizeToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBackgroundSizeProperty));
  }
  | kBackgroundToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBackgroundProperty));
  }
  | kBorderToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBorderProperty));
  }
  | kBorderColorToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBorderColorProperty));
  }
  | kBorderRadiusToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBorderRadiusProperty));
  }
  | kBorderStyleToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBorderStyleProperty));
  }
  | kBorderWidthToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBorderWidthProperty));
  }
  | kBottomToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kBottomProperty));
  }
  | kColorToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kColorProperty));
  }
  | kContentToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kContentProperty));
  }
  | kDisplayToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kDisplayProperty));
  }
  | kFontFamilyToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kFontFamilyProperty));
  }
  | kFontSizeToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kFontSizeProperty));
  }
  | kFontWeightToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kFontWeightProperty));
  }
  | kHeightToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kHeightProperty));
  }
  | kLeftToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kLeftProperty));
  }
  | kLineHeightToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kLineHeightProperty));
  }
  | kMarginBottomToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kMarginBottomProperty));
  }
  | kMarginLeftToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kMarginLeftProperty));
  }
  | kMarginRightToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kMarginRightProperty));
  }
  | kMarginToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kMarginProperty));
  }
  | kMarginTopToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kMarginTopProperty));
  }
  | kMaxHeightToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kMaxHeightProperty));
  }
  | kMaxWidthToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kMaxWidthProperty));
  }
  | kMinHeightToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kMinHeightProperty));
  }
  | kMinWidthToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kMinWidthProperty));
  }
  | kOpacityToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kOpacityProperty));
  }
  | kOverflowToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kOverflowProperty));
  }
  | kOverflowWrapToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kOverflowWrapProperty));
  }
  | kPaddingBottomToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kPaddingBottomProperty));
  }
  | kPaddingLeftToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kPaddingLeftProperty));
  }
  | kPaddingRightToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kPaddingRightProperty));
  }
  | kPaddingToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kPaddingProperty));
  }
  | kPaddingTopToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kPaddingTopProperty));
  }
  | kPositionToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kPositionProperty));
  }
  | kRightToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kRightProperty));
  }
  | kSrcToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kSrcProperty));
  }
  | kTabSizeToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTabSizeProperty));
  }
  | kTextAlignToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTextAlignProperty));
  }
  | kTextIndentToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTextIndentProperty));
  }
  | kTextOverflowToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTextOverflowProperty));
  }
  | kTextTransformToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTextTransformProperty));
  }
  | kTopToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTopProperty));
  }
  | kTransformToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTransformProperty));
  }
  | kTransitionDelayToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTransitionDelayProperty));
  }
  | kTransitionDurationToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTransitionDurationProperty));
  }
  | kTransitionPropertyToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTransitionPropertyProperty));
  }
  | kTransitionTimingFunctionToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTransitionTimingFunctionProperty));
  }
  | kTransitionToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kTransitionProperty));
  }
  | kUnicodeRangePropertyToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kUnicodeRangeProperty));
  }
  | kVerticalAlignToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kVerticalAlignProperty));
  }
  | kVisibilityToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kVisibilityProperty));
  }
  | kWhiteSpacePropertyToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kWhiteSpaceProperty));
  }
  | kWidthToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kWidthProperty));
  }
  | kZIndexToken {
    $$ = TrivialStringPiece::FromCString(
            cssom::GetPropertyName(cssom::kZIndexProperty));
  }
  // Property values.
  | kAbsoluteToken {
    $$ = TrivialStringPiece::FromCString(cssom::kAbsoluteKeywordName);
  }
  | kAlternateToken {
    $$ = TrivialStringPiece::FromCString(cssom::kAlternateKeywordName);
  }
  | kAlternateReverseToken {
    $$ = TrivialStringPiece::FromCString(cssom::kAlternateReverseKeywordName);
  }
  | kAquaToken {
    $$ = TrivialStringPiece::FromCString(cssom::kAquaKeywordName);
  }
  | kAutoToken {
    $$ = TrivialStringPiece::FromCString(cssom::kAutoKeywordName);
  }
  | kBackwardsToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBackwardsKeywordName);
  }
  | kBaselineToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBaselineKeywordName);
  }
  | kBlackToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBlackKeywordName);
  }
  | kBlockToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBlockKeywordName);
  }
  | kBlueToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBlueKeywordName);
  }
  | kBoldToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBoldKeywordName);
  }
  | kBothToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBothKeywordName);
  }
  // A rule for kBottomToken is already defined for the matching property name.
  | kBreakWordToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBreakWordKeywordName);
  }
  | kCenterToken {
    $$ = TrivialStringPiece::FromCString(cssom::kCenterKeywordName);
  }
  | kClipToken {
    $$ = TrivialStringPiece::FromCString(cssom::kClipKeywordName);
  }
  | kContainToken {
    $$ = TrivialStringPiece::FromCString(cssom::kContainKeywordName);
  }
  | kCoverToken {
    $$ = TrivialStringPiece::FromCString(cssom::kCoverKeywordName);
  }
  | kCursiveToken {
    $$ = TrivialStringPiece::FromCString(cssom::kCursiveKeywordName);
  }
  | kEaseInOutToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEaseInOutKeywordName);
  }
  | kEaseInToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEaseInKeywordName);
  }
  | kEaseOutToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEaseOutKeywordName);
  }
  | kEaseToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEaseKeywordName);
  }
  | kEllipsisToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEllipsisKeywordName);
  }
  | kEndToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEndKeywordName);
  }
  | kFantasyToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFantasyKeywordName);
  }
  | kFixedToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFixedKeywordName);
  }
  | kForwardsToken {
    $$ = TrivialStringPiece::FromCString(cssom::kForwardsKeywordName);
  }
  | kFromToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFromKeywordName);
  }
  | kFuchsiaToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFuchsiaKeywordName);
  }
  | kGrayToken {
    $$ = TrivialStringPiece::FromCString(cssom::kGrayKeywordName);
  }
  | kGreenToken {
    $$ = TrivialStringPiece::FromCString(cssom::kGreenKeywordName);
  }
  | kHiddenToken {
    $$ = TrivialStringPiece::FromCString(cssom::kHiddenKeywordName);
  }
  | kInfiniteToken {
    $$ = TrivialStringPiece::FromCString(cssom::kInfiniteKeywordName);
  }
  | kInheritToken {
    $$ = TrivialStringPiece::FromCString(cssom::kInheritKeywordName);
  }
  | kInitialToken {
    $$ = TrivialStringPiece::FromCString(cssom::kInitialKeywordName);
  }
  | kInlineBlockToken {
    $$ = TrivialStringPiece::FromCString(cssom::kInlineBlockKeywordName);
  }
  | kInlineToken {
    $$ = TrivialStringPiece::FromCString(cssom::kInlineKeywordName);
  }
  | kItalicToken {
    $$ = TrivialStringPiece::FromCString(cssom::kItalicKeywordName);
  }
  | kLimeToken {
    $$ = TrivialStringPiece::FromCString(cssom::kLimeKeywordName);
  }
  | kLinearToken {
    $$ = TrivialStringPiece::FromCString(cssom::kLinearKeywordName);
  }
  | kMaroonToken {
    $$ = TrivialStringPiece::FromCString(cssom::kMaroonKeywordName);
  }
  | kMiddleToken {
    $$ = TrivialStringPiece::FromCString(cssom::kMiddleKeywordName);
  }
  | kMonospaceToken {
    $$ = TrivialStringPiece::FromCString(cssom::kMonospaceKeywordName);
  }
  | kNavyToken {
    $$ = TrivialStringPiece::FromCString(cssom::kNavyKeywordName);
  }
  | kNoneToken {
    $$ = TrivialStringPiece::FromCString(cssom::kNoneKeywordName);
  }
  | kNoRepeatToken {
    $$ = TrivialStringPiece::FromCString(cssom::kNoRepeatKeywordName);
  }
  | kNormalToken {
    $$ = TrivialStringPiece::FromCString(cssom::kNormalKeywordName);
  }
  | kNoWrapToken {
    $$ = TrivialStringPiece::FromCString(cssom::kNoWrapKeywordName);
  }
  | kObliqueToken {
    $$ = TrivialStringPiece::FromCString(cssom::kObliqueKeywordName);
  }
  | kOliveToken {
    $$ = TrivialStringPiece::FromCString(cssom::kOliveKeywordName);
  }
  | kPreToken {
    $$ = TrivialStringPiece::FromCString(cssom::kPreKeywordName);
  }
  | kPurpleToken {
    $$ = TrivialStringPiece::FromCString(cssom::kPurpleKeywordName);
  }
  | kRedToken {
    $$ = TrivialStringPiece::FromCString(cssom::kRedKeywordName);
  }
  | kRepeatToken {
    $$ = TrivialStringPiece::FromCString(cssom::kRepeatKeywordName);
  }
  | kRepeatXToken {
    $$ = TrivialStringPiece::FromCString(cssom::kRepeatXKeywordName);
  }
  | kRepeatYToken {
    $$ = TrivialStringPiece::FromCString(cssom::kRepeatYKeywordName);
  }
  | kRelativeToken {
    $$ = TrivialStringPiece::FromCString(cssom::kRelativeKeywordName);
  }
  | kReverseToken {
    $$ = TrivialStringPiece::FromCString(cssom::kReverseKeywordName);
  }
  | kSansSerifToken {
    $$ = TrivialStringPiece::FromCString(cssom::kSansSerifKeywordName);
  }
  | kSerifToken {
    $$ = TrivialStringPiece::FromCString(cssom::kSerifKeywordName);
  }
  | kSilverToken {
    $$ = TrivialStringPiece::FromCString(cssom::kSilverKeywordName);
  }
  | kSolidToken {
    $$ = TrivialStringPiece::FromCString(cssom::kSolidKeywordName);
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
  | kTealToken {
    $$ = TrivialStringPiece::FromCString(cssom::kTealKeywordName);
  }
  | kToToken {
    $$ = TrivialStringPiece::FromCString(cssom::kToKeywordName);
  }
  // A rule for kTopToken is already defined for the matching property name.
  | kTransparentToken {
    $$ = TrivialStringPiece::FromCString(cssom::kTransparentKeywordName);
  }
  | kUppercaseToken {
    $$ = TrivialStringPiece::FromCString(cssom::kUppercaseKeywordName);
  }
  | kVisibleToken {
    $$ = TrivialStringPiece::FromCString(cssom::kVisibleKeywordName);
  }
  | kWhiteToken {
    $$ = TrivialStringPiece::FromCString(cssom::kWhiteKeywordName);
  }
  | kYellowToken {
    $$ = TrivialStringPiece::FromCString(cssom::kYellowKeywordName);
  }
  // Pseudo-class names.
  | kActiveToken {
    $$ = TrivialStringPiece::FromCString(cssom::kActivePseudoClassName);
  }
  | kEmptyToken {
    $$ = TrivialStringPiece::FromCString(cssom::kEmptyPseudoClassName);
  }
  | kFocusToken {
    $$ = TrivialStringPiece::FromCString(cssom::kFocusPseudoClassName);
  }
  | kHoverToken {
    $$ = TrivialStringPiece::FromCString(cssom::kHoverPseudoClassName);
  }
  // Pseudo-element names.
  | kAfterToken {
    $$ = TrivialStringPiece::FromCString(cssom::kAfterPseudoElementName);
  }
  | kBeforeToken {
    $$ = TrivialStringPiece::FromCString(cssom::kBeforePseudoElementName);
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

// Allows to indicate single elements or all elements at regularly-spaced
// intervals in a list.
//   http://www.w3.org/TR/css-syntax-3/#anb
an_plus_b:
    integer
  | kNthToken maybe_whitespace
  ;

// Gracefully handling known but unsupported pseudo-classes to work around
// violation of Cobalt API subset by ***REMOVED***.
//
// TODO(***REMOVED***): This is a non-compliant behavior, invalid selectors should
//               cause the entire rule to be rejected. Remove this reduction
//               once all tracking bugs below are resolved.
unsupported_pseudo_class_token:
  // Tracked by b/19149783.
    kNthChildFunctionToken an_plus_b ')' {
    $$ = TrivialStringPiece::FromCString("nth-child");
  }
  // Tracked by b/19149783.
  | kNthLastChildFunctionToken an_plus_b ')' {
    $$ = TrivialStringPiece::FromCString("nth-last-child");
  }
  // Tracked by b/19149783.
  | kNthLastOfTypeFunctionToken an_plus_b ')' {
    $$ = TrivialStringPiece::FromCString("nth-last-of-type");
  }
  // Tracked by b/19149783.
  | kNthOfTypeFunctionToken an_plus_b ')' {
    $$ = TrivialStringPiece::FromCString("nth-of-type");
  }
  ;

// The pseudo-class concept is introduced to permit selection based on
// information that lies outside of the document tree or that can be awkward or
// impossible to express using the other simple selectors.
//   http://www.w3.org/TR/selectors4/#pseudo-classes
pseudo_class_token:
  // The :active pseudo-class applies while an element is being activated by the
  // user. For example, between the times the user presses the mouse button and
  // releases it. On systems with more than one mouse button, :active applies
  // only to the primary or primary activation button (typically the "left"
  // mouse button), and any aliases thereof.
  //   http://www.w3.org/TR/selectors4/#active-pseudo
    ':' kActiveToken {
    $$ = new cssom::ActivePseudoClass();
  }
  // The :empty pseudo-class represents an element that has no children. In
  // terms of the document tree, only element nodes and content nodes (such as
  // DOM text nodes, CDATA nodes, and entity references) whose data has a
  // non-zero length must be considered as affecting emptiness; comments,
  // processing instructions, and other nodes must not affect whether an element
  // is considered empty or not.
  //   http://www.w3.org/TR/selectors4/#empty-pseudo
  | ':' kEmptyToken {
    $$ = new cssom::EmptyPseudoClass();
  }
  // The :focus pseudo-class applies while an element has the focus (accepts
  // keyboard or mouse events, or other forms of input).
  //   http://www.w3.org/TR/selectors4/#focus-pseudo
  | ':' kFocusToken {
    $$ = new cssom::FocusPseudoClass();
  }
  // The :hover pseudo-class applies while the user designates an element with a
  // pointing device, but does not necessarily activate it. For example, a
  // visual user agent could apply this pseudo-class when the cursor (mouse
  // pointer) hovers over a box generated by the element. Interactive user
  // agents that cannot detect hovering due to hardware limitations (e.g., a pen
  // device that does not detect hovering) are still conforming.
  //   http://www.w3.org/TR/selectors4/#hover-pseudo
  | ':' kHoverToken {
    $$ = new cssom::HoverPseudoClass();
  }
  // The negation pseudo-class, :not(), is a functional pseudo-class taking a
  // selector list as an argument. It represents an element that is not
  // represented by its argument.
  //   http://www.w3.org/TR/selectors4/#negation-pseudo
  | ':' kNotFunctionToken compound_selector_token ')' {
    scoped_ptr<cssom::CompoundSelector> compound_selector($3);
    scoped_ptr<cssom::NotPseudoClass> not_pseudo_class(new
        cssom::NotPseudoClass());
    not_pseudo_class->set_selector(compound_selector.Pass());
    $$ = not_pseudo_class.release();
  }
  | ':' kNotFunctionToken errors ')' {
    parser_impl->LogWarning(@1, "unsupported selector within :not()");
    $$ = NULL;
  }
  | ':' unsupported_pseudo_class_token {
#ifdef __LB_SHELL__FORCE_LOGGING__
    DCHECK(non_trivial_static_fields.Get().
        thread_checker.CalledOnValidThread());
    base::hash_set<std::string>& pseudo_classes_warned_about =
        non_trivial_static_fields.Get().pseudo_classes_warned_about;

    // TODO(***REMOVED***): Stop deduplicating warnings after fixing CSS in ***REMOVED***
    //               (b/24955665).
    std::string pseudo_class_name = $2.ToString();
    if (pseudo_classes_warned_about.find(pseudo_class_name) ==
        pseudo_classes_warned_about.end()) {
      pseudo_classes_warned_about.insert(pseudo_class_name);
      parser_impl->LogWarning(@1,
                              "unsupported pseudo-class " + pseudo_class_name);
    }
#endif  // __LB_SHELL__FORCE_LOGGING__

    $$ = new cssom::UnsupportedPseudoClass();
  }
  | ':' errors {
    parser_impl->LogWarning(@1, "unsupported pseudo-class");
    $$ = NULL;
  }
  ;

// Pseudo-elements create abstractions about the document tree beyond those
// specified by the document language.
//   http://www.w3.org/TR/selectors4/#pseudo-elements
pseudo_element_token:
// Authors specify the style and location of generated content with the
// :before and :after pseudo-elements.
//   http://www.w3.org/TR/CSS21/generate.html#before-after-content
// User agents must also accept the previous one-colon notation for
// pseudo-elements introduced in CSS levels 1 and 2.
//   http://www.w3.org/TR/selectors4/#pseudo-elements
    ':' kAfterToken {
    $$ = new cssom::AfterPseudoElement();
  }
  | ':' kBeforeToken {
    $$ = new cssom::BeforePseudoElement();
  }
  | ':' ':' kAfterToken {
    $$ = new cssom::AfterPseudoElement();
  }
  | ':' ':' kBeforeToken {
    $$ = new cssom::BeforePseudoElement();
  }
  | ':' ':' errors {
    parser_impl->LogWarning(@1, "unsupported pseudo-element");
    $$ = NULL;
  }
  ;

// A simple selector represents an aspect of an element to be matched against.
//   http://www.w3.org/TR/selectors4/#simple
simple_selector_token:
    class_selector_token
  | id_selector_token
  | pseudo_class_token
  | pseudo_element_token
  | type_selector_token
  | universal_selector_token
  ;

// A compound selector is a chain of simple selectors that are not separated by
// a combinator.
//   http://www.w3.org/TR/selectors4/#compound
compound_selector_token:
    simple_selector_token {
    scoped_ptr<cssom::SimpleSelector> simple_selector($1);

    if (simple_selector) {
      $$ = new cssom::CompoundSelector();
      $$->AppendSelector(simple_selector.Pass());
    } else {
      $$ = NULL;
    }
  }
  | compound_selector_token simple_selector_token {
    scoped_ptr<cssom::CompoundSelector> compound_selector($1);
    scoped_ptr<cssom::SimpleSelector> simple_selector($2);

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

auto:
    kAutoToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetAuto().get());
  }

// The scanner that we adopted from WebKit was built with assumption that sign
// is handled in the grammar. Practically this means that tokens of <number>
// and <real> types has to be prepended with this rule.
//   http://www.w3.org/TR/css3-syntax/#consume-a-number
maybe_sign_token:
    /* empty */ { $$ = 1; }
  | '+' { $$ = 1; }
  | '-' { $$ = -1; }
  ;

// This is for integers that can only have the values 0 or 1, including '-0'.
// As used in the 'grid' css media feature:
//   http://www.w3.org/TR/css3-mediaqueries/#grid
zero_or_one:
    integer {
    if (($1 < 0) || ($1 > 1)) {
      parser_impl->LogError(@1, "integer value must be 0 or 1");
      YYERROR;
    } else {
      $$ = $1;
    }
  }
  ;

// An integer is one or more decimal digits "0" through "9". The first digit
// of an integer may be immediately preceded by "-" or "+" to indicate the sign.
//   http://www.w3.org/TR/css3-values/#integers
integer:
    maybe_sign_token kIntegerToken maybe_whitespace {
    $$ = $1 * $2;
  }
  ;

// Wrap the |integer| in order to validate if the integer is not negative
non_negative_integer:
    integer {
    if ($1 < 0) {
      parser_impl->LogError(@1, "integer value must not be negative");
      YYERROR;
    } else {
      $$ = $1;
    }
  }
  ;

// Wrap the |integer| in order to validate if the integer is positive
positive_integer:
    integer {
    if ($1 < 1) {
      parser_impl->LogError(@1, "integer value must be positive");
      YYERROR;
    } else {
      $$ = $1;
    }
  }
  ;

// A number is either an |integer| or zero or more decimal digits followed
// by a dot followed by one or more decimal digits.
//   http://www.w3.org/TR/css3-values/#numbers
number:
    integer { $$ = $1; }
  | maybe_sign_token kRealToken maybe_whitespace { $$ = $1 * $2; }
  ;

// Wrap |number| and validates that it is not negative.
non_negative_number:
    number {
    if ($1 < 0) {
      parser_impl->LogError(@1, "number value must not be negative");
      YYERROR;
    } else {
      $$ = $1;
    }
  }
  ;

// Percentage values are always relative to another value, for example a length.
// Each property that allows percentages also defines the value to which
// the percentage refers.
//   http://www.w3.org/TR/css3-values/#percentages
percentage:
    maybe_sign_token kPercentageToken maybe_whitespace {
    $$ = AddRef(new cssom::PercentageValue($1 * $2 / 100));
  }
  ;

// Wrap the |percentage| in order to validate if the percentage is positive.
positive_percentage:
    percentage {
    scoped_refptr<cssom::PercentageValue> percentage =
        MakeScopedRefPtrAndRelease($1);
    if (percentage && percentage->value() < 0) {
      $$ = NULL;
      parser_impl->LogError(@1, "negative values of percentage are illegal");
      YYERROR;
    } else {
      $$ = AddRef(percentage.get());
    }
  }
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

positive_length:
  length {
    scoped_refptr<cssom::LengthValue> length = MakeScopedRefPtrAndRelease($1);
    if (length && length->value() < 0) {
      $$ = NULL;
      parser_impl->LogError(@1, "negative values of length are illegal");
      YYERROR;
    } else {
      $$ = AddRef(length.get());
    }
  }
  ;

// Ratio units.
//   http://www.w3.org/TR/css3-mediaqueries/#values
ratio:
    positive_integer '/' maybe_whitespace positive_integer {
    $$ = AddRef(new cssom::RatioValue(math::Rational($1, $4)));
  }
  ;

// Resolution units.
//   http://www.w3.org/TR/css3-mediaqueries/#resolution0
resolution:
    maybe_sign_token kDotsPerInchToken maybe_whitespace {
    float value = $1 * $2;
    if (value <= 0) {
      parser_impl->LogError(@1, "resolution value must be positive");
      YYERROR;
    }
    $$ = AddRef(new cssom::ResolutionValue(value, cssom::kDPIUnit));
  }
  | maybe_sign_token kDotsPerCentimeterToken maybe_whitespace {
    float value = $1 * $2;
    if (value <= 0) {
      parser_impl->LogError(@1, "resolution value must be positive");
      YYERROR;
    }
    $$ = AddRef(new cssom::ResolutionValue(value, cssom::kDPCMUnit));
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
  | time_with_units_required
  ;

time_with_units_required:
  maybe_sign_token kSecondsToken maybe_whitespace {
    $$ = base::TimeDelta::FromMilliseconds(
             static_cast<int64>($1 * $2 * base::Time::kMillisecondsPerSecond)).
             ToInternalValue();
  }
  | maybe_sign_token kMillisecondsToken maybe_whitespace {
    $$ = base::TimeDelta::FromMilliseconds(static_cast<int64>($1 * $2)).
             ToInternalValue();
  }
  ;

colon: ':' maybe_whitespace ;
comma: ',' maybe_whitespace ;

// All properties accept the CSS-wide keywords.
//   http://www.w3.org/TR/css3-values/#common-keywords
common_values:
    common_values_without_errors
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

common_values_without_errors:
    kInheritToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetInherit().get());
  }
  | kInitialToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetInitial().get());
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
        uint32 rgb = ParseHexToken($1);
        uint32 r = (rgb & 0xf00) >> 8;
        uint32 g = (rgb & 0x0f0) >> 4;
        uint32 b = (rgb & 0x00f);
        $$ = AddRef(new cssom::RGBAColorValue((r << 28) | (r << 24) |
                                              (g << 20) | (g << 16) |
                                              (b << 12) | (b << 8) |
                                              0xff));
        break;
      }
      case 6: {
        uint32 rgb = ParseHexToken($1);
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
    uint8 r = static_cast<uint8>(ClampToRange(0, 255, $3));
    uint8 g = static_cast<uint8>(ClampToRange(0, 255, $5));
    uint8 b = static_cast<uint8>(ClampToRange(0, 255, $7));
    $$ = AddRef(new cssom::RGBAColorValue(r, g, b, 0xff));
  }
  // RGB color model with opacity.
  //   http://www.w3.org/TR/css3-color/#rgba-color
  | kRGBAFunctionToken maybe_whitespace integer comma integer comma integer
      comma alpha ')' maybe_whitespace {
    uint8 r = static_cast<uint8>(ClampToRange(0, 255, $3));
    uint8 g = static_cast<uint8>(ClampToRange(0, 255, $5));
    uint8 b = static_cast<uint8>(ClampToRange(0, 255, $7));
    float a = $9;  // Already clamped.
    $$ = AddRef(
        new cssom::RGBAColorValue(r, g, b, static_cast<uint8>(a * 0xff)));
  }
  | kAquaToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetAqua().get());
  }
  | kBlackToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetBlack().get());
  }
  | kBlueToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetBlue().get());
  }
  | kFuchsiaToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetFuchsia().get());
  }
  | kGrayToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetGray().get());
  }
  | kGreenToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetGreen().get());
  }
  | kLimeToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetLime().get());
  }
  | kMaroonToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetMaroon().get());
  }
  | kNavyToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetNavy().get());
  }
  | kOliveToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetOlive().get());
  }
  | kPurpleToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetPurple().get());
  }
  | kRedToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetRed().get());
  }
  | kSilverToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetSilver().get());
  }
  | kTealToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetTeal().get());
  }
  | kTransparentToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetTransparent().get());
  }
  | kWhiteToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetWhite().get());
  }
  | kYellowToken maybe_whitespace {
    $$ = AddRef(cssom::RGBAColorValue::GetYellow().get());
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

length_percent_property_value:
    length {
    $$ = $1;
  }
  | percentage {
    $$ = $1;
  }
  ;

positive_length_percent_property_value:
    positive_length {
    $$ = $1;
  }
  | positive_percentage {
    $$ = $1;
  }
  ;

background_property_element:
    color {
    if (!$<background_shorthand_layer>0->background_color) {
      $<background_shorthand_layer>0->background_color =
          MakeScopedRefPtrAndRelease($1);
    } else {
      parser_impl->LogError(
          @1, "background-color value declared twice in background.");
    }
  }
  | background_image_property_list_element {
    if (!$<background_shorthand_layer>0->background_image) {
      scoped_ptr<cssom::PropertyListValue::Builder>
          background_image_builder(new cssom::PropertyListValue::Builder());
      background_image_builder->reserve(1);
      background_image_builder->push_back(MakeScopedRefPtrAndRelease($1));
      $<background_shorthand_layer>0->background_image =
          new cssom::PropertyListValue(background_image_builder.Pass());
    } else {
      parser_impl->LogError(
          @1, "background-image value declared twice in background.");
    }
  }
  ;

maybe_background_size_property_value:
    /* empty */ {
    $$ = NULL;
  }
  | '/' maybe_whitespace background_size_property_value {
    $$ = $3;
  }
  ;

background_position_and_size_shorthand_property_value:
    background_position_property_value maybe_background_size_property_value {
    scoped_ptr<BackgroundShorthandLayer> shorthand_layer(
        new BackgroundShorthandLayer());

    shorthand_layer->background_position = MakeScopedRefPtrAndRelease($1);
    if ($2) {
      shorthand_layer->background_size = MakeScopedRefPtrAndRelease($2);
    }

    $$ = shorthand_layer.release();
  }
  ;

background_repeat_shorthand_property_value:
    background_repeat_property_value {
    scoped_ptr<BackgroundShorthandLayer> shorthand_layer(
        new BackgroundShorthandLayer());
    shorthand_layer->background_repeat = MakeScopedRefPtrAndRelease($1);
    $$ = shorthand_layer.release();
  }
  ;


// 'background-size' property should follow with 'background-position' property
//  and a '/'.
background_position_and_repeat_combination:
    background_position_and_size_shorthand_property_value
  | background_repeat_shorthand_property_value
  | background_position_and_size_shorthand_property_value
    background_repeat_shorthand_property_value {
    scoped_ptr<BackgroundShorthandLayer> shorthand_layer($1);
    scoped_ptr<BackgroundShorthandLayer> non_overlapped($2);
    shorthand_layer->IntegrateNonOverlapped(*non_overlapped);
    $$ = shorthand_layer.release();
  }
  | background_repeat_shorthand_property_value
    background_position_and_size_shorthand_property_value {
    scoped_ptr<BackgroundShorthandLayer> shorthand_layer($1);
    scoped_ptr<BackgroundShorthandLayer> non_overlapped($2);
    shorthand_layer->IntegrateNonOverlapped(*non_overlapped);
    $$ = shorthand_layer.release();
  }
  ;

final_background_layer_without_position_and_repeat:
    /* empty */ {
    // Initialize the background shorthand which is to be filled in by
    // subsequent reductions.
    $$ = new BackgroundShorthandLayer();
  }
  | final_background_layer background_property_element {
    // Propagate the return value from the reduced list.
    // Appending of the new background_property_element to the list is done
    // within background_property_element's reduction.
    $$ = $1;
  }
  ;

// Only the final background layer is allowed to set the background color.
//   http://www.w3.org/TR/css3-background/#ltfinal-bg-layergt
final_background_layer:
    /* empty */ {
    // Initialize the background shorthand which is to be filled in by
    // subsequent reductions.
    $$ = new BackgroundShorthandLayer();
  }
  | final_background_layer_without_position_and_repeat
    background_position_and_repeat_combination {
    scoped_ptr<BackgroundShorthandLayer> shorthand($1);
    scoped_ptr<BackgroundShorthandLayer> background_values($2);
    if (!shorthand->IsBackgroundPropertyOverlapped(*background_values.get())) {
      shorthand->IntegrateNonOverlapped(*background_values.get());
      $$ = shorthand.release();
    } else {
      parser_impl->LogError(
          @1, "background-position or background-repeat declared twice in "
              "background.");
      $$ = NULL;
    }
  }
  | final_background_layer background_property_element {
    // Propagate the return value from the reduced list.
    // Appending of the new background_property_element to the list is done
    // within background_property_element's reduction.
    $$ = $1;
  }
  ;

// Shorthand property for setting most background properties at the same place.
//   http://www.w3.org/TR/css3-background/#the-background
background_property_value:
    final_background_layer
  | common_values {
    // Replicate the common value into each of the properties that background
    // is a shorthand for.
    scoped_ptr<BackgroundShorthandLayer> background(
        new BackgroundShorthandLayer());
    background->background_color = $1;
    background->background_image = $1;
    background->background_repeat = $1;
    background->background_position = $1;
    background->background_size = $1;
    $$ = background.release();
  }
  ;

// Background color of an element drawn behind any background images.
//   http://www.w3.org/TR/css3-background/#the-background-color
background_color_property_value:
    color { $$ = $1; }
  | common_values
  ;

color_stop:
    color {
    $$ = new cssom::ColorStop(MakeScopedRefPtrAndRelease($1));
  }
  | color length_percent_property_value {
    $$ = new cssom::ColorStop(MakeScopedRefPtrAndRelease($1),
                              MakeScopedRefPtrAndRelease($2));
  }
  ;

comma_separated_color_stop_list:
    color_stop {
    $$ = new cssom::LinearGradientValue::ColorStopList();
    $$->push_back($1);
  }
  | comma_separated_color_stop_list comma color_stop {
    $$ = $1;
    $$->push_back($3);
  }
  ;

side:
    kBottomToken maybe_whitespace {
    $$ = cssom::LinearGradientValue::kBottom;
  }
  | kLeftToken maybe_whitespace {
    $$ = cssom::LinearGradientValue::kLeft;
  }
  | kRightToken maybe_whitespace {
    $$ = cssom::LinearGradientValue::kRight;
  }
  | kTopToken maybe_whitespace {
    $$ = cssom::LinearGradientValue::kTop;
  }
  ;

side_or_corner:
    side
  | side side {
    if ($1 == cssom::LinearGradientValue::kBottom) {
      if($2 == cssom::LinearGradientValue::kLeft) {
        $$ = cssom::LinearGradientValue::kBottomLeft;
      } else if ($2 == cssom::LinearGradientValue::kRight) {
        $$ = cssom::LinearGradientValue::kBottomRight;
      } else {
        parser_impl->LogWarning(@1, "Illegal corner value.");
      }
    } else if ($1 == cssom::LinearGradientValue::kLeft) {
      if($2 == cssom::LinearGradientValue::kBottom) {
        $$ = cssom::LinearGradientValue::kBottomLeft;
      } else if ($2 == cssom::LinearGradientValue::kTop) {
        $$ = cssom::LinearGradientValue::kTopLeft;
      } else {
        parser_impl->LogWarning(@1, "Illegal corner value.");
      }
    } else if ($1 == cssom::LinearGradientValue::kRight) {
      if($2 == cssom::LinearGradientValue::kBottom) {
        $$ = cssom::LinearGradientValue::kBottomRight;
      } else if ($2 == cssom::LinearGradientValue::kTop) {
        $$ = cssom::LinearGradientValue::kTopRight;
      } else {
        parser_impl->LogWarning(@1, "Illegal corner value.");
      }
    } else if ($1 == cssom::LinearGradientValue::kTop) {
      if($2 == cssom::LinearGradientValue::kLeft) {
        $$ = cssom::LinearGradientValue::kTopLeft;
      } else if ($2 == cssom::LinearGradientValue::kRight) {
        $$ = cssom::LinearGradientValue::kTopRight;
      } else {
        parser_impl->LogWarning(@1, "Illegal corner value.");
      }
    }
  }
  ;

linear_gradient_params:
    comma_separated_color_stop_list {
    // If the first argument to the linear gradient function is omitted, it
    // defaults to 'to bottom'.
    $$ = AddRef(new cssom::LinearGradientValue(
             cssom::LinearGradientValue::kBottom, $1));
  }
  | angle comma comma_separated_color_stop_list {
    $$ = AddRef(new cssom::LinearGradientValue($1, $3));
  }
  | kToToken kWhitespaceToken maybe_whitespace side_or_corner comma
    comma_separated_color_stop_list {
    $$ = AddRef(new cssom::LinearGradientValue($4, $6));
  }
  ;

background_image_property_list_element:
    url { $$ = $1; }
  | kLinearGradientFunctionToken maybe_whitespace linear_gradient_params ')' {
    $$ = $3;
  }
  | kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  ;

comma_separated_background_image_list:
    background_image_property_list_element {
    $$ = new cssom::PropertyListValue::Builder();
    $$->push_back(MakeScopedRefPtrAndRelease($1));
  }
  | comma_separated_background_image_list comma
    background_image_property_list_element {
    $$ = $1;
    $$->push_back(MakeScopedRefPtrAndRelease($3));
  }
  ;

// Images are drawn with the first specified one on top (closest to the user)
// and each subsequent image behind the previous one.
//   http://www.w3.org/TR/css3-background/#the-background-image
background_image_property_value:
  comma_separated_background_image_list {
    scoped_ptr<cssom::PropertyListValue::Builder> property_value($1);
    $$ = property_value
         ? AddRef(new cssom::PropertyListValue(property_value.Pass()))
         : NULL;
  }
  | common_values
  ;

background_position_property_list_element:
    kBottomToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetBottom().get());;
  }
  | kCenterToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetCenter().get());;
  }
  | kLeftToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetLeft().get());;
  }
  | kRightToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetRight().get());;
  }
  | kTopToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetTop().get());;
  }
  | length_percent_property_value
  ;

background_position_property_list:
    background_position_property_list_element {
    scoped_ptr<BackgroundPositionInfo> position_info(
        new BackgroundPositionInfo());
    position_info->PushBackElement(MakeScopedRefPtrAndRelease($1));
    $$ = position_info.release();
  }
  | background_position_property_list
    background_position_property_list_element {
    scoped_ptr<BackgroundPositionInfo> position_info($1);
    scoped_refptr<cssom::PropertyValue> element = MakeScopedRefPtrAndRelease($2);
    if (position_info &&
        !position_info->PushBackElement(element)) {
      parser_impl->LogWarning(@2, "invalid background position value");
      $$ = NULL;
    } else {
      $$ = position_info.release();
    }
  }
  ;

validated_background_position_property:
    background_position_property_list {
    scoped_ptr<BackgroundPositionInfo> position_info($1);
    if (!position_info) {
      $$ = NULL;
    } else if (position_info->background_position_builder()->size() == 1) {
      // If only one value is specified, the second value is assumed to be
      // 'center'.
      position_info->PushBackElement(cssom::KeywordValue::GetCenter().get());
      $$ = new cssom::PropertyListValue::Builder(
          *position_info->background_position_builder());
    } else if (position_info->background_position_builder()->size() == 2 &&
               !position_info->IsBackgroundPositionValidOnSizeTwo()) {
      parser_impl->LogWarning(@1, "invalid background position value");
      $$ = NULL;
    } else {
      DCHECK_GE(position_info->background_position_builder()->size(),
                static_cast<size_t>(2));
      DCHECK_LE(position_info->background_position_builder()->size(),
                static_cast<size_t>(4));
      $$ = new cssom::PropertyListValue::Builder(
          *position_info->background_position_builder());
    }
  }
  ;

// If background images have been specified, this property specifies their
// initial position (after any resizing) within their corresponding background
// positioning area.
//   http://www.w3.org/TR/css3-background/#the-background-position
background_position_property_value:
    validated_background_position_property {
    scoped_ptr<cssom::PropertyListValue::Builder> property_value($1);
    $$ = property_value
         ? AddRef(new cssom::PropertyListValue(property_value.Pass()))
         : NULL;
  }
  ;

// Specifies how background images are tiled after they have been sized and
// positioned.
//   http://www.w3.org/TR/css3-background/#the-background-repeat
background_repeat_element:
    kNoRepeatToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNoRepeat().get());
  }
  | kRepeatToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetRepeat().get());
  }
  ;

background_repeat_property_value:
    background_repeat_element {
    scoped_ptr<cssom::PropertyListValue::Builder> builder(
        new cssom::PropertyListValue::Builder());
    builder->reserve(2);
    builder->push_back(MakeScopedRefPtrAndRelease($1));
    builder->push_back($1);
    $$ = AddRef(new cssom::PropertyListValue(builder.Pass()));
  }
  | background_repeat_element background_repeat_element {
    scoped_ptr<cssom::PropertyListValue::Builder> builder(
        new cssom::PropertyListValue::Builder());
    builder->reserve(2);
    builder->push_back(MakeScopedRefPtrAndRelease($1));
    builder->push_back(MakeScopedRefPtrAndRelease($2));
    $$ = AddRef(new cssom::PropertyListValue(builder.Pass()));
  }
  | kRepeatXToken maybe_whitespace {
    scoped_ptr<cssom::PropertyListValue::Builder> builder(
        new cssom::PropertyListValue::Builder());
    builder->reserve(2);
    builder->push_back(cssom::KeywordValue::GetRepeat().get());
    builder->push_back(cssom::KeywordValue::GetNoRepeat().get());
    $$ = AddRef(new cssom::PropertyListValue(builder.Pass()));
  }
  | kRepeatYToken maybe_whitespace {
    scoped_ptr<cssom::PropertyListValue::Builder> builder(
        new cssom::PropertyListValue::Builder());
    builder->reserve(2);
    builder->push_back(cssom::KeywordValue::GetNoRepeat().get());
    builder->push_back(cssom::KeywordValue::GetRepeat().get());
    $$ = AddRef(new cssom::PropertyListValue(builder.Pass()));
  }
  ;

background_size_property_list_element:
    length { $$ = $1; }
  | positive_percentage { $$ = $1; }
  | auto
  ;

background_size_property_list:
    background_size_property_list_element {
    $$ = new cssom::PropertyListValue::Builder();
    $$->reserve(2);
    $$->push_back(MakeScopedRefPtrAndRelease($1));
    // Fill in the omitted 'auto'.
    $$->push_back(cssom::KeywordValue::GetAuto().get());
  }
  | background_size_property_list_element
    background_size_property_list_element {
    $$ = new cssom::PropertyListValue::Builder();
    $$->reserve(2);
    $$->push_back(MakeScopedRefPtrAndRelease($1));
    $$->push_back(MakeScopedRefPtrAndRelease($2));
  }
  ;

// Specifies the size of the background images.
//   http://www.w3.org/TR/css3-background/#the-background-size
background_size_property_value:
    background_size_property_list {
    scoped_ptr<cssom::PropertyListValue::Builder> property_value($1);
    $$ = property_value
         ? AddRef(new cssom::PropertyListValue(property_value.Pass()))
         : NULL;
  }
  | kContainToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetContain().get());
  }
  | kCoverToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetCover().get());
  }
  ;

// 'border-color' sets the foreground color of the border specified by the
// border-style properties.
//   http://www.w3.org/TR/css3-background/#border-color
border_color_property_value:
    color  { $$ = $1; }
  | common_values
  ;

//   http://www.w3.org/TR/css3-background/#ltline-stylegt
line_style:
    kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | kHiddenToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetHidden().get());
  }
  | kSolidToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetSolid().get());
  }
  ;

// 'border-style' sets the style of the border, unless there is a border-image.
//   http://www.w3.org/TR/css3-background/#border-style
border_style_property_value:
    line_style
  | common_values
  ;

// 'border-width' sets the thickness of the border.
//   http://www.w3.org/TR/css3-background/#border-width
border_width_property_value:
    positive_length  { $$ = $1; }
  | common_values
  ;

// border_property_element represents a component of a single border property.
// It uses $0 to access its parent's BorderShorthand object and build it, so it
// should always be used to the right of a border shorthand object.
border_property_element:
    color {
    if (!$<border_shorthand>0->border_color) {
      $<border_shorthand>0->border_color = MakeScopedRefPtrAndRelease($1);
    } else {
      parser_impl->LogError(
          @1, "border-color value declared twice in border.");
    }
  }
  | line_style {
    if (!$<border_shorthand>0->border_style) {
      $<border_shorthand>0->border_style = MakeScopedRefPtrAndRelease($1);
    } else {
      parser_impl->LogError(
          @1, "border-style value declared twice in border.");
    }
  }
  | positive_length {
    if (!$<border_shorthand>0->border_width) {
      $<border_shorthand>0->border_width = MakeScopedRefPtrAndRelease($1);
    } else {
      parser_impl->LogError(
          @1, "border-width value declared twice in border.");
    }
  }
  ;

border_property_list:
    /* empty */ {
    $$ = new BorderShorthand();
  }
  | border_property_list border_property_element {
    $$ = $1;
  }
  ;

// TODO(***REMOVED***): Support border-image.
// The border can either be a predefined style (solid line, double line, dotted
// line, pseudo-3D border, etc.) or it can be an image. In the former case,
// various properties define the style ('border-style'), color ('border-color'),
// and thickness ('border-width') of the border.
//   http://www.w3.org/TR/css3-background/#borders
border_property_value:
    border_property_list
  | common_values_without_errors {
    // Replicate the common value into each of the properties that border is a
    // shorthand for.
    scoped_ptr<BorderShorthand> border(new BorderShorthand());
    border->border_color = $1;
    border->border_style = $1;
    border->border_width = $1;
    $$ = border.release();
  }
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
    color { $$ = $1; }
  | common_values
  ;

// Used to add generated content with a :before or :after pseudo-element.
//   http://www.w3.org/TR/CSS21/generate.html#content
content_property_value:
    kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | kNormalToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNormal().get());
  }
  | url { $$ = $1; }
  | kStringToken maybe_whitespace {
    $$ = AddRef(new cssom::StringValue($1.ToString()));
  }
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

// External font-face references consist of a URL, followed by an optional
// hint describing the format of the font resource referenced by that URL.
//   http://www.w3.org/TR/css3-fonts/#descdef-src
font_face_url_src:
    url kFormatFunctionToken maybe_whitespace kStringToken maybe_whitespace ')'
        maybe_whitespace {
    $$ = AddRef(new cssom::UrlSrcValue(MakeScopedRefPtrAndRelease($1),
                                       $4.ToString()));
  }
  | url {
    $$ = AddRef(new cssom::UrlSrcValue(MakeScopedRefPtrAndRelease($1), ""));
  }
  ;

// The syntax for a local font-face reference is a unique font face name
// enclosed by "local(" and ")".
//   http://www.w3.org/TR/css3-fonts/#descdef-src
font_face_local_src:
    kLocalFunctionToken maybe_whitespace font_family_specific_name ')'
        maybe_whitespace {
    $$ = AddRef(new cssom::LocalSrcValue($3->value()));
    $3->Release();
  }
  ;

font_face_src_list_element:
    font_face_url_src
  | font_face_local_src
  ;

comma_separated_font_face_src_list:
    font_face_src_list_element {
    $$ = new cssom::PropertyListValue::Builder();
    $$->push_back(MakeScopedRefPtrAndRelease($1));
  }
  | comma_separated_font_face_src_list comma font_face_src_list_element {
    $$ = $1;
    $$->push_back(MakeScopedRefPtrAndRelease($3));
  }
  ;

// The src descriptor specifies the resource containing font data. Its value is
// a prioritized, comma-separated list of external references or
// locally-installed font face names.
//   http://www.w3.org/TR/css3-fonts/#src-desc
font_face_src_property_value:
    comma_separated_font_face_src_list {
    scoped_ptr<cssom::PropertyListValue::Builder> property_value($1);
    $$ = property_value
         ? AddRef(new cssom::PropertyListValue(property_value.Pass()))
         : NULL;
  }
  | common_values
  ;

// The unquoted name must be a sequence of identifiers separated by whitespace
// which is converted to a string by joining the identifiers together separated
// by a single space.
//   http://www.w3.org/TR/css3-fonts/#descdef-src
font_family_name_identifier_list:
    identifier_token maybe_whitespace {
    $$ = AddRef(new cssom::StringValue($1.ToString()));
  }
  | font_family_name_identifier_list identifier_token maybe_whitespace {
    $$ = AddRef(new cssom::StringValue($1->value() + " " + $2.ToString()));
    $1->Release();
  }
  ;

font_family_string_name:
    kStringToken maybe_whitespace {
    $$ = AddRef(new cssom::StringValue($1.ToString()));
  }
  ;

// Font family names other than generic families must be quoted
// as strings or unquoted as a sequence of one or more identifiers.
//   http://www.w3.org/TR/css3-fonts/#family-name-value
font_family_specific_name:
    font_family_name_identifier_list
  | font_family_string_name
  ;

// Specific font name that does not accept a single identifier. This is needed
// in certain cases to avoid ambiguous states, since single identifier names
// require special rules for keywords.
font_family_specific_name_no_single_identifier:
    font_family_name_identifier_list identifier_token maybe_whitespace {
    $$ = AddRef(new cssom::StringValue($1->value() + " " + $2.ToString()));
    $1->Release();
  }
  | font_family_string_name
  ;

// There are two types of font family names:
//   -- The name of a specific font family
//   -- A generic font family which can be used as a general fallback mechanism
//   http://www.w3.org/TR/css3-fonts/#family-name-value
font_family_name:
    identifier_token maybe_whitespace {
    // Generic families are defined in all CSS implementations.
    //   http://www.w3.org/TR/css3-fonts/#generic-font-families
    if ($1 == cssom::kCursiveKeywordName) {
      $$ = AddRef(cssom::KeywordValue::GetCursive().get());
    } else if ($1 == cssom::kFantasyKeywordName) {
      $$ = AddRef(cssom::KeywordValue::GetFantasy().get());
    } else if ($1 == cssom::kMonospaceKeywordName) {
      $$ = AddRef(cssom::KeywordValue::GetMonospace().get());
    } else if ($1 == cssom::kSansSerifKeywordName) {
      $$ = AddRef(cssom::KeywordValue::GetSansSerif().get());
    } else if ($1 == cssom::kSerifKeywordName) {
      $$ = AddRef(cssom::KeywordValue::GetSerif().get());
    } else {
      $$ = AddRef(new cssom::StringValue($1.ToString()));
    }
  }
  | font_family_specific_name_no_single_identifier { $$ = $1; }
  ;

comma_separated_font_family_name_list:
    font_family_name {
    $$ = new cssom::PropertyListValue::Builder();
    $$->push_back(MakeScopedRefPtrAndRelease($1));
  }
  | comma_separated_font_family_name_list comma font_family_name {
    $$ = $1;
    $$->push_back(MakeScopedRefPtrAndRelease($3));
  }
  ;

// Prioritized list of font family names.
//   http://www.w3.org/TR/css3-fonts/#font-family-prop
font_family_property_value:
  // Special case for a single identifier, which can be a reserved keyword or
  // a generic font name.
    identifier_token maybe_whitespace {
    // "inherit" and "initial" are reserved values and not allowed as font
    // family names.
    //   http://www.w3.org/TR/css3-fonts/#propdef-font-family
    if ($1 == cssom::kInheritKeywordName) {
      $$ = AddRef(cssom::KeywordValue::GetInherit().get());
    } else if ($1 == cssom::kInitialKeywordName) {
      $$ = AddRef(cssom::KeywordValue::GetInitial().get());
    } else {
      scoped_ptr<cssom::PropertyListValue::Builder>
          builder(new cssom::PropertyListValue::Builder());

      // Generic families are defined in all CSS implementations.
      //   http://www.w3.org/TR/css3-fonts/#generic-font-families
      if ($1 == cssom::kCursiveKeywordName) {
        builder->push_back(cssom::KeywordValue::GetCursive().get());
      } else if ($1 == cssom::kFantasyKeywordName) {
        builder->push_back(cssom::KeywordValue::GetFantasy().get());
      } else if ($1 == cssom::kMonospaceKeywordName) {
        builder->push_back(cssom::KeywordValue::GetMonospace().get());
      } else if ($1 == cssom::kSansSerifKeywordName) {
        builder->push_back(cssom::KeywordValue::GetSansSerif().get());
      } else if ($1 == cssom::kSerifKeywordName) {
        builder->push_back(cssom::KeywordValue::GetSerif().get());
      } else {
        builder->push_back(new cssom::StringValue($1.ToString()));
      }

      $$ = AddRef(new cssom::PropertyListValue(builder.Pass()));
    }
  }
  | font_family_specific_name_no_single_identifier {
    scoped_ptr<cssom::PropertyListValue::Builder>
        builder(new cssom::PropertyListValue::Builder());
    builder->push_back(MakeScopedRefPtrAndRelease($1));
    $$ = AddRef(new cssom::PropertyListValue(builder.Pass()));
  }
  | comma_separated_font_family_name_list comma font_family_name {
    scoped_ptr<cssom::PropertyListValue::Builder> builder($1);
    builder->push_back(MakeScopedRefPtrAndRelease($3));
    $$ = AddRef(new cssom::PropertyListValue(builder.Pass()));
  }
  | errors {
    parser_impl->LogError(@1, "unsupported property value for font-family");
    $$ = NULL;
  }
  ;

// Desired height of glyphs from the font.
//   http://www.w3.org/TR/css3-fonts/#font-size-prop
font_size_property_value:
    positive_length_percent_property_value
  | common_values
  ;

// The 'font-style' property allows italic or oblique faces to be selected.
//   http://www.w3.org/TR/css3-fonts/#font-style-prop
font_style_property_value:
    kItalicToken maybe_whitespace {
    $$ = AddRef(cssom::FontStyleValue::GetItalic().get());
  }
  | kNormalToken maybe_whitespace {
    $$ = AddRef(cssom::FontStyleValue::GetNormal().get());
  }
  | kObliqueToken maybe_whitespace {
    $$ = AddRef(cssom::FontStyleValue::GetOblique().get());
  }
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
  | positive_integer {
    switch ($1) {
      case 100:
        $$ = AddRef(cssom::FontWeightValue::GetThinAka100().get());
        break;
      case 200:
        $$ = AddRef(cssom::FontWeightValue::GetExtraLightAka200().get());
        break;
      case 300:
        $$ = AddRef(cssom::FontWeightValue::GetLightAka300().get());
        break;
      case 400:
        $$ = AddRef(cssom::FontWeightValue::GetNormalAka400().get());
        break;
      case 500:
        $$ = AddRef(cssom::FontWeightValue::GetMediumAka500().get());
        break;
      case 600:
        $$ = AddRef(cssom::FontWeightValue::GetSemiBoldAka600().get());
        break;
      case 700:
        $$ = AddRef(cssom::FontWeightValue::GetBoldAka700().get());
        break;
      case 800:
        $$ = AddRef(cssom::FontWeightValue::GetExtraBoldAka800().get());
        break;
      case 900:
        $$ = AddRef(cssom::FontWeightValue::GetBlackAka900().get());
        break;
      default:
        parser_impl->LogError(@1, "unsupported property value for font-weight");
        $$ = NULL;
    }
  }
  | common_values
  ;

// Specifies the content height of boxes.
//   http://www.w3.org/TR/CSS21/visudet.html#the-height-property
//   http://www.w3.org/TR/CSS2/visudet.html#propdef-min-height
height_property_value:
    positive_length_percent_property_value
  | auto
  | common_values
  ;

// 'max-height' value can be height_property_value or 'none'.
//   http://www.w3.org/TR/CSS2/visudet.html#propdef-max-height
max_height_property_value:
    kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | height_property_value
  ;


// Specifies the minimal height of line boxes within the element.
//   http://www.w3.org/TR/CSS21/visudet.html#line-height
line_height_property_value:
    kNormalToken maybe_whitespace  {
    $$ = AddRef(cssom::KeywordValue::GetNormal().get());
  }
  | positive_length {  $$ = $1; }
  | common_values
  ;

// <margin-width> value type.
//   http://www.w3.org/TR/CSS21/box.html#value-def-margin-width
margin_width:
    length_percent_property_value
  | auto
  ;

// Specifies the width of a top, right, bottom, or left side of the margin area
// of a box.
//   http://www.w3.org/TR/CSS21/box.html#margin-properties
margin_side_property_value:
    margin_width
  | common_values
  ;

// The "margin" property is a shorthand property for setting "margin-top",
// "margin-right", "margin-bottom", and "margin-left" at the same place.
//   http://www.w3.org/TR/CSS21/box.html#margin-properties
margin_property_value:
    // If there is only one component value, it applies to all sides.
    margin_width {
    scoped_refptr<cssom::PropertyValue> width = MakeScopedRefPtrAndRelease($1);
    $$ = MarginOrPaddingShorthand::TryCreate(
        width, width, width, width).release();
  }
    // If there are two values, the top and bottom margins are set to the first
    // value and the right and left margins are set to the second.
  | margin_width margin_width {
    scoped_refptr<cssom::PropertyValue> vertical_width =
        MakeScopedRefPtrAndRelease($1);
    scoped_refptr<cssom::PropertyValue> horizontal_width =
        MakeScopedRefPtrAndRelease($2);
    $$ = MarginOrPaddingShorthand::TryCreate(
             vertical_width, horizontal_width,
             vertical_width, horizontal_width).release();
  }
    // If there are three values, the top is set to the first value, the left
    // and right are set to the second, and the bottom is set to the third.
  | margin_width margin_width margin_width {
    scoped_refptr<cssom::PropertyValue> top_width =
        MakeScopedRefPtrAndRelease($1);
    scoped_refptr<cssom::PropertyValue> horizontal_width =
        MakeScopedRefPtrAndRelease($2);
    scoped_refptr<cssom::PropertyValue> bottom_width =
        MakeScopedRefPtrAndRelease($3);
    $$ = MarginOrPaddingShorthand::TryCreate(
             top_width, horizontal_width,
             bottom_width, horizontal_width).release();
  }
    // If there are four values, they apply to the top, right, bottom, and left,
    // respectively.
  | margin_width margin_width margin_width margin_width {
    scoped_refptr<cssom::PropertyValue> top_width =
        MakeScopedRefPtrAndRelease($1);
    scoped_refptr<cssom::PropertyValue> left_width =
        MakeScopedRefPtrAndRelease($2);
    scoped_refptr<cssom::PropertyValue> bottom_width =
        MakeScopedRefPtrAndRelease($3);
    scoped_refptr<cssom::PropertyValue> right_width =
        MakeScopedRefPtrAndRelease($4);
    $$ = MarginOrPaddingShorthand::TryCreate(
             top_width, left_width, bottom_width, right_width).release();
  }
  | common_values {
    $$ = MarginOrPaddingShorthand::TryCreate($1, $1, $1, $1).release();
  }
  ;

// Specifies top, right, bottom, or left offset of the box relatively
// to the container block.
//   http://www.w3.org/TR/CSS21/visuren.html#position-props
offset_property_value:
    length_percent_property_value
  | auto
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

// Specifies whether the user agent may arbitrarily break within a word to
// prevent overflow when an otherwise unbreakable string is too long to
// fit within the line box.
//   http://www.w3.org/TR/css-text-3/#overflow-wrap-property
overflow_wrap_property_value:
    kBreakWordToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetBreakWord().get());
  }
  | kNormalToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNormal().get());
  }
  | common_values
  ;

// <padding-width> value type is the same as
// positive_length_percent_property_value.
//   http://www.w3.org/TR/CSS21/box.html#value-def-padding-width
// Specifies the width of a top, right, bottom, or left side of the padding area
// of a box.
//   http://www.w3.org/TR/CSS21/box.html#padding-properties
padding_side_property_value:
    positive_length_percent_property_value
  | common_values
  ;

// The "padding" property is a shorthand property for setting "padding-top",
// "padding-right", "padding-bottom", and "padding-left" at the same place.
//   http://www.w3.org/TR/CSS21/box.html#padding-properties
padding_property_value:
    // If there is only one component value, it applies to all sides.
    positive_length_percent_property_value {
    scoped_refptr<cssom::PropertyValue> width =
        MakeScopedRefPtrAndRelease($1);
    $$ = MarginOrPaddingShorthand::TryCreate(
             width, width, width, width).release();
  }
    // If there are two values, the top and bottom paddings are set to the first
    // value and the right and left paddings are set to the second.
  | positive_length_percent_property_value
    positive_length_percent_property_value {
    scoped_refptr<cssom::PropertyValue> vertical_width =
        MakeScopedRefPtrAndRelease($1);
    scoped_refptr<cssom::PropertyValue> horizontal_width =
        MakeScopedRefPtrAndRelease($2);
    $$ = MarginOrPaddingShorthand::TryCreate(
             vertical_width, horizontal_width,
             vertical_width, horizontal_width).release();
  }
    // If there are three values, the top is set to the first value, the left
    // and right are set to the second, and the bottom is set to the third.
  | positive_length_percent_property_value
    positive_length_percent_property_value
    positive_length_percent_property_value {
    scoped_refptr<cssom::PropertyValue> top_width =
        MakeScopedRefPtrAndRelease($1);
    scoped_refptr<cssom::PropertyValue> horizontal_width =
        MakeScopedRefPtrAndRelease($2);
    scoped_refptr<cssom::PropertyValue> bottom_width =
        MakeScopedRefPtrAndRelease($3);
    $$ = MarginOrPaddingShorthand::TryCreate(
             top_width, horizontal_width,
             bottom_width, horizontal_width).release();
  }
    // If there are four values, they apply to the top, right, bottom, and left,
    // respectively.
  | positive_length_percent_property_value
    positive_length_percent_property_value
    positive_length_percent_property_value
    positive_length_percent_property_value {
    scoped_refptr<cssom::PropertyValue> top_width =
        MakeScopedRefPtrAndRelease($1);
    scoped_refptr<cssom::PropertyValue> left_width =
        MakeScopedRefPtrAndRelease($2);
    scoped_refptr<cssom::PropertyValue> bottom_width =
        MakeScopedRefPtrAndRelease($3);
    scoped_refptr<cssom::PropertyValue> right_width =
        MakeScopedRefPtrAndRelease($4);
    $$ = MarginOrPaddingShorthand::TryCreate(
             top_width, left_width, bottom_width, right_width).release();
  }
  | common_values {
    $$ = MarginOrPaddingShorthand::TryCreate($1, $1, $1, $1).release();
  }
  ;

// Determines which of the positioning algorithms is used to calculate
// the position of a box.
//   http://www.w3.org/TR/CSS21/visuren.html#choose-position
position_property_value:
    kAbsoluteToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetAbsolute().get());
  }
  | kFixedToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetFixed().get());
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

// This property determines the tab size used to render preserved tab
// characters. Integers represent the measure as multiples of the space
// character's advance width. Negative values are not allowed.
//   http://www.w3.org/TR/css-text-3/#tab-size
tab_size_property_value:
    positive_integer {
    $$ = AddRef(new cssom::IntegerValue($1));
  }
  | common_values
  ;

// This property describes how inline-level content of a block container is
// aligned.
//   http://www.w3.org/TR/CSS21/text.html#propdef-text-align
text_align_property_value:
    kLeftToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetLeft().get());
  }
  | kCenterToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetCenter().get());
  }
  | kRightToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetRight().get());
  }
  | common_values
  ;

// This property specifies the indentation applied to lines of inline content in
// a block.
//   http://www.w3.org/TR/css-text-3/#text-indent
text_indent_property_value:
    length {
    $$ = $1;
  }
  | common_values
  ;

// This property specifies rendering when inline content overflows its line box
// edge in the inline progression direction of its block container element.
//   http://www.w3.org/TR/css3-ui/#propdef-text-overflow
text_overflow_property_value:
    kClipToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetClip().get());
  }
  | kEllipsisToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetEllipsis().get());
  }
  | common_values
  ;

// This property controls capitalization effects of an element's text.
//   http://www.w3.org/TR/css3-text/#text-transform-property
text_transform_property_value:
    kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | kUppercaseToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetUppercase().get());
  }
  | common_values
  ;

// The set of allowed transform functions.
//   http://www.w3.org/TR/css3-transforms/#transform-functions
transform_function:
  // Specifies an arbitrary affine 2D transformation.
    kMatrixFunctionToken maybe_whitespace number comma number comma number
      comma number comma number comma number ')' maybe_whitespace {
    $<transform_functions>0->push_back(
        new cssom::MatrixFunction($3, $5, $7, $9, $11, $13));
  }
  // Specifies a 2D rotation around the z-axis.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-rotate
  | kRotateFunctionToken maybe_whitespace angle ')'
      maybe_whitespace {
    $<transform_functions>0->push_back(new cssom::RotateFunction($3));
  }
  // Specifies a 2D scale operation by the scaling vector.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-scale
  | kScaleFunctionToken maybe_whitespace scale_function_parameters ')'
      maybe_whitespace {
    $<transform_functions>0->push_back($3);
  }
  // Specifies a 2D scale operation using the [sx, 1] scaling vector, where sx
  // is given as the parameter.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-scalex
  | kScaleXFunctionToken maybe_whitespace number ')'
      maybe_whitespace {
    $<transform_functions>0->push_back(new cssom::ScaleFunction($3, 1.0f));
  }
  // Specifies a 2D scale operation using the [1, sy] scaling vector, where sy
  // is given as the parameter.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-scaley
  | kScaleYFunctionToken maybe_whitespace number ')'
      maybe_whitespace {
    $<transform_functions>0->push_back(new cssom::ScaleFunction(1.0f, $3));
  }
  // Specifies a 2D translation by the vector [tx, ty], where tx is the first
  // translation-value parameter and ty is the optional second translation-value
  // parameter. If <ty> is not provided, ty has zero as a value.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-translate
  | kTranslateFunctionToken maybe_whitespace length_percent_property_value ')'
      maybe_whitespace {
    $<transform_functions>0->push_back(new cssom::TranslateFunction(
        cssom::TranslateFunction::kXAxis, MakeScopedRefPtrAndRelease($3)));
    $<transform_functions>0->push_back(new cssom::TranslateFunction(
        cssom::TranslateFunction::kYAxis,
        scoped_refptr<cssom::LengthValue>(
            new cssom::LengthValue(0, cssom::kPixelsUnit))));
  }
  | kTranslateFunctionToken maybe_whitespace length_percent_property_value comma
    length_percent_property_value ')' maybe_whitespace {
    $<transform_functions>0->push_back(new cssom::TranslateFunction(
        cssom::TranslateFunction::kXAxis, MakeScopedRefPtrAndRelease($3)));
    $<transform_functions>0->push_back(new cssom::TranslateFunction(
        cssom::TranslateFunction::kYAxis, MakeScopedRefPtrAndRelease($5)));
  }
  // Specifies a translation by the given amount in the X direction.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-translatex
  | kTranslateXFunctionToken maybe_whitespace length_percent_property_value ')'
      maybe_whitespace {
    $<transform_functions>0->push_back(new cssom::TranslateFunction(
        cssom::TranslateFunction::kXAxis, MakeScopedRefPtrAndRelease($3)));
  }
  // Specifies a translation by the given amount in the Y direction.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-translatey
  | kTranslateYFunctionToken maybe_whitespace length_percent_property_value ')'
      maybe_whitespace {
    $<transform_functions>0->push_back(new cssom::TranslateFunction(
        cssom::TranslateFunction::kYAxis, MakeScopedRefPtrAndRelease($3)));
  }
  // Specifies a 3D translation by the vector [0,0,z] with the given amount
  // in the Z direction.
  //   http://www.w3.org/TR/css3-transforms/#funcdef-translatez
  | kTranslateZFunctionToken maybe_whitespace length ')'
      maybe_whitespace {
    $<transform_functions>0->push_back(new cssom::TranslateFunction(
        cssom::TranslateFunction::kZAxis, MakeScopedRefPtrAndRelease($3)));
  }
  ;

// One or more transform functions separated by whitespace.
//   http://www.w3.org/TR/css3-transforms/#typedef-transform-list
transform_list:
    /* empty */ {
    $$ = new cssom::TransformFunctionListValue::Builder();
  }
  | transform_list transform_function {
    $$ = $1;
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
    $$ = AddRef(new cssom::TransformFunctionListValue(
             transform_functions->Pass()));
  }
  | transform_list errors {
    scoped_ptr<cssom::TransformFunctionListValue::Builder>
        transform_functions($1);
    parser_impl->LogWarning(@2, "invalid transform function");
    $$ = NULL;
  }
  | common_values_without_errors
  ;

// Determines the vertical alignment of a box.
//   http://www.w3.org/TR/CSS21/visudet.html#propdef-vertical-align
vertical_align_property_value:
    kBottomToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetBottom().get());
  }
  | kTopToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetTop().get());
  }
  | kMiddleToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetMiddle().get());
  }
  | kBaselineToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetBaseline().get());
  }
  | common_values
  ;

// The 'visibility' property specifies whether the boxes generated by an element
// are rendered.
//   http://www.w3.org/TR/CSS21/visufx.html#propdef-visibility
visibility_property_value:
    kHiddenToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetHidden().get());
  }
  | kVisibleToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetVisible().get());
  }
  | common_values
  ;

// One ore more time values separated by commas.
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

time_list_property_value:
    comma_separated_time_list {
    scoped_ptr<cssom::ListValue<base::TimeDelta>::Builder> time_list($1);
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

single_timing_function:
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

comma_separated_single_timing_function_list:
    single_timing_function {
    $$ = new cssom::TimingFunctionListValue::Builder();
    $$->push_back(MakeScopedRefPtrAndRelease($1));
  }
  | comma_separated_single_timing_function_list comma
    single_timing_function {
    $$ = $1;
    $$->push_back(MakeScopedRefPtrAndRelease($3));
  }
  ;

timing_function_list_property_value:
    comma_separated_single_timing_function_list {
    scoped_ptr<cssom::TimingFunctionListValue::Builder>
        timing_function_list($1);
    $$ = timing_function_list
         ? AddRef(new cssom::TimingFunctionListValue(
               timing_function_list.Pass()))
         : NULL;
  }
  | common_values
  ;

// The 'animation-delay' property defines when the animation will start.
//  http://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-delay-property
animation_delay_property_value: time_list_property_value;

// The 'animation-direction' property defines which direction time should flow
// given the current iteration number.
//  http://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-direction-property
animation_direction_list_element:
    kNormalToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNormal().get());
  }
  | kReverseToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetReverse().get());
  }
  | kAlternateToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetAlternate().get());
  }
  | kAlternateReverseToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetAlternateReverse().get());
  }
  ;

comma_separated_animation_direction_list:
    animation_direction_list_element {
    $$ = new cssom::PropertyListValue::Builder();
    $$->push_back(MakeScopedRefPtrAndRelease($1));
  }
  | comma_separated_animation_direction_list comma
        animation_direction_list_element {
    $$ = $1;
    $$->push_back(MakeScopedRefPtrAndRelease($3));
  }
  | errors {
    parser_impl->LogError(
        @1, "unsupported property value for animation-direction");
    $$ = NULL;
  }
  ;

animation_direction_property_value:
    comma_separated_animation_direction_list {
    scoped_ptr<cssom::PropertyListValue::Builder> direction_list($1);
    $$ = direction_list
         ? AddRef(new cssom::PropertyListValue(direction_list.Pass()))
         : NULL;
  }
  | common_values_without_errors
  ;

// The 'animation-duration' property defines the length of time that an
// animation takes to complete one cycle.
//  http://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-duration-property
animation_duration_property_value: time_list_property_value;

// The 'animation-fill-mode' property defines what values are applied by the
// animation outside the time it is executing.
//  http://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-fill-mode-property
animation_fill_mode_list_element:
    kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | kForwardsToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetForwards().get());
  }
  | kBackwardsToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetBackwards().get());
  }
  | kBothToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetBoth().get());
  }
  ;

comma_separated_animation_fill_mode_list:
    animation_fill_mode_list_element {
    $$ = new cssom::PropertyListValue::Builder();
    $$->push_back(MakeScopedRefPtrAndRelease($1));
  }
  | comma_separated_animation_fill_mode_list comma
        animation_fill_mode_list_element {
    $$ = $1;
    $$->push_back(MakeScopedRefPtrAndRelease($3));
  }
  | errors {
    parser_impl->LogError(
        @1, "unsupported property value for animation-fill-mode");
    $$ = NULL;
  }
  ;

animation_fill_mode_property_value:
    comma_separated_animation_fill_mode_list {
    scoped_ptr<cssom::PropertyListValue::Builder> fill_mode_list($1);
    $$ = fill_mode_list
         ? AddRef(new cssom::PropertyListValue(fill_mode_list.Pass()))
         : NULL;
  }
  | common_values_without_errors
  ;

// The 'animation-iteration-count' property specifies the number of times an
// animation cycle is played.
//  http://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-iteration-count-property
animation_iteration_count_list_element:
    kInfiniteToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetInfinite().get());
  }
  | non_negative_number {
    $$ = AddRef(new cssom::NumberValue($1));
  }
  ;

comma_separated_animation_iteration_count_list:
    animation_iteration_count_list_element {
    $$ = new cssom::PropertyListValue::Builder();
    $$->push_back(MakeScopedRefPtrAndRelease($1));
  }
  | comma_separated_animation_iteration_count_list comma
        animation_iteration_count_list_element {
    $$ = $1;
    $$->push_back(MakeScopedRefPtrAndRelease($3));
  }
  | errors {
    parser_impl->LogError(
        @1, "unsupported property value for animation-iteration-count");
    $$ = NULL;
  }
  ;

animation_iteration_count_property_value:
    comma_separated_animation_iteration_count_list {
    scoped_ptr<cssom::PropertyListValue::Builder> iteration_count_list($1);
    $$ = iteration_count_list
         ? AddRef(new cssom::PropertyListValue(iteration_count_list.Pass()))
         : NULL;
  }
  | common_values_without_errors
  ;

// The 'animation-name' property defines a list of animations that apply.
//  http://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-name-property
animation_name_list_element:
    kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | kIdentifierToken maybe_whitespace {
    $$ = AddRef(new cssom::StringValue($1.ToString()));
  }
  ;

comma_separated_animation_name_list:
    animation_name_list_element {
    $$ = new cssom::PropertyListValue::Builder();
    $$->push_back(MakeScopedRefPtrAndRelease($1));
  }
  | comma_separated_animation_name_list comma
        animation_name_list_element {
    $$ = $1;
    $$->push_back(MakeScopedRefPtrAndRelease($3));
  }
  | errors {
    parser_impl->LogError(
        @1, "unsupported property value for animation-name");
    $$ = NULL;
  }
  ;

animation_name_property_value:
    comma_separated_animation_name_list {
    scoped_ptr<cssom::PropertyListValue::Builder> name_list($1);
    $$ = name_list
         ? AddRef(new cssom::PropertyListValue(name_list.Pass()))
         : NULL;
  }
  | common_values_without_errors
  ;

// The 'animation-timing-function' property describes how the animation will
// progress over one cycle of its duration.
//  http://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-timing-function-property
animation_timing_function_property_value: timing_function_list_property_value;

// single_animation_element represents a component of a single animation.
// It uses $0 to access its parent's SingleAnimationShorthand object and build
// it, so it should always be used to the right of a single_animation object.
single_animation_element:
    kIdentifierToken maybe_whitespace {
    if (!$<single_animation>0->name) {
      $<single_animation>0->name = new cssom::StringValue($1.ToString());
    } else {
      parser_impl->LogWarning(
          @1, "animation-name value declared twice in animation.");
    }
  }
  | animation_direction_list_element {
    if (!$<single_animation>0->direction) {
      $<single_animation>0->direction = MakeScopedRefPtrAndRelease($1);
    } else {
      parser_impl->LogWarning(
          @1, "animation-direction value declared twice in animation.");
    }
  }
  | animation_fill_mode_list_element {
    if (!$<single_animation>0->fill_mode) {
      $<single_animation>0->fill_mode = MakeScopedRefPtrAndRelease($1);
    } else {
      parser_impl->LogWarning(
          @1, "animation-fill-mode value declared twice in animation.");
    }
  }
  | animation_iteration_count_list_element {
    if (!$<single_animation>0->iteration_count) {
      $<single_animation>0->iteration_count = MakeScopedRefPtrAndRelease($1);
    } else {
      parser_impl->LogWarning(
          @1, "animation-iteration-count value declared twice in animation.");
    }
  }
  | time_with_units_required {
    if (!$<single_animation>0->duration) {
      // The first time encountered sets the duration.
      $<single_animation>0->duration = base::TimeDelta::FromInternalValue($1);
    } else if (!$<single_animation>0->delay) {
      // The second time encountered sets the delay.
      $<single_animation>0->delay = base::TimeDelta::FromInternalValue($1);
    } else {
      parser_impl->LogWarning(
          @1, "time value declared too many times in animation.");
    }
  }
  | single_timing_function maybe_whitespace {
    if (!$<single_animation>0->timing_function) {
      $<single_animation>0->timing_function = MakeScopedRefPtrAndRelease($1);
    } else {
      parser_impl->LogWarning(
          @1, "animation-timing-function value declared twice in animation.");
    }
  }
  | error maybe_whitespace {
    parser_impl->LogError(@1, "unsupported property value for animation");
    $<single_animation>0->error = true;
  }
  ;

single_animation:
    /* empty */ {
    // Initialize the result, to be filled in by single_animation_element
    $$ = new SingleAnimationShorthand();
  }
  | single_non_empty_animation
  ;

single_non_empty_animation:
    single_animation single_animation_element {
    // Propagate the list from our parent single_animation.
    // single_animation_element will have already taken care of adding itself
    // to the list via $0.
    $$ = $1;
  }
  ;

comma_separated_animation_list:
    single_non_empty_animation {
    scoped_ptr<SingleAnimationShorthand> single_animation($1);
    scoped_ptr<AnimationShorthandBuilder> animation_builder(
        new AnimationShorthandBuilder());

    if (!single_animation->error) {
      single_animation->ReplaceNullWithInitialValues();

      animation_builder->delay_list_builder->push_back(
          *single_animation->delay);
      animation_builder->direction_list_builder->push_back(
          single_animation->direction);
      animation_builder->duration_list_builder->push_back(
          *single_animation->duration);
      animation_builder->fill_mode_list_builder->push_back(
          single_animation->fill_mode);
      animation_builder->iteration_count_list_builder->push_back(
          single_animation->iteration_count);
      animation_builder->name_list_builder->push_back(
          single_animation->name);
      animation_builder->timing_function_list_builder->push_back(
          single_animation->timing_function);
    }

    $$ = animation_builder.release();
  }
  | comma_separated_animation_list comma single_non_empty_animation {
    scoped_ptr<SingleAnimationShorthand> single_animation($3);
    $$ = $1;

    if (!single_animation->error) {
      single_animation->ReplaceNullWithInitialValues();

      $$->delay_list_builder->push_back(*single_animation->delay);
      $$->direction_list_builder->push_back(single_animation->direction);
      $$->duration_list_builder->push_back(*single_animation->duration);
      $$->fill_mode_list_builder->push_back(single_animation->fill_mode);
      $$->iteration_count_list_builder->push_back(
          single_animation->iteration_count);
      $$->name_list_builder->push_back(single_animation->name);
      $$->timing_function_list_builder->push_back(
          single_animation->timing_function);
    }
  }
  ;

// Animation shorthand property.
//   http://www.w3.org/TR/2013/WD-css3-animations-20130219/#animation-shorthand-property
animation_property_value:
    comma_separated_animation_list {
    scoped_ptr<AnimationShorthandBuilder> animation_builder($1);

    scoped_ptr<AnimationShorthand> animation(new AnimationShorthand());

    animation->delay_list = new cssom::TimeListValue(
        animation_builder->delay_list_builder.Pass());
    animation->direction_list = new cssom::PropertyListValue(
        animation_builder->direction_list_builder.Pass());
    animation->duration_list = new cssom::TimeListValue(
        animation_builder->duration_list_builder.Pass());
    animation->fill_mode_list = new cssom::PropertyListValue(
        animation_builder->fill_mode_list_builder.Pass());
    animation->iteration_count_list = new cssom::PropertyListValue(
        animation_builder->iteration_count_list_builder.Pass());
    animation->name_list = new cssom::PropertyListValue(
        animation_builder->name_list_builder.Pass());
    animation->timing_function_list = new cssom::TimingFunctionListValue(
        animation_builder->timing_function_list_builder.Pass());

    $$ = animation.release();
  }
  | common_values_without_errors {
    // Replicate the common value into each of the properties that animation
    // is a shorthand for.
    scoped_ptr<AnimationShorthand> animation(new AnimationShorthand());
    animation->delay_list = $1;
    animation->direction_list = $1;
    animation->duration_list = $1;
    animation->fill_mode_list = $1;
    animation->iteration_count_list = $1;
    animation->name_list = $1;
    animation->timing_function_list = $1;
    $$ = animation.release();
  }
  ;

// Parse a list of time values for transition-delay.
//   http://www.w3.org/TR/css3-transitions/#transition-delay
transition_delay_property_value: time_list_property_value;

// Parse a list of time values for transition-duration.
//   http://www.w3.org/TR/css3-transitions/#transition-duration
transition_duration_property_value: time_list_property_value;

// Parse a list of timing function values for transition-timing-function.
//   http://www.w3.org/TR/css3-transitions/#transition-timing-function-property
transition_timing_function_property_value: timing_function_list_property_value;

// One or more property names separated by commas.
//   http://www.w3.org/TR/css3-transitions/#transition-property
comma_separated_animatable_property_name_list:
    animatable_property_token maybe_whitespace {
    $$ = new cssom::PropertyKeyListValue::Builder();
    $$->push_back($1);
  }
  | comma_separated_animatable_property_name_list comma
    animatable_property_token maybe_whitespace {
    $$ = $1;
    $$->push_back($3);
  }
  | errors {
    parser_impl->LogError(@1, "unsupported property value for animation");
    $$ = NULL;
  }
  ;

// Parse a list of references to property names.
//   http://www.w3.org/TR/css3-transitions/#transition-property
transition_property_property_value:
    kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | comma_separated_animatable_property_name_list {
    scoped_ptr<cssom::PropertyKeyListValue::Builder> property_name_list($1);
    $$ = property_name_list
         ? AddRef(new cssom::PropertyKeyListValue(property_name_list.Pass()))
         : NULL;
  }
  | common_values_without_errors
  ;

// single_transition_element represents a component of a single transition.
// It uses $0 to access its parent's SingleTransitionShorthand object and build
// it, so it should always be used to the right of a single_transition object.
single_transition_element:
    animatable_property_token maybe_whitespace {
    if (!$<single_transition>0->property) {
      $<single_transition>0->property = $1;
    } else {
      parser_impl->LogWarning(
          @1, "transition-property value declared twice in transition.");
    }
  }
  | kNoneToken maybe_whitespace {
    if (!$<single_transition>0->property) {
      // We use cssom::kNoneProperty as a symbol that 'none' was specified
      // here and that the entire transition-property list value should be set
      // to the None keyword.
      $<single_transition>0->property = cssom::kNoneProperty;
    } else {
      parser_impl->LogWarning(
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
      parser_impl->LogWarning(
          @1, "time value declared twice in transition.");
    }
  }
  | single_timing_function maybe_whitespace {
    if (!$<single_transition>0->timing_function) {
      $<single_transition>0->timing_function = MakeScopedRefPtrAndRelease($1);
    } else {
      parser_impl->LogWarning(
          @1, "transition-timing-function value declared twice in transition.");
    }
  }
  | error maybe_whitespace {
    parser_impl->LogError(@1, "unsupported property value for animation");
    $<single_transition>0->error = true;
  }
  ;

single_transition:
    /* empty */ {
    // Initialize the result, to be filled in by single_transition_element
    $$ = new SingleTransitionShorthand();
  }
  | single_non_empty_transition
  ;

single_non_empty_transition:
    single_transition single_transition_element {
    // Propagate the list from our parent single_transition.
    // single_transition_element will have already taken care of adding itself
    // to the list via $0.
    $$ = $1;
  }
  ;

comma_separated_transition_list:
    single_non_empty_transition {
    scoped_ptr<SingleTransitionShorthand> single_transition($1);
    scoped_ptr<TransitionShorthandBuilder> transition_builder(
        new TransitionShorthandBuilder());

    if (!single_transition->error) {
      single_transition->ReplaceNullWithInitialValues();

      transition_builder->property_list_builder->push_back(
          *single_transition->property);
      transition_builder->duration_list_builder->push_back(
          *single_transition->duration);
      transition_builder->timing_function_list_builder->push_back(
          single_transition->timing_function);
      transition_builder->delay_list_builder->push_back(
          *single_transition->delay);
    }

    $$ = transition_builder.release();
  }
  | comma_separated_transition_list comma single_non_empty_transition {
    scoped_ptr<SingleTransitionShorthand> single_transition($3);
    $$ = $1;

    if (!single_transition->error) {
      single_transition->ReplaceNullWithInitialValues();

      $$->property_list_builder->push_back(*single_transition->property);
      $$->duration_list_builder->push_back(*single_transition->duration);
      $$->timing_function_list_builder->push_back(
          single_transition->timing_function);
      $$->delay_list_builder->push_back(*single_transition->delay);
    }
  }
  ;

// Transition shorthand property.
//   http://www.w3.org/TR/css3-transitions/#transition
transition_property_value:
    comma_separated_transition_list {
    scoped_ptr<TransitionShorthandBuilder> transition_builder($1);

    // Before proceeding, check that 'none' is not specified if the
    // number of transition statements is larger than 1, as per the
    // specification.
    const cssom::PropertyKeyListValue::Builder& property_list_builder =
        *(transition_builder->property_list_builder);
    if (property_list_builder.size() > 1) {
      for (int i = 0; i < static_cast<int>(property_list_builder.size()); ++i) {
        if (property_list_builder[i] == cssom::kNoneProperty) {
          parser_impl->LogWarning(
              @1, "If 'none' is specified, transition can only have one item.");
          break;
        }
      }
    }

    scoped_ptr<TransitionShorthand> transition(new TransitionShorthand());

    if (property_list_builder.size() == 1 &&
        property_list_builder[0] == cssom::kNoneProperty) {
      transition->property_list = cssom::KeywordValue::GetNone();
    } else {
      transition->property_list = new cssom::PropertyKeyListValue(
          transition_builder->property_list_builder.Pass());
    }
    transition->duration_list = new cssom::TimeListValue(
        transition_builder->duration_list_builder.Pass());
    transition->timing_function_list = new cssom::TimingFunctionListValue(
        transition_builder->timing_function_list_builder.Pass());
    transition->delay_list = new cssom::TimeListValue(
        transition_builder->delay_list_builder.Pass());

    $$ = transition.release();
  }
  | common_values_without_errors {
    // Replicate the common value into each of the properties that transition
    // is a shorthand for.
    scoped_ptr<TransitionShorthand> transition(new TransitionShorthand());
    transition->property_list = $1;
    transition->duration_list = $1;
    transition->timing_function_list = $1;
    transition->delay_list = $1;
    $$ = transition.release();
  }
  ;

unicode_range_property_value:
    comma_separated_unicode_range_list {
    scoped_ptr<cssom::PropertyListValue::Builder> property_value($1);
    $$ = property_value
         ? AddRef(new cssom::PropertyListValue(property_value.Pass()))
         : NULL;
  }
  | common_values
  ;

comma_separated_unicode_range_list:
    kUnicodeRangeToken maybe_whitespace {
    $$ = new cssom::PropertyListValue::Builder();
    $$->push_back(new cssom::UnicodeRangeValue($1.first, $1.second));
  }
  | comma_separated_unicode_range_list comma kUnicodeRangeToken
        maybe_whitespace {
    $$ = $1;
    $$->push_back(new cssom::UnicodeRangeValue($3.first, $3.second));
  }
  ;

// This property declares how white space inside the element is handled.
//   http://www.w3.org/TR/css3-text/#white-space-property
white_space_property_value:
    kNormalToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNormal().get());
  }
  | kNoWrapToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNoWrap().get());
  }
  | kPreToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetPre().get());
  }
  | common_values
  ;

// Specifies the content width of boxes.
//   http://www.w3.org/TR/CSS21/visudet.html#the-width-property
//   http://www.w3.org/TR/CSS2/visudet.html#propdef-min-width
width_property_value:
    positive_length_percent_property_value
  | auto
  | common_values
  ;

// 'max-width' value can be width property_value or 'none'.
//   http://www.w3.org/TR/CSS2/visudet.html#propdef-max-width
max_width_property_value:
    kNoneToken maybe_whitespace {
    $$ = AddRef(cssom::KeywordValue::GetNone().get());
  }
  | width_property_value
  ;

maybe_important:
    /* empty */ { $$ = false; }
  | kImportantToken maybe_whitespace { $$ = true; }
  ;

z_index_property_value:
  integer {
    $$ = AddRef(new cssom::IntegerValue($1));
  }
  | auto
  | common_values
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
    $$ = cssom::kAllProperty;
  }
  | kBackgroundColorToken {
    $$ = cssom::kBackgroundColorProperty;
  }
  | kBorderRadiusToken {
    $$ = cssom::kBorderRadiusProperty;
  }
  | kColorToken {
    $$ = cssom::kColorProperty;
  }
  | kOpacityToken {
    $$ = cssom::kOpacityProperty;
  }
  | kTransformToken {
    $$ = cssom::kTransformProperty;
  }
  ;


// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Declarations.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

// Consume a declaration.
//   http://www.w3.org/TR/css3-syntax/#consume-a-declaration0
maybe_declaration:
    /* empty */ { $$ = NULL; }
  | kAnimationDelayToken maybe_whitespace colon
      animation_delay_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kAnimationDelayProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kAnimationDirectionToken maybe_whitespace colon
      animation_direction_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kAnimationDirectionProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kAnimationDurationToken maybe_whitespace colon
      animation_duration_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kAnimationDurationProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kAnimationFillModeToken maybe_whitespace colon
      animation_fill_mode_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kAnimationFillModeProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kAnimationIterationCountToken maybe_whitespace colon
      animation_iteration_count_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kAnimationIterationCountProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kAnimationNameToken maybe_whitespace colon
      animation_name_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kAnimationNameProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kAnimationTimingFunctionToken maybe_whitespace colon
      animation_timing_function_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kAnimationTimingFunctionProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kAnimationToken maybe_whitespace colon
      animation_property_value maybe_important {
    scoped_ptr<AnimationShorthand> animation($4);
    DCHECK(animation);

    scoped_ptr<PropertyDeclaration> property_declaration(
        new PropertyDeclaration($5));

    // Unpack the animation shorthand property values.
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kAnimationDelayProperty,
            animation->delay_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kAnimationDirectionProperty,
            animation->direction_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kAnimationDurationProperty,
            animation->duration_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kAnimationFillModeProperty,
            animation->fill_mode_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kAnimationIterationCountProperty,
            animation->iteration_count_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kAnimationNameProperty,
            animation->name_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kAnimationTimingFunctionProperty,
            animation->timing_function_list));

    $$ = property_declaration.release();
  }
  | kBackgroundToken maybe_whitespace colon background_property_value
      maybe_important {
    scoped_ptr<BackgroundShorthandLayer> background($4);
    DCHECK(background);
    scoped_ptr<PropertyDeclaration> property_declaration(
        new PropertyDeclaration($5));

    // Unpack the background shorthand property values.
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kBackgroundColorProperty,
            background->background_color));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kBackgroundImageProperty,
            background->background_image));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kBackgroundPositionProperty,
            background->background_position));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kBackgroundRepeatProperty,
            background->background_repeat));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kBackgroundSizeProperty,
            background->background_size));

    $$ = property_declaration.release();
  }
  | kBackgroundColorToken maybe_whitespace colon background_color_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBackgroundColorProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBackgroundImageToken maybe_whitespace colon background_image_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBackgroundImageProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBackgroundPositionToken maybe_whitespace colon
    background_position_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBackgroundPositionProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBackgroundRepeatToken maybe_whitespace colon
    background_repeat_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBackgroundRepeatProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBackgroundSizeToken maybe_whitespace colon background_size_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBackgroundSizeProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBorderToken maybe_whitespace colon border_property_value
      maybe_important {
    scoped_ptr<BorderShorthand> border($4);
    DCHECK(border);

    scoped_ptr<PropertyDeclaration> property_declaration(
        new PropertyDeclaration($5));

    // Unpack the border shorthand property values.
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kBorderColorProperty, border->border_color));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kBorderStyleProperty, border->border_style));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kBorderWidthProperty, border->border_width));

    $$ = property_declaration.release();
  }
  | kBorderColorToken maybe_whitespace colon border_color_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBorderColorProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBorderRadiusToken maybe_whitespace colon border_radius_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBorderRadiusProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBorderStyleToken maybe_whitespace colon border_style_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBorderStyleProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBorderWidthToken maybe_whitespace colon border_width_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBorderWidthProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kBottomToken maybe_whitespace colon offset_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kBottomProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kColorToken maybe_whitespace colon color_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kColorProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kContentToken maybe_whitespace colon content_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kContentProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kDisplayToken maybe_whitespace colon display_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kDisplayProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kFontFamilyToken maybe_whitespace colon font_family_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kFontFamilyProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kFontSizeToken maybe_whitespace colon font_size_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kFontSizeProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kFontStyleToken maybe_whitespace colon font_style_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kFontStyleProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kFontWeightToken maybe_whitespace colon font_weight_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kFontWeightProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kHeightToken maybe_whitespace colon height_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kHeightProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kLeftToken maybe_whitespace colon offset_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kLeftProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kLineHeightToken maybe_whitespace colon line_height_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kLineHeightProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kMarginBottomToken maybe_whitespace colon margin_side_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kMarginBottomProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kMarginLeftToken maybe_whitespace colon margin_side_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kMarginLeftProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kMarginRightToken maybe_whitespace colon margin_side_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kMarginRightProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kMarginTopToken maybe_whitespace colon margin_side_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kMarginTopProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kMarginToken maybe_whitespace colon margin_property_value maybe_important {
    scoped_ptr<MarginOrPaddingShorthand> margin($4);
    if (margin) {
      scoped_ptr<PropertyDeclaration> property_declaration(
          new PropertyDeclaration($5));

      property_declaration->property_values.push_back(
          PropertyDeclaration::PropertyKeyValuePair(
              cssom::kMarginTopProperty, margin->top));
      property_declaration->property_values.push_back(
          PropertyDeclaration::PropertyKeyValuePair(
              cssom::kMarginRightProperty, margin->right));
      property_declaration->property_values.push_back(
          PropertyDeclaration::PropertyKeyValuePair(
              cssom::kMarginBottomProperty, margin->bottom));
      property_declaration->property_values.push_back(
          PropertyDeclaration::PropertyKeyValuePair(
              cssom::kMarginLeftProperty, margin->left));

      $$ = property_declaration.release();
    } else {
      $$ = NULL;
    }
  }
  | kMaxHeightToken maybe_whitespace colon max_height_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kMaxHeightProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kMaxWidthToken maybe_whitespace colon max_width_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kMaxWidthProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kMinHeightToken maybe_whitespace colon height_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kMinHeightProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kMinWidthToken maybe_whitespace colon width_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kMinWidthProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kOpacityToken maybe_whitespace colon opacity_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kOpacityProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kOverflowToken maybe_whitespace colon overflow_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kOverflowProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kOverflowWrapToken maybe_whitespace colon overflow_wrap_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kOverflowWrapProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kPaddingBottomToken maybe_whitespace colon padding_side_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kPaddingBottomProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kPaddingLeftToken maybe_whitespace colon padding_side_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kPaddingLeftProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kPaddingRightToken maybe_whitespace colon padding_side_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kPaddingRightProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kPaddingTopToken maybe_whitespace colon padding_side_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kPaddingTopProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kPaddingToken maybe_whitespace colon padding_property_value
      maybe_important {
    scoped_ptr<MarginOrPaddingShorthand> padding($4);
    if (padding) {
      scoped_ptr<PropertyDeclaration> property_declaration(
          new PropertyDeclaration($5));

      property_declaration->property_values.push_back(
          PropertyDeclaration::PropertyKeyValuePair(
              cssom::kPaddingTopProperty, padding->top));
      property_declaration->property_values.push_back(
          PropertyDeclaration::PropertyKeyValuePair(
              cssom::kPaddingRightProperty, padding->right));
      property_declaration->property_values.push_back(
          PropertyDeclaration::PropertyKeyValuePair(
              cssom::kPaddingBottomProperty, padding->bottom));
      property_declaration->property_values.push_back(
          PropertyDeclaration::PropertyKeyValuePair(
              cssom::kPaddingLeftProperty, padding->left));

      $$ = property_declaration.release();
    } else {
      $$ = NULL;
    }
  }
  | kPositionToken maybe_whitespace colon position_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kPositionProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kRightToken maybe_whitespace colon offset_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kRightProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kSrcToken maybe_whitespace colon font_face_src_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kSrcProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTabSizeToken maybe_whitespace colon tab_size_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTabSizeProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTextAlignToken maybe_whitespace colon text_align_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTextAlignProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTextIndentToken maybe_whitespace colon text_indent_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTextIndentProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTextOverflowToken maybe_whitespace colon text_overflow_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTextOverflowProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTextTransformToken maybe_whitespace colon text_transform_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTextTransformProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTopToken maybe_whitespace colon offset_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTopProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTransformToken maybe_whitespace colon transform_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTransformProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTransitionToken maybe_whitespace colon
      transition_property_value maybe_important {
    scoped_ptr<TransitionShorthand> transition($4);
    DCHECK(transition);

    scoped_ptr<PropertyDeclaration> property_declaration(
        new PropertyDeclaration($5));

    // Unpack the transition shorthand property values.
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kTransitionPropertyProperty,
            transition->property_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kTransitionDurationProperty,
            transition->duration_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kTransitionTimingFunctionProperty,
            transition->timing_function_list));
    property_declaration->property_values.push_back(
        PropertyDeclaration::PropertyKeyValuePair(
            cssom::kTransitionDelayProperty,
            transition->delay_list));

    $$ = property_declaration.release();
  }
  | kTransitionDelayToken maybe_whitespace colon
      transition_delay_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTransitionDelayProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTransitionDurationToken maybe_whitespace colon
      transition_duration_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTransitionDurationProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTransitionPropertyToken maybe_whitespace colon
      transition_property_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTransitionPropertyProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kTransitionTimingFunctionToken maybe_whitespace colon
      transition_timing_function_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kTransitionTimingFunctionProperty,
                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kUnicodeRangePropertyToken maybe_whitespace colon
      unicode_range_property_value maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kUnicodeRangeProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kVerticalAlignToken maybe_whitespace colon vertical_align_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kVerticalAlignProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kVisibilityToken maybe_whitespace colon visibility_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kVisibilityProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kWhiteSpacePropertyToken maybe_whitespace colon white_space_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kWhiteSpaceProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kWidthToken maybe_whitespace colon width_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kWidthProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kZIndexToken maybe_whitespace colon z_index_property_value
      maybe_important {
    $$ = $4 ? new PropertyDeclaration(cssom::kZIndexProperty,
                                      MakeScopedRefPtrAndRelease($4), $5)
            : NULL;
  }
  | kIdentifierToken maybe_whitespace colon errors {
    std::string property_name = $1.ToString();
    DCHECK_GT(property_name.size(), 0U);

#ifdef __LB_SHELL__FORCE_LOGGING__
    // Do not warn about non-standard or non-WebKit properties.
    if (property_name[0] != '-') {
      DCHECK(non_trivial_static_fields.Get().
          thread_checker.CalledOnValidThread());
      base::hash_set<std::string>& properties_warned_about =
          non_trivial_static_fields.Get().properties_warned_about;

      if (properties_warned_about.find(property_name) ==
          properties_warned_about.end()) {
        properties_warned_about.insert(property_name);
        parser_impl->LogWarning(@1, "unsupported property " + property_name);
      }
    }
#endif  // __LB_SHELL__FORCE_LOGGING__

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
style_declaration_list:
    maybe_declaration {
    $$ = AddRef(new cssom::CSSStyleDeclarationData());

    scoped_ptr<PropertyDeclaration> property_declaration($1);
    if (property_declaration) {
      property_declaration->Apply($$);
    }
  }
  | style_declaration_list semicolon maybe_declaration {
    $$ = $1;

    scoped_ptr<PropertyDeclaration> property_declaration($3);
    if (property_declaration) {
      property_declaration->Apply($$);
    }
  }
  ;

font_face_declaration_list:
    maybe_declaration {
    $$ = AddRef(new cssom::CSSFontFaceDeclarationData());

    scoped_ptr<PropertyDeclaration> property_declaration($1);
    if (property_declaration) {
      property_declaration->Apply($$);
    }
  }
  | font_face_declaration_list semicolon maybe_declaration {
    $$ = $1;

    scoped_ptr<PropertyDeclaration> property_declaration($3);
    if (property_declaration) {
      property_declaration->Apply($$);
    }
  }
  ;

style_declaration_block:
    '{' maybe_whitespace style_declaration_list '}' maybe_whitespace {
    $$ = AddRef(new cssom::CSSStyleDeclaration(
             MakeScopedRefPtrAndRelease($3), parser_impl->css_parser()));
  }
  ;

rule_list_block:
    '{' maybe_whitespace rule_list '}' maybe_whitespace {
    $$ = $3;
  }
  | semicolon {
    $$ = AddRef(new cssom::CSSRuleList());
  }
  ;

// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Rules.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

// A style rule is a qualified rule that associates a selector list with a list
// of property declarations.
//   http://www.w3.org/TR/css3-syntax/#style-rule
style_rule:
    selector_list style_declaration_block {
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
qualified_rule: style_rule;

invalid_rule:
    error ';' maybe_whitespace { parser_impl->LogWarning(@1, "invalid rule"); }
  | error maybe_whitespace kIdentifierToken maybe_whitespace '{'
    maybe_whitespace keyframe_rule_list '}' maybe_whitespace {
      scoped_refptr<cssom::CSSRuleList> unused_rule_list =
          MakeScopedRefPtrAndRelease($7);
      parser_impl->LogWarning(@1, "invalid rule");
    }
  | error style_declaration_block {
    scoped_refptr<cssom::CSSStyleDeclaration> unused_style =
        MakeScopedRefPtrAndRelease($2);
    parser_impl->LogWarning(@1, "invalid rule");
  }
  ;

// Consume a list of rules.
//   http://www.w3.org/TR/css3-syntax/#consume-a-list-of-rules
rule:
    kSgmlCommentDelimiterToken maybe_whitespace { $$ = NULL; }
  | qualified_rule { $$ = $1; }
  | at_font_face_rule { $$ = $1; }
  | at_media_rule { $$ = $1; }
  | at_keyframes_rule { $$ = $1; }
  | invalid_rule { $$ = NULL; }
  ;

rule_list:
    /* empty */ {
    $$ = AddRef(new cssom::CSSRuleList());
  }
  | rule_list rule {
    $$ = $1;
    scoped_refptr<cssom::CSSRule> css_rule =
        MakeScopedRefPtrAndRelease($2);
    if (css_rule) {
      $$->AppendCSSRule(css_rule);
    }
  }
  ;


// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.
// Entry points.
// ...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:...:.

// To parse a stylesheet, consume a list of rules.
//   http://www.w3.org/TR/css3-syntax/#parse-a-stylesheet
style_sheet: rule_list {
    $$ = AddRef(new cssom::CSSStyleSheet(parser_impl->css_parser()));
    $$->set_css_rules(MakeScopedRefPtrAndRelease($1));
}

// Parser entry points.
//   http://dev.w3.org/csswg/css-syntax/#parser-entry-points
entry_point:
  // Parses the entire stylesheet.
    kStyleSheetEntryPointToken maybe_whitespace style_sheet {
    scoped_refptr<cssom::CSSStyleSheet> style_sheet =
        MakeScopedRefPtrAndRelease($3);
    parser_impl->set_style_sheet(style_sheet);
  }
  // Parses the media list.
  | kMediaListEntryPointToken maybe_whitespace media_list {
    scoped_refptr<cssom::MediaList> media_list =
        MakeScopedRefPtrAndRelease($3);
    parser_impl->set_media_list(media_list);
  }
  // Parses the media query.
  | kMediaQueryEntryPointToken maybe_whitespace media_query {
    scoped_refptr<cssom::MediaQuery> media_query =
        MakeScopedRefPtrAndRelease($3);
    parser_impl->set_media_query(media_query);
  }
  // Parses the rule.
  | kRuleEntryPointToken maybe_whitespace rule {
    scoped_refptr<cssom::CSSRule> rule = MakeScopedRefPtrAndRelease($3);
    parser_impl->set_rule(rule);
  }
  // Parses the contents of a HTMLElement.style attribute.
  | kStyleDeclarationListEntryPointToken maybe_whitespace
        style_declaration_list {
    scoped_refptr<cssom::CSSStyleDeclarationData> declaration_data =
        MakeScopedRefPtrAndRelease($3);
    parser_impl->set_style_declaration_data(declaration_data);
  }
  // Parses the contents of an @font-face rule.
  | kFontFaceDeclarationListEntryPointToken maybe_whitespace
        font_face_declaration_list {
    scoped_refptr<cssom::CSSFontFaceDeclarationData> declaration_data =
        MakeScopedRefPtrAndRelease($3);
    parser_impl->set_font_face_declaration_data(declaration_data);
  }
  // Parses a single non-shorthand property value.
  | kPropertyValueEntryPointToken maybe_whitespace maybe_declaration {
    scoped_ptr<PropertyDeclaration> property_declaration($3);
    if (property_declaration != NULL) {
      DCHECK_EQ(1, property_declaration->property_values.size()) <<
          "Cannot parse shorthand properties as single property values.";
      if (property_declaration->is_important) {
        parser_impl->LogWarning(
          @1, "!important is not allowed when setting single property values.");
      } else {
        if (!property_declaration->property_values[0].value) {
          parser_impl->LogWarning(
            @1, "declaration must have a property value.");
        } else {
          parser_impl->set_property_value(
              property_declaration->property_values[0].value);
        }
      }
    }
  }
  // Parses the property value and correspondingly sets the values of a passed
  // in CSSDeclarationData.
  // This is Cobalt's equivalent of a "list of component values".
  | kPropertyIntoDeclarationDataEntryPointToken maybe_whitespace
        maybe_declaration {
    scoped_ptr<PropertyDeclaration> property_declaration($3);
    if (property_declaration != NULL) {
      if (property_declaration->is_important) {
        parser_impl->LogError(
            @1, "!important is not allowed when setting single property value");
      } else {
        DCHECK(parser_impl->into_declaration_data());
        property_declaration->Apply(
            parser_impl->into_declaration_data());
      }
    }
  }
  ;
