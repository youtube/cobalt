# Fonts

By default, Cobalt includes a robust set of fonts that support most characters
encountered around the world. While this will likely meet many porters' needs,
the font system is also extremely configurable when desired.

It can be configured in the following ways:
  1. The porter can select from a variety of font packages that Cobalt offers
     and then override specific sections of those packages to add or remove
     additional fonts.
  2. The porter can use system fonts to complement or even replace Cobalt's
     fonts. However, it is important to note that, given the extensive language
     support that Cobalt provides in its font packages, care should be taken
     when replacing Cobalt's fonts to not significantly degrade that support.

Both font packages and system fonts are described in more detail below.

## Font Packages

While the fonts included in Cobalt's default font package should work well in
most cases, Cobalt offers the ability to customize the fonts included in builds
through package profile selection and package overrides.

### How to use

To use this:
  1.  Select one of the package profiles below.
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

### Package Profiles
*  'standard' -- The default package. It includes all sans-serif, serif, and FCC
                 fonts, non-CJK fallback fonts in both 'normal' and 'bold'
                 weights, 'normal' weight CJK ('bold' weight CJK is synthesized
                 from it), historic script fonts, and color emojis. This package
                 is ~38.3MB.

                 Package category values:
                  'package_named_sans_serif': 4,
                  'package_named_serif': 3,
                  'package_named_fcc_fonts': 2,
                  'package_fallback_lang_non_cjk': 2,
                  'package_fallback_lang_cjk': 1,
                  'package_fallback_lang_cjk_low_quality': 0,
                  'package_fallback_historic': 1,
                  'package_fallback_color_emoji': 1,
                  'package_fallback_emoji': 0,
                  'package_fallback_symbols': 1,

*  'limited'  -- A significantly smaller package than 'standard'. This package
                 removes all but 'normal' and 'bold' weighted sans-serif and
                 serif, removes the FCC fonts (which must be provided by the
                 system or downloaded from the web), replaces standard CJK with
                 low quality CJK, removes historic script fonts, and replaces
                 colored emojis with uncolored ones. Because low quality CJK
                 cannot synthesize bold, bold glyphs are unavailable in Chinese,
                 Japanese and Korean. This package is ~8.3MB.

                 Package category values:
                  'package_named_sans_serif': 2,
                  'package_named_serif': 0,
                  'package_named_fcc_fonts': 0,
                  'package_fallback_lang_non_cjk': 1,
                  'package_fallback_lang_cjk': 0,
                  'package_fallback_lang_cjk_low_quality': 1,
                  'package_fallback_historic': 0,
                  'package_fallback_color_emoji': 0,
                  'package_fallback_emoji': 1,
                  'package_fallback_symbols': 1,

*  'minimal'  -- The smallest possible font package. It only includes Roboto's
                 Basic Latin characters. Everything else must be provided by the
                 system or downloaded from the web. This package is ~40.0KB.

                 Package category values:
                  'package_named_sans_serif': 0,
                  'package_named_serif': 0,
                  'package_named_fcc_fonts': 0,
                  'package_fallback_lang_non_cjk': 0,
                  'package_fallback_lang_cjk': 0,
                  'package_fallback_lang_cjk_low_quality': 0,
                  'package_fallback_historic': 0,
                  'package_fallback_color_emoji': 0,
                  'package_fallback_emoji': 0,
                  'package_fallback_symbols': 0,

NOTE: When bold is needed, but unavailable, it is typically synthesized,
      resulting in lower quality glyphs than those generated directly from a
      bold font. However, this does not occur with low quality CJK, which is
      not high enough quality to synthesize. Its glyphs always have a regular
      weight.


### Package Font Categories
Each package contains values for the following categories, which specify the
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

  *  'package_fallback_historic':
       Historic script fallback fonts.

  *  'package_fallback_color_emoji':
       Color emoji-related fallback fonts.

  *  'package_fallback_emoji':
       Uncolored emoji-related fallback fonts.

  *  'package_fallback_symbols':
       Symbol-related fallback fonts.


### Package Font Category Values
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


### Overriding Packages
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


### Override Package Mappings
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
  *  'cobalt_font_package_override_fallback_color_emoji' ==>
       'package_fallback_color_emoji'
  *  'cobalt_font_package_override_fallback_emoji' ==>
       'package_fallback_emoji'
  *  'cobalt_font_package_override_fallback_symbols' ==>
       'package_fallback_symbols'


## System Fonts

Beyond simply providing the ability to configure the fonts that are included
within its package, Cobalt supports the use of system fonts.

### Starboard System Font Paths

In order to enable system fonts, within SbSystemGetPath() porters must provide
paths for kSbSystemPathFontDirectory, which contains the system font files, and
for kSbSystemPathFontConfigurationDirectory, which contains the system font
configuration file. These directories may be the same.

### System Font Configuration File

In addition to providing the directory paths, porters must implement a fonts.xml
configuration file, which describes the system fonts that are available for use.

The syntax of the system font configuration file is identical to that of
Cobalt's font configuration file, which can be used as a reference. The system
font configuration file can include both named font families and fallback font
families. However, system fonts shouldn't duplicate any family, alias, or font
names contained within Cobalt's font configuration, unless the corresponding
font files are stripped from Cobalt's font package.

For example: if any of Cobalt's "sans-serif" font files are included in the
selected Cobalt font package, then the system font configuration cannot contain
a family named "sans-serif"; however, if Cobalt's "sans-serif" font files are
entirely stripped from the package, then the system may provide its own
"sans-serif" family without issues.

For more information on creating a font configuration, see
[Cobalt's configuration file](config/common/fonts.xml).
