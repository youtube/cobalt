# Generating a minimal font for devices with small space requirements

[GlyphIGo](https://github.com/pettarin/glyphIgo) was used to generate a subset
of the Roboto font

`cobalt/content/fonts` contains a script called `create_minimized_roboto.sh`
that can help recreate minimized font if needed

Steps:

1.  `cd src/cobalt/content/fonts`
1.  `./create_minimized_roboto.sh`
1.  Download `fontforge` using apt.  `sudo apt install fontforge`
1.  In `fontforge`, navigate the menu: `Encoding`->`Reencode`->`Glyph Order`.
Scroll to the top, find the first glyph.  By spec, this glyph is called
`.notdef`, and is used when this font is the default font and there glyph for a
character we're looking for is missing in the file.  Often this will be blank
after the last step, which can be undesired.
1.  Copy `.notdef` glyph from a different font.
    1.  Open a different font.
    1.  Find the `.notdef` glyph.
    1.  Select the glyph without opening it.
    1.  Navigate the menu: `Edit`->`Copy` from the font you want to copy from.
    1.  Switch focus to the minimized font.
    1.  Select `.notdef` glyph.
    1.  Navigate the menu: `Edit`->`Paste`.
1.  Export the font using the menu: `File`->`Generate Fonts...`, make sure that
the file name is correct.
1.  Fix any errors if found, or if you can.
