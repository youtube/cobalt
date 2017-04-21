# Copyright 2014 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This file is meant to be included by a GYP target that needs to copy the
# platform specific font data into <(PRODUCT_DIR)/content.

{
  'includes': [ '../build/contents_dir.gypi' ],
  'variables': {
    'source_font_files_dir': '<(static_contents_source_dir)/fonts/font_files',

    'include_minimal_font': 0,

    'include_named_roboto_regular': 0,
    'include_named_roboto_bold': 0,
    'include_named_roboto_italic': 0,
    'include_named_noto_serif_regular': 0,
    'include_named_noto_serif_bold': 0,
    'include_named_noto_serif_italic': 0,
    'include_named_fcc_fonts_regular': 0,
    'include_named_fcc_fonts_bold': 0,

    'include_fallback_lang_non_cjk_noto_regular': 0,
    'include_fallback_lang_non_cjk_noto_bold': 0,
    'include_fallback_lang_cjk_noto_regular': 0,
    'include_fallback_lang_cjk_noto_bold': 0,
    'include_fallback_lang_cjk_droid_regular': 0,
    'include_fallback_lang_jp_noto_regular': 0,
    'include_fallback_emoji_noto_regular': 0,
    'include_fallback_symbols_noto_regular': 0,

    'conditions': [
      [ 'cobalt_font_package == "expanded"', {
        'source_xml_file_dir': '<(static_contents_source_dir)/fonts/xml/common',

        'include_named_roboto_regular': 1,
        'include_named_roboto_bold': 1,
        'include_named_roboto_italic': 1,
        'include_named_noto_serif_regular': 1,
        'include_named_noto_serif_bold': 1,
        'include_named_noto_serif_italic': 1,
        'include_named_fcc_fonts_regular': 1,
        'include_named_fcc_fonts_bold': 1,

        'include_fallback_lang_non_cjk_noto_regular': 1,
        'include_fallback_lang_non_cjk_noto_bold': 1,
        'include_fallback_lang_cjk_noto_regular': 1,
        'include_fallback_lang_cjk_noto_bold': 1,
        'include_fallback_emoji_noto_regular': 1,
        'include_fallback_symbols_noto_regular': 1,
      }],

      # 'unlimited' is deprecated but is mapped to 'standard'
      [ 'cobalt_font_package == "standard" or cobalt_font_package == "unlimited"', {
        'source_xml_file_dir': '<(static_contents_source_dir)/fonts/xml/common',

        'include_named_roboto_regular': 1,
        'include_named_roboto_bold': 1,
        'include_named_roboto_italic': 1,
        'include_named_noto_serif_regular': 1,
        'include_named_noto_serif_bold': 1,
        'include_named_noto_serif_italic': 1,
        'include_named_fcc_fonts_regular': 1,
        'include_named_fcc_fonts_bold': 1,

        'include_fallback_lang_non_cjk_noto_regular': 1,
        'include_fallback_lang_non_cjk_noto_bold': 1,
        'include_fallback_lang_cjk_noto_regular': 1,
        'include_fallback_emoji_noto_regular': 1,
        'include_fallback_symbols_noto_regular': 1,
      }],

      # '10megabytes' is deprecated but is mapped to 'limited_with_noto_jp'
      [ 'cobalt_font_package == "limited_with_noto_jp" or cobalt_font_package == "10megabytes"', {
        'source_xml_file_dir': '<(static_contents_source_dir)/fonts/xml/common',

        'include_named_roboto_regular': 1,
        'include_named_roboto_bold': 1,

        'include_fallback_lang_non_cjk_noto_regular': 1,
        'include_fallback_lang_cjk_droid_regular': 1,
        'include_fallback_lang_jp_noto_regular': 1,
        'include_fallback_emoji_noto_regular': 1,
        'include_fallback_symbols_noto_regular': 1,
      }],

      [ 'cobalt_font_package == "limited"', {
        'source_xml_file_dir': '<(static_contents_source_dir)/fonts/xml/common',

        'include_named_roboto_regular': 1,
        'include_named_roboto_bold': 1,

        'include_fallback_lang_non_cjk_noto_regular': 1,
        'include_fallback_lang_cjk_droid_regular': 1,
        'include_fallback_emoji_noto_regular': 1,
        'include_fallback_symbols_noto_regular': 1,
      }],

      [ 'cobalt_font_package == "minimal"', {
        'source_xml_file_dir': '<(static_contents_source_dir)/fonts/xml/minimal',

        'include_minimal_font': 1,
      }],

      [ 'cobalt_font_package == "android_system"', {
        # fonts.xml contains a superset of what we expect to find on Android
        # devices. The Android SbFile implementation falls back to system font
        # files for those not in cobalt content.
        'source_xml_file_dir': '<(static_contents_source_dir)/fonts/xml/android',

        # Include minimal font to guarantee that a fallback Roboto font is
        # available for Basic Latin.
        'include_minimal_font': 1,

        # Emojis are currently included because Cobalt's version of Skia does
        # not support Android's color emojis and will be removed when Skia is
        # rebased.
        'include_fallback_emoji_noto_regular': 1,
      }],
    ],
  },

  'copies': [
    {
      'destination': '<(static_contents_output_data_dir)/fonts/',
      'files': [
        '<(source_xml_file_dir)/fonts.xml',
      ],
      'conditions': [
        [ 'include_minimal_font', {
          'files+': [
            '<(source_font_files_dir)/Roboto-Regular-Subsetted.ttf',
          ],
        }],
        [ 'include_named_roboto_regular', {
          'files+': [
            '<(source_font_files_dir)/Roboto-Regular.ttf',
          ],
        }],
        [ 'include_named_roboto_bold', {
          'files+': [
            '<(source_font_files_dir)/Roboto-Bold.ttf',
          ],
        }],
        [ 'include_named_roboto_italic', {
          'files+': [
            '<(source_font_files_dir)/Roboto-Italic.ttf',
            '<(source_font_files_dir)/Roboto-BoldItalic.ttf',
          ],
        }],
        [ 'include_named_noto_serif_regular', {
          'files+': [
            '<(source_font_files_dir)/NotoSerif-Regular.ttf',
          ],
        }],
        [ 'include_named_noto_serif_bold', {
          'files+': [
            '<(source_font_files_dir)/NotoSerif-Bold.ttf',
          ],
        }],
        [ 'include_named_noto_serif_italic', {
          'files+': [
            '<(source_font_files_dir)/NotoSerif-Italic.ttf',
            '<(source_font_files_dir)/NotoSerif-BoldItalic.ttf',
          ],
        }],
        [ 'include_named_fcc_fonts_regular', {
          'files+': [
            # sans-serif-monospace
            '<(source_font_files_dir)/DroidSansMono.ttf',
            # serif-monospace
            '<(source_font_files_dir)/CutiveMono.ttf',
            # casual
            '<(source_font_files_dir)/ComingSoon.ttf',
            # cursive
            '<(source_font_files_dir)/DancingScript-Regular.ttf',
            # sans-serif-smallcaps
            '<(source_font_files_dir)/CarroisGothicSC-Regular.ttf',
          ],
        }],
        [ 'include_named_fcc_fonts_bold', {
          'files+': [
            # cursive
            '<(source_font_files_dir)/DancingScript-Bold.ttf',
          ],
        }],
        [ 'include_fallback_lang_non_cjk_noto_regular', {
          'files+': [
            '<(source_font_files_dir)/NotoNaskhArabicUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansArmenian-Regular.ttf',
            '<(source_font_files_dir)/NotoSansBalinese-Regular.ttf',
            '<(source_font_files_dir)/NotoSansBamum-Regular.ttf',
            '<(source_font_files_dir)/NotoSansBatak-Regular.ttf',
            '<(source_font_files_dir)/NotoSansBengaliUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansBuginese-Regular.ttf',
            '<(source_font_files_dir)/NotoSansBuhid-Regular.ttf',
            '<(source_font_files_dir)/NotoSansCanadianAboriginal-Regular.ttf',
            '<(source_font_files_dir)/NotoSansCham-Regular.ttf',
            '<(source_font_files_dir)/NotoSansCherokee-Regular.ttf',
            '<(source_font_files_dir)/NotoSansCoptic-Regular.ttf',
            '<(source_font_files_dir)/NotoSansDevanagariUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansEthiopic-Regular.ttf',
            '<(source_font_files_dir)/NotoSansGeorgian-Regular.ttf',
            '<(source_font_files_dir)/NotoSansGlagolitic-Regular.ttf',
            '<(source_font_files_dir)/NotoSansGujaratiUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansGurmukhiUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansHanunoo-Regular.ttf',
            '<(source_font_files_dir)/NotoSansHebrew-Regular.ttf',
            '<(source_font_files_dir)/NotoSansJavanese-Regular.ttf',
            '<(source_font_files_dir)/NotoSansKannadaUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansKayahLi-Regular.ttf',
            '<(source_font_files_dir)/NotoSansKhmerUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansLaoUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansLepcha-Regular.ttf',
            '<(source_font_files_dir)/NotoSansLimbu-Regular.ttf',
            '<(source_font_files_dir)/NotoSansLisu-Regular.ttf',
            '<(source_font_files_dir)/NotoSansMalayalamUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansMandaic-Regular.ttf',
            '<(source_font_files_dir)/NotoSansMeeteiMayek-Regular.ttf',
            '<(source_font_files_dir)/NotoSansMongolian-Regular.ttf',
            '<(source_font_files_dir)/NotoSansMyanmarUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansNewTaiLue-Regular.ttf',
            '<(source_font_files_dir)/NotoSansNKo-Regular.ttf',
            '<(source_font_files_dir)/NotoSansOlChiki-Regular.ttf',
            '<(source_font_files_dir)/NotoSansOriyaUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansRejang-Regular.ttf',
            '<(source_font_files_dir)/NotoSansSaurashtra-Regular.ttf',
            '<(source_font_files_dir)/NotoSansSinhala-Regular.ttf',
            '<(source_font_files_dir)/NotoSansSundanese-Regular.ttf',
            '<(source_font_files_dir)/NotoSansSylotiNagri-Regular.ttf',
            '<(source_font_files_dir)/NotoSansSyriacEstrangela-Regular.ttf',
            '<(source_font_files_dir)/NotoSansTagbanwa-Regular.ttf',
            '<(source_font_files_dir)/NotoSansTaiLe-Regular.ttf',
            '<(source_font_files_dir)/NotoSansTaiTham-Regular.ttf',
            '<(source_font_files_dir)/NotoSansTaiViet-Regular.ttf',
            '<(source_font_files_dir)/NotoSansTamilUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansTeluguUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansThaana-Regular.ttf',
            '<(source_font_files_dir)/NotoSansThaiUI-Regular.ttf',
            '<(source_font_files_dir)/NotoSansTibetan-Regular.ttf',
            '<(source_font_files_dir)/NotoSansTifinagh-Regular.ttf',
            '<(source_font_files_dir)/NotoSansVai-Regular.ttf',
            '<(source_font_files_dir)/NotoSansYi-Regular.ttf',
          ],
        }],
        [ 'include_fallback_lang_non_cjk_noto_bold', {
          'files+': [
            '<(source_font_files_dir)/NotoNaskhArabicUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansArmenian-Bold.ttf',
            '<(source_font_files_dir)/NotoSansBengaliUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansCham-Bold.ttf',
            '<(source_font_files_dir)/NotoSansDevanagariUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansEthiopic-Bold.ttf',
            '<(source_font_files_dir)/NotoSansGeorgian-Bold.ttf',
            '<(source_font_files_dir)/NotoSansGujaratiUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansGurmukhiUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansHebrew-Bold.ttf',
            '<(source_font_files_dir)/NotoSansKannadaUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansKhmerUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansLaoUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansMalayalamUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansMyanmarUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansOriyaUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansSinhala-Bold.ttf',
            '<(source_font_files_dir)/NotoSansTamilUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansTeluguUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansThaana-Bold.ttf',
            '<(source_font_files_dir)/NotoSansThaiUI-Bold.ttf',
            '<(source_font_files_dir)/NotoSansTibetan-Bold.ttf',
          ],
        }],
        [ 'include_fallback_lang_cjk_noto_regular', {
          'files+': [
            '<(source_font_files_dir)/NotoSansCJK-Regular.ttc',
          ],
        }],
        [ 'include_fallback_lang_cjk_noto_bold', {
          'files+': [
            '<(source_font_files_dir)/NotoSansCJK-Bold.ttc',
          ],
        }],
        [ 'include_fallback_lang_cjk_droid_regular', {
          'files+': [
            '<(source_font_files_dir)/DroidSansFallback.ttf',
          ],
        }],
        [ 'include_fallback_lang_jp_noto_regular', {
          'files+': [
            '<(source_font_files_dir)/NotoSansJP-Regular.otf',
          ],
        }],
        [ 'include_fallback_emoji_noto_regular', {
          'files+': [
            '<(source_font_files_dir)/NotoEmoji-Regular.ttf',
          ],
        }],
        [ 'include_fallback_symbols_noto_regular', {
          'files+': [
            '<(source_font_files_dir)/NotoSansSymbols-Regular-Subsetted.ttf',
            '<(source_font_files_dir)/NotoSansSymbols-Regular-Subsetted2.ttf',
          ],
        }],
      ],
    },
  ],
}
