## Description

This directory contains fonts that can be packaged with Cobalt.

## How to use

To use this:

1.  Select one of the packages below.
2.  Add a variable named `cobalt_font_package` in your platform's
    `gyp_configuration.gypi` file.
3.  Optional: Add package overrides, which can be used to add or remove fonts
    from the package.

Example:
    This example uses the 'limited' package, but overrides it to include bold
    non-CJK language fallback and to not include any CJK language fallback.

    'variables': {
        'cobalt_font_package': 'limited',
        'cobalt_font_package_override_fallback_lang_non_cjk': 2,
        'cobalt_font_package_override_fallback_lang_cjk_low_quality': 0,
    }

## Packages
*  'expanded' -- The largest package. It includes everything in the 'standard'
                 package, along with 'bold' weight CJK. It is recommended that
                 'local_font_cache_size_in_bytes' be increased to 24MB when
                 using this package to account for the extra memory required by
                 bold CJK. This package is ~46.2MB.

                 Package category values:
                   'package_named_sans_serif': 4,
                   'package_named_serif': 4,
                   'package_named_fcc_fonts': 2,
                   'package_fallback_lang_non_cjk': 2,
                   'package_fallback_lang_cjk': 2,
                   'package_fallback_lang_cjk_low_quality': 0,
                   'package_fallback_lang_jp': 0,
                   'package_fallback_emoji': 1,
                   'package_fallback_symbols': 1,

*  'standard' -- The default package. It includes all non-CJK fallback fonts in
                 both 'normal' and 'bold' weights, 'normal' weight CJK ('bold'
                 weight CJK is synthesized from it), and all FCC fonts. This
                 package is ~26.9MB.

                 Package category values:
                  'package_named_sans_serif': 3,
                  'package_named_serif': 3,
                  'package_named_fcc_fonts': 2,
                  'package_fallback_lang_non_cjk': 2,
                  'package_fallback_lang_cjk': 1,
                  'package_fallback_lang_cjk_low_quality': 0,
                  'package_fallback_lang_jp': 0,
                  'package_fallback_emoji': 1,
                  'package_fallback_symbols': 1,

*  'limited_with_jp' -- A significantly smaller package than 'standard'. This
                 package removes the 'bold' weighted non-CJK fallback fonts (the
                 'normal' weight is still included and is used to synthesize
                 bold), removes the FCC fonts (which must be downloaded from the
                 web), and replaces standard CJK with low quality CJK. However,
                 higher quality Japanese is still included. Because low quality
                 CJK cannot synthesize bold, bold glyphs are unavailable in
                 Chinese and Korean. This package is ~10.9MB.

                 Package category values:
                  'package_named_sans_serif': 2,
                  'package_named_serif': 0,
                  'package_named_fcc_fonts': 0,
                  'package_fallback_lang_non_cjk': 1,
                  'package_fallback_lang_cjk': 0,
                  'package_fallback_lang_cjk_low_quality': 1,
                  'package_fallback_lang_jp': 1,
                  'package_fallback_emoji': 1,
                  'package_fallback_symbols': 1,

*  'limited'  -- A smaller package than 'limited_with_jp'. The two packages are
                 identical with the exception that 'limited' does not include
                 the higher quality Japanese font; instead it relies on low
                 quality CJK for all CJK characters. Because low quality CJK
                 cannot synthesize bold, bold glyphs are unavailable in Chinese,
                 Japanese, and Korean. This package is ~7.7MB.

                 Package category values:
                  'package_named_sans_serif': 2,
                  'package_named_serif': 0,
                  'package_named_fcc_fonts': 0,
                  'package_fallback_lang_non_cjk': 1,
                  'package_fallback_lang_cjk': 0,
                  'package_fallback_lang_cjk_low_quality': 1,
                  'package_fallback_lang_jp': 0,
                  'package_fallback_emoji': 1,
                  'package_fallback_symbols': 1,

*  'minimal'  -- The smallest possible font package. It only includes Roboto's
                 Basic Latin characters. Everything else must be downloaded from
                 the web. This package is ~16.4KB.

                 Package category values:
                  'package_named_sans_serif': 0,
                  'package_named_serif': 0,
                  'package_named_fcc_fonts': 0,
                  'package_fallback_lang_non_cjk': 0,
                  'package_fallback_lang_cjk': 0,
                  'package_fallback_lang_cjk_low_quality': 0,
                  'package_fallback_lang_jp': 0,
                  'package_fallback_emoji': 0,
                  'package_fallback_symbols': 0,

NOTE: When bold is needed, but unavailable, it is typically synthesized,
      resulting in lower quality glyphs than those generated directly from a
      bold font. However, this does not occur with low quality CJK, which is
      not high enough quality to synthesize. Its glyphs always have a regular
      weight.


## Package font categories
Each package contains values for the following categories, which specifies the
fonts from each category included within the package:
  *  'package_named_sans_serif':
       Named sans-serif fonts.

  *  'package_named_serif':
       Named serif fonts.

  *  'package_named_fcc_fonts':
       All FCC-required fonts that are not included within sans-serif or serif:
       monospace, serif-monospace, casual, cursive, and sans-serif-smallcaps.

  *  'package_fallback_lang_non_cjk':
       All non-CJK language-specific fallback fonts.

  *  'package_fallback_lang_cjk':
       Higher quality CJK language-specific fallback fonts.

  *  'package_fallback_lang_cjk_low_quality':
       Low quality CJK language-specific fallback fonts. These should only be
       included when 'package_fallback_lang_cjk' has a value of '0'. This is the
       only category of fonts that is not synthetically boldable.

  *  'package_fallback_lang_jp':
       Higher quality Japanese language-specific fallback fonts. These should
       only be included when 'package_fallback_lang_cjk' has a value of '0'.

  *  'package_fallback_emoji':
       Emoji-related fallback fonts.

  *  'package_fallback_symbols':
       Symbol-related fallback fonts.


## Package font category values
The following explains the meaning behind the values that packages use for each
of the font categories:
  *  0 -- No fonts from the specified category are included.
  *  1 -- Fonts from the specified category with a weight of 'normal' and a
          style of 'normal' are included.
  *  2 -- Fonts from the the specified category with a weight of either 'normal'
          or 'bold' and a style of 'normal' are included.
  *  3 -- Fonts from the the specified category with a weight of either 'normal'
          or 'bold' and a style of either 'normal' or 'italic' are included.
  *  4 -- All available fonts from the specified category are included. This may
          include additional weights beyond 'normal' and 'bold'.


## Overriding packages
Font package overrides can be used to modify the files included within the
selected package. The following override values are available for each font
category:
  * -1 -- The package value for the specified type is not overridden.
  *  0 -- The package value is overridden and no fonts from the specified
          category are included.
  *  1 -- The package value is overridden and fonts from the specified category
          with a weight of 'normal' and a style of 'normal' are included.
  *  2 -- The package value is overridden and fonts from the specified category
          with a weight of either 'normal' or bold' and a style of 'normal' are
          included.
  *  3 -- The package value is overridden and fonts from the specified category
          with a weight of either 'normal' or 'bold' and a style of either
          'normal' or 'italic' are included.
  *  4 -- The package value is overridden and all available fonts from the
          specified category are included. This may include additional weights
          beyond 'normal' and 'bold'.


## Override mappings
The mapping between the override category name and the package category name:
  *  'cobalt_font_package_override_named_sans_serif' ==>
       'package_named_sans_serif'
  *  'cobalt_font_package_override_named_serif' ==>
       'package_named_serif'
  *  'cobalt_font_package_override_named_fcc_fonts' ==>
       'package_named_fcc_fonts'
  *  'cobalt_font_package_override_fallback_lang_non_cjk' ==>
       'package_fallback_lang_non_cjk'
  *  'cobalt_font_package_override_fallback_lang_cjk' ==>
       'package_fallback_lang_cjk'
  *  'cobalt_font_package_override_fallback_lang_cjk_low_quality' ==>
       'package_fallback_lang_cjk_low_quality'
  *  'cobalt_font_package_override_fallback_lang_jp' ==>
       'package_fallback_lang_jp'
  *  'cobalt_font_package_override_fallback_emoji' ==>
       'package_fallback_emoji'
  *  'cobalt_font_package_override_fallback_symbols' ==>
       'package_fallback_symbols'
