# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

import re

SPECIAL_PREFIXES = [
    'WebGL',
    'SVG',
]

MATCHING_EXPRESSION = '((?:[A-Z][a-z]+)|[0-9]D?$)'


class SmartTokenizer(object):
    """Detects special cases that are not easily discernible without additional
       knowledge, such as recognizing that in SVGSVGElement, the first two SVGs
       are separate tokens, but WebGL is one token."""

    def __init__(self, name):
        self.remaining = name

    def detect_special_prefix(self):
        for prefix in SPECIAL_PREFIXES:
            if self.remaining.startswith(prefix):
                result = self.remaining[:len(prefix)]
                self.remaining = self.remaining[len(prefix):]
                return result
        return None

    def tokenize(self):
        prefix = self.detect_special_prefix()
        return filter(None,
                      [prefix] + re.split(MATCHING_EXPRESSION, self.remaining))


class NameStyleConverter(object):
    """Converts names from camelCase and other styles to various other styles.
    """

    def __init__(self, name):
        self.tokens = self.tokenize(name)

    def tokenize(self, name):
        tokenizer = SmartTokenizer(name)
        return tokenizer.tokenize()

    def to_snake_case(self):
        """Snake case is the file and variable name style per Google C++ Style
           Guide:
           https://google.github.io/styleguide/cppguide.html#Variable_Names

           Also known as the hacker case.
           https://en.wikipedia.org/wiki/Snake_case
        """
        return '_'.join([token.lower() for token in self.tokens])

    def to_upper_camel_case(self):
        """Upper-camel case is the class and function name style per
           Google C++ Style Guide:
           https://google.github.io/styleguide/cppguide.html#Function_Names

           Also known as the PascalCase.
           https://en.wikipedia.org/wiki/Camel_case.
        """
        return ''.join([token[0].upper() + token[1:] for token in self.tokens])

    def to_macro_case(self):
        """Macro case is the macro name style per Google C++ Style Guide:
           https://google.github.io/styleguide/cppguide.html#Macro_Names
        """
        return '_'.join([token.upper() for token in self.tokens])

    def to_all_cases(self):
        return {
            'snake_case': self.to_snake_case(),
            'upper_camel_case': self.to_upper_camel_case(),
            'macro_case': self.to_macro_case(),
        }
