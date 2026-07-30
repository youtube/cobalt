#!/usr/bin/python2
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
#
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
