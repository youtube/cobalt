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

1.  `10megabytes`: Use this set of fonts if the target space allocated for fonts
is approximately 10 megabytes.  This directory contains DroidSansFallback, which
will render many Chinese, and Korean characters at lower quality.  The benefit
of using this font is space savings at the cost of reduced quality.
1.  `minimal`: Use this if minimizing space is a goal, and Cobalt should rely
on web fonts.
1.  `unlimited`: This font set is preferred, and default.  This will enable the
use the fonts with highest quality and coverage, without the network latency of
fetching fonts from the server.
