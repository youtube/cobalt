#!/usr/bin/python2
"""Generates a minimal subset of the characters from Roboto-Regular."""

import fontforge


def main():
  font = fontforge.open("Roboto-Regular.ttf")

  # Add the .notdef glyph
  font.selection.select(".notdef")
  # Add Basic Latin
  font.selection.select(("more", "ranges"), 0, 127)
  # Add No-Break Space
  font.selection.select(("more", "unicode"), 160)

  # Clear out everything from the font but the selections.
  font.selection.invert()
  font.clear()

  font.generate("Roboto-Regular-Subsetted.ttf")


if __name__ == "__main__":
  main()
