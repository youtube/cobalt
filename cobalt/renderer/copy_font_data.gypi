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

    'conditions': [
      # 'unlimited' is deprecated but is mapped to 'standard'
      # 'expanded' also currently maps to 'standard' but this may change in the future
      [ 'cobalt_font_package == "standard" or cobalt_font_package == "unlimited" or cobalt_font_package == "expanded"', {
        'source_font_config_dir': '<(static_contents_source_dir)/fonts/config/common',

        'package_named_sans_serif': 4,
        'package_named_serif': 3,
        'package_named_fcc_fonts': 2,
        'package_fallback_lang_non_cjk': 2,
        'package_fallback_lang_cjk': 1,
        'package_fallback_lang_cjk_low_quality': 0,
        'package_fallback_lang_jp': 0,
        'package_fallback_historic': 1,
        'package_fallback_color_emoji': 1,
        'package_fallback_emoji': 0,
        'package_fallback_symbols': 1,
      }],

      # '10megabytes' is deprecated but is mapped to 'limited_with_jp'
      [ 'cobalt_font_package == "limited_with_jp" or cobalt_font_package == "10megabytes"', {
        'source_font_config_dir': '<(static_contents_source_dir)/fonts/config/common',

        'package_named_sans_serif': 2,
        'package_named_serif': 0,
        'package_named_fcc_fonts': 0,
        'package_fallback_lang_non_cjk': 1,
        'package_fallback_lang_cjk': 0,
        'package_fallback_lang_cjk_low_quality': 1,
        'package_fallback_lang_jp': 1,
        'package_fallback_historic': 0,
        'package_fallback_color_emoji': 0,
        'package_fallback_emoji': 1,
        'package_fallback_symbols': 1,
      }],

      [ 'cobalt_font_package == "limited"', {
        'source_font_config_dir': '<(static_contents_source_dir)/fonts/config/common',

        'package_named_sans_serif': 2,
        'package_named_serif': 0,
        'package_named_fcc_fonts': 0,
        'package_fallback_lang_non_cjk': 1,
        'package_fallback_lang_cjk': 0,
        'package_fallback_lang_cjk_low_quality': 1,
        'package_fallback_lang_jp': 0,
        'package_fallback_historic': 0,
        'package_fallback_color_emoji': 0,
        'package_fallback_emoji': 1,
        'package_fallback_symbols': 1,
      }],

      [ 'cobalt_font_package == "minimal"', {
        'source_font_config_dir': '<(static_contents_source_dir)/fonts/config/common',

        'package_named_sans_serif': 0,
        'package_named_serif': 0,
        'package_named_fcc_fonts': 0,
        'package_fallback_lang_non_cjk': 0,
        'package_fallback_lang_cjk': 0,
        'package_fallback_lang_cjk_low_quality': 0,
        'package_fallback_lang_jp': 0,
        'package_fallback_historic': 0,
        'package_fallback_color_emoji': 0,
        'package_fallback_emoji': 0,
        'package_fallback_symbols': 0,
      }],

      [ 'cobalt_font_package == "android_system"', {
        # fonts.xml contains a superset of what we expect to find on Android
        # devices. The Android SbFile implementation falls back to system font
        # files for those not in cobalt content.
        'source_font_config_dir': '<(static_contents_source_dir)/fonts/config/android',

        'package_named_sans_serif': 0,
        'package_named_serif': 0,
        'package_named_fcc_fonts': 0,
        'package_fallback_lang_non_cjk': 0,
        'package_fallback_lang_cjk': 0,
        'package_fallback_lang_cjk_low_quality': 0,
        'package_fallback_lang_jp': 0,
        'package_fallback_historic': 0,
        'package_fallback_color_emoji': 0,
        'package_fallback_emoji': 0,
        'package_fallback_symbols': 0,
      }],

      [ 'cobalt_font_package_override_named_sans_serif >= 0', {
        'package_named_sans_serif': '<(cobalt_font_package_override_named_sans_serif)',
      }],
      [ 'cobalt_font_package_override_named_serif >= 0', {
        'package_named_serif': '<(cobalt_font_package_override_named_serif)',
      }],
      [ 'cobalt_font_package_override_named_fcc_fonts >= 0', {
        'package_named_fcc_fonts': '<(cobalt_font_package_override_named_fcc_fonts)',
      }],
      [ 'cobalt_font_package_override_fallback_lang_non_cjk >= 0', {
        'package_fallback_lang_non_cjk': '<(cobalt_font_package_override_fallback_lang_non_cjk)',
      }],
      [ 'cobalt_font_package_override_fallback_lang_cjk >= 0', {
        'package_fallback_lang_cjk': '<(cobalt_font_package_override_fallback_lang_cjk)',
      }],
      [ 'cobalt_font_package_override_fallback_lang_cjk_low_quality >= 0', {
        'package_fallback_lang_cjk_low_quality': '<(cobalt_font_package_override_fallback_lang_cjk_low_quality)',
      }],
      [ 'cobalt_font_package_override_fallback_lang_jp >= 0', {
        'package_fallback_lang_jp': '<(cobalt_font_package_override_fallback_lang_jp)',
      }],
      [ 'cobalt_font_package_override_fallback_historic >= 0', {
        'package_fallback_historic': '<(cobalt_font_package_override_fallback_historic)',
      }],
      [ 'cobalt_font_package_override_fallback_color_emoji >= 0', {
        'package_fallback_color_emoji': '<(cobalt_font_package_override_fallback_color_emoji)',
      }],
      [ 'cobalt_font_package_override_fallback_emoji >= 0', {
        'package_fallback_emoji': '<(cobalt_font_package_override_fallback_emoji)',
      }],
      [ 'cobalt_font_package_override_fallback_symbols >= 0', {
        'package_fallback_symbols': '<(cobalt_font_package_override_fallback_symbols)',
      }],
    ],
  },

  'copies': [
    {
      'destination': '<(sb_static_contents_output_data_dir)/fonts/',
      'files': [
        '<(source_font_config_dir)/fonts.xml',
      ],
      'conditions': [
        [ 'package_named_sans_serif == 0', {
          'files+': [
            '<(source_font_files_dir)/Roboto-Regular-Subsetted.ttf',
          ],
        }],
        [ 'package_named_sans_serif >= 1', {
          'files+': [
            '<(source_font_files_dir)/Roboto-Regular.ttf',
          ],
        }],
        [ 'package_named_sans_serif >= 2', {
          'files+': [
            '<(source_font_files_dir)/Roboto-Bold.ttf',
          ],
        }],
        [ 'package_named_sans_serif >= 3', {
          'files+': [
            '<(source_font_files_dir)/Roboto-Italic.ttf',
            '<(source_font_files_dir)/Roboto-BoldItalic.ttf',
          ],
        }],
        [ 'package_named_sans_serif >= 4', {
          'files+': [
            '<(source_font_files_dir)/Roboto-Thin.ttf',
            '<(source_font_files_dir)/Roboto-ThinItalic.ttf',
            '<(source_font_files_dir)/Roboto-Light.ttf',
            '<(source_font_files_dir)/Roboto-LightItalic.ttf',
            '<(source_font_files_dir)/Roboto-Medium.ttf',
            '<(source_font_files_dir)/Roboto-MediumItalic.ttf',
            '<(source_font_files_dir)/Roboto-Black.ttf',
            '<(source_font_files_dir)/Roboto-BlackItalic.ttf',
          ],
        }],
        [ 'package_named_serif >= 1', {
          'files+': [
            '<(source_font_files_dir)/NotoSerif-Regular.ttf',
          ],
        }],
        [ 'package_named_serif >= 2', {
          'files+': [
            '<(source_font_files_dir)/NotoSerif-Bold.ttf',
          ],
        }],
        [ 'package_named_serif >= 3', {
          'files+': [
            '<(source_font_files_dir)/NotoSerif-Italic.ttf',
            '<(source_font_files_dir)/NotoSerif-BoldItalic.ttf',
          ],
        }],
        [ 'package_named_fcc_fonts >= 1', {
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
        [ 'package_named_fcc_fonts >= 2', {
          'files+': [
            # cursive
            '<(source_font_files_dir)/DancingScript-Bold.ttf',
          ],
        }],
        [ 'package_fallback_lang_non_cjk >= 1', {
          'files+': [
            '<(source_font_files_dir)/NotoSansAdlam-Regular.ttf',
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
            '<(source_font_files_dir)/NotoSansSyriacEastern-Regular.ttf',
            '<(source_font_files_dir)/NotoSansSyriacEstrangela-Regular.ttf',
            '<(source_font_files_dir)/NotoSansSyriacWestern-Regular.ttf',
            '<(source_font_files_dir)/NotoSansTagalog-Regular.ttf',
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
        [ 'package_fallback_lang_non_cjk >= 2', {
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
        [ 'package_fallback_lang_cjk >= 1', {
          'files+': [
            '<(source_font_files_dir)/NotoSansCJK-Regular.ttc',
          ],
        }],
        [ 'package_fallback_lang_cjk_low_quality >= 1', {
          'files+': [
            '<(source_font_files_dir)/DroidSansFallback.ttf',
          ],
        }],
        [ 'package_fallback_lang_jp >= 1', {
          'files+': [
            '<(source_font_files_dir)/NotoSansJP-Regular.otf',
          ],
        }],
        [ 'package_fallback_historic >= 1', {
          'files+': [
            '<(source_font_files_dir)/NotoSansAvestan-Regular.ttf',
            '<(source_font_files_dir)/NotoSansBrahmi-Regular.ttf',
            '<(source_font_files_dir)/NotoSansCarian-Regular.ttf',
            '<(source_font_files_dir)/NotoSansCuneiform-Regular.ttf',
            '<(source_font_files_dir)/NotoSansCypriot-Regular.ttf',
            '<(source_font_files_dir)/NotoSansDeseret-Regular.ttf',
            '<(source_font_files_dir)/NotoSansEgyptianHieroglyphs-Regular.ttf',
            '<(source_font_files_dir)/NotoSansGothic-Regular.ttf',
            '<(source_font_files_dir)/NotoSansImperialAramaic-Regular.ttf',
            '<(source_font_files_dir)/NotoSansInscriptionalPahlavi-Regular.ttf',
            '<(source_font_files_dir)/NotoSansInscriptionalParthian-Regular.ttf',
            '<(source_font_files_dir)/NotoSansKaithi-Regular.ttf',
            '<(source_font_files_dir)/NotoSansKharoshthi-Regular.ttf',
            '<(source_font_files_dir)/NotoSansLinearB-Regular.ttf',
            '<(source_font_files_dir)/NotoSansLycian-Regular.ttf',
            '<(source_font_files_dir)/NotoSansLydian-Regular.ttf',
            '<(source_font_files_dir)/NotoSansOgham-Regular.ttf',
            '<(source_font_files_dir)/NotoSansOldItalic-Regular.ttf',
            '<(source_font_files_dir)/NotoSansOldPersian-Regular.ttf',
            '<(source_font_files_dir)/NotoSansOldSouthArabian-Regular.ttf',
            '<(source_font_files_dir)/NotoSansOldTurkic-Regular.ttf',
            '<(source_font_files_dir)/NotoSansOsmanya-Regular.ttf',
            '<(source_font_files_dir)/NotoSansPhagsPa-Regular.ttf',
            '<(source_font_files_dir)/NotoSansPhoenician-Regular.ttf',
            '<(source_font_files_dir)/NotoSansRunic-Regular.ttf',
            '<(source_font_files_dir)/NotoSansSamaritan-Regular.ttf',
            '<(source_font_files_dir)/NotoSansShavian-Regular.ttf',
            '<(source_font_files_dir)/NotoSansUgaritic-Regular.ttf',
          ],
        }],
        [ 'package_fallback_color_emoji >= 1', {
          'files+': [
            '<(source_font_files_dir)/NotoColorEmoji.ttf',
          ],
        }],
        [ 'package_fallback_emoji >= 1', {
          'files+': [
            '<(source_font_files_dir)/NotoEmoji-Regular.ttf',
          ],
        }],
        [ 'package_fallback_symbols >= 1', {
          'files+': [
            '<(source_font_files_dir)/NotoSansSymbols-Regular-Subsetted.ttf',
            '<(source_font_files_dir)/NotoSansSymbols-Regular-Subsetted2.ttf',
          ],
        }],
      ],
    },
  ],
}
