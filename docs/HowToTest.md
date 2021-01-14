Test suite
==========

## Prerequisites

On Ubuntu, install as many as possible TrueType and OpenType fonts:

    $ sudo apt-get install ttf-.*[^0]$

On Mac OS X, fonts installed on `/Library/Fonts/` and `/System/Library/Fonts/`
will be used.

## Running the tests

Simply run:

    $ make check

Which will run tests to verify that:

1. OTS does not reject these unmalicious fonts.
2. OTS can reject these malicious fonts.
3. The transcoded fonts OTS generates can be loaded by a system font renderer
   (FreeType, Core Text or GDI) and do not crash it.
4. Fuzzed fonts do not hang or crash OTS.

Command line tools
==================

We have some command line tools for tests.

## `ots-sanitize` - font validator/transcoder

### Description:
`ots-sanitize` is a program which validates and/or transcodes a font file using
the OTS library:

    transcoded_font = ValidateAndTranscode(original_font);
    if (validation_error)
      PrintErrorAndExit;
    OutputToStdout(transcoded_font);

### Usage:

    $ ./ots-sanitize ttf_or_otf_file [transcoded_file]

### Example:

    $ ./ots-sanitize sample.otf transcoded_sample.otf
    $ ./ots-sanitize malformed.ttf
    WARNING: bad range shift
    ERROR at src/ots.cc:670 (ProcessGeneric)
    Failed to sanitize file!

## `ots-idempotent` — font transcoder

### Description:
`ots-idempotent` is a program which validates and transcodes a font file using OTS.
This tool transcodes the original font twice and then verifies that the two
transcoded fonts are identical:

    t1 = ValidateAndTranscode(original_font);
    if (validation_error)
      PrintErrorAndExit;
    t2 = ValidateAndTranscode(t1);
    if (validation_error)
      PrintErrorAndExit;
    if (t1 != t2)
      PrintErrorAndExit;

This tool is basically for OTS developers.

### Usage:

    $ ./ots-idempotent ttf_or_otf_file

### Example:

    $ ./ots-idempotent sample.otf
    $ ./ots-idempotent malformed.ttf
    WARNING: bad range shift
    ERROR at src/ots.cc:670 (ProcessGeneric)
    Failed to sanitize file!

## `ots-validator-checker` — font validation checker

### Description:
`ots-validator-checker` is a program which is intended to validate malformed fonts.
If the program detects that the font is invalid, it prints “OK” and returns
with 0 (success). If it coulndn’t detect any errors, the program then opens the
transcoded font and renders some characters using FreeType:

    transcoded_font = ValidateAndTranscode(malicious_font);
    if (validation_error)
      Print("OK");
    OpenAndRenderSomeCharacters(transcoded_font);  # may cause SIGSEGV
    Print("OK");

If SEGV doesn't raise inside FreeType library, the program prints “OK” and
returns with 0 as well. You should run this tool under the catchsegv or
valgrind command so that you can easily verify that all transformed fonts don't
crash the library (see the example below).

### Usage:

    $ catchsegv ./validator_checker malicous_ttf_or_otf_file

### Example:

    $ for f in malformed/*.ttf ; do catchsegv ./ots-validator-checker "$f" ; done
    OK: the malicious font was filtered: malformed/1.ttf
    OK: the malicious font was filtered: malformed/2.ttf
    OK: FreeType2 didn't crash: malformed/3.ttf
    OK: the malicious font was filtered: malformed/4.ttf

## `ots-perf` — performance checker

### Description:
`ots-perf` is a program which validates and transcodes a font file N times using
OTS, then prints the elapsed time:

    for (N times)
      ValidateAndTranscode(original_font);
    Print(elapsed_time_in_us / N);

### Usage:

    $ ./ots-perf ttf_or_otf_file

### Example:

    $ ./ots-perf sample.ttf
    903 [us] sample.ttf (139332 bytes, 154 [byte/us])
    $ ./ots-perf sample-bold.otf
    291 [us] sample-bold.otf (150652 bytes, 517 [byte/us])

## `ots-side-by-side` — font quality checker

### Description:
`ots-side-by-side` is a program which renders some characters (ASCII, Latin-1, CJK)
using both original font and transcoded font and checks that the two rendering
results are exactly equal.

The following Unicode characters are used during the test:

    0x0020 - 0x007E  // Basic Latin
    0x00A1 - 0x017F  // Latin-1
    0x1100 - 0x11FF  // Hangul
    0x3040 - 0x309F  // Japanese HIRAGANA letters
    0x3130 - 0x318F  // Hangul
    0x4E00 - 0x4F00  // CJK Kanji/Hanja
    0xAC00 - 0xAD00  // Hangul

This tool uses FreeType library.

Note: This tool doesn't check kerning (GPOS/kern) nor font substitution
(GSUB). These should be tested in Layout tests if necessary.

### Usage:

    $ ./ots-side-by-side ttf_or_otf_file

### Example:

    $ ./ots-side-by-side linux/kochi-gothic.ttf  # no problem
    $ ./ots-side-by-side free/kredit1.ttf        # this is known issue of OTS.
    bitmap metrics doesn't match! (14, 57), (37, 45)
    EXPECTED:

      +#######*.
     +##########+
    .###+.#.   .#.
    *#*   #     #*
    ##.   #     ##
    ##    #     ##
    ##    #     ##
    ##    #.    ##
    ##.   #.   .##
    ##.   #.   .##
    *#+   *+   +#*
    *#+   *+   +#*
    *#+   *+   +#*
    *#+   *+   +#*
    *#+   *+   *#*
    *#+   ++   *#+
    +#*   +*   *#+
    +#*   +*   *#+
    +#*   +*   *#+
    +#*   +*   ##.
    +#*   +*   ##.
    .##   .#   ##
    .##   .#   ##
    .##   .#   ##
     ##    #   ##
     ##    #   ##
     ##    #  .##
     ##    #  .##
     ##   .#+ +#*
     ##  +######*
     ##.+#######*
     *##########*
     +##########+
      #########*
      .########
        +####+






      .*######*
     +##*.*#####
    .##+.#+    +#
    *#* ##      #+
    ##*###      ##
    ######      ##
    ##+.##+    +##
    ##  ##########
    ##  +#########
    ##   +########
    *#. .########*
    .#* #########.
     +##########+
      +*######*

    ACTUAL:

      .*##*+
     +##+.##*.
    .#* .##.+#*
    *#  ###   *#+
    #*######+  .*#+
    #########*.  +#*.
    ###########*   +#*
    *############+   *#+
    +##############.  .##.
     *##############*   +#*
      +###############+   *#+
        *###############+  .*#+
         .###############*.  +#*.
           +###############*   +#*
             *###############+   *#+
              .*###############+  .*#+
                +###############*.  +#*
                  +###############*   **
                    *###############+  #+
                     .###############* ##
                       +############+  ##
                         +########*   .##
                          .######.   +###
                         +#####+   .*#..#
                       +#####*    *###..#
                      *#####.   +#######*
                    +#####+   .*########.
                  +#####*    +#########*
                 *#####.   +##########+
               +#####+    *#########*.
             .#####*    +##########+
            *#####.   +##########*
          +#####+    *#########*.
        .#####*    +##########+
       *#####+   +##########*
     .#*++#+    *#########*.
    .#+  ##   +##########+
    ****###+.##########*
    ##################.
    ###+  *#########+
    ##   +########*
    *#+ *########.
     ##.#######+
     +#######*
       *###*.


    Glyph mismatch! (file: free/kredit1.ttf, U+0021, 100pt)!
