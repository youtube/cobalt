Cobalt font-related scripts
---------------------

This directory contains all of Cobalt's font-related scripts.

Instructions for generating a minimal subset of 'Roboto-Regular.ttf for devices
with limited space:

  1.  Download `fontforge` using apt. `sudo apt install python-fontforge`
  2.  `cd src/cobalt/content/fonts/scripts`
  3.  `python generate_roboto_regular_subsetted.py`
  4.  Move 'Roboto-Regular-Subsetted.ttf' into 'cobalt/content/fonts/all_fonts'
