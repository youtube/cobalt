## Description

This directory contains a few different set of font packages one can select
to be packaged with cobalt.

## How to use

To use this:

1.  Select one of the profiles below.
2.  Add a variable named `cobalt_font_package` in your platform's
`gyp_configuration.gypi` file.

Example:

    'variables': {
        'cobalt_font_package': '10megabytes',
    }



## Profiles
1. 'expanded' -- The largest package. It includes everything in the 'standard'
                 package, along with bold Noto CJK. It is recommended that
                 'local_font_cache_size_in_bytes' in base.gypi be increased to
                 24MB when using this package to account for the extra memory
                 required by bold Noto CJK. This package is ~46.2MB.
2. 'standard' -- The default package. It includes all non-CJK Noto fallback
                 fonts in both regular and bold weights, along with regular
                 weight Noto CJK (bold CJK is synthesized from it), and all FCC
                 fonts. This package is ~26.9MB.
3. 'limited_with_noto_jp' -- A significantly smaller package than 'standard'.
                 This package removes the bold non-CJK Noto fallback fonts (the
                 regular weight is still included and is used to synthesize
                 bold), removes the FCC fonts (which must be downloaded from the
                 web), and replaces high quality Noto CJK with lower quality
                 DroidSansFallback for both Chinese and Korean (Noto is still
                 provided for Japanese). Because DroidSansFallback cannot
                 synthesize bold, bold glyphs are unavailable in Chinese and
                 Korean. This package is ~10.9MB.
4. 'limited'  -- A smaller package than 'limited_with_noto_jp'. The packages are
                 identical with the exception that 'limited' does not include
                 the high quality Noto Japanese font; instead it relies on the
                 lower quality DroidSansFallback for all CJK characters. Because
                 DroidSansFallback cannot synthesize bold, bold glyphs are
                 unavailable in Chinese, Japanese, and Korean. This package is
                 ~7.7MB.
5. 'minimal'  -- The smallest possible font package. It only includes Roboto's
                 Basic Latin characters. Everything else must be downloaded from
                 the web. This package is ~16.4KB.

NOTE: When bold is needed, but unavailable, it is typically synthesized,
      resulting in lower quality glyphs than those generated directly from a
      bold font. However, this does not occur with DroidSansFallback, which is
      not high enough quality to synthesize. Its glyphs always have a regular
      weight.
