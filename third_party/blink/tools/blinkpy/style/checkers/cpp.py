# -*- coding: utf-8 -*-
#
# Copyright (C) 2009, 2010, 2012 Google Inc. All rights reserved.
# Copyright (C) 2009 Torch Mobile Inc.
# Copyright (C) 2009 Apple Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This is the modified version of Google's cpplint. The original code is
# https://github.com/google/styleguide/tree/gh-pages/cpplint
"""Support for check_blink_style.py."""

import math  # for log
import os
import os.path
import re
import six
import sre_compile
import unicodedata

from blinkpy.common.system.filesystem import FileSystem

from functools import total_ordering

xrange = six.moves.xrange

# Headers that we consider STL headers.
_STL_HEADERS = frozenset([
    'algobase.h',
    'algorithm',
    'alloc.h',
    'bitset',
    'deque',
    'exception',
    'function.h',
    'functional',
    'hash_map',
    'hash_map.h',
    'hash_set',
    'hash_set.h',
    'iterator',
    'list',
    'list.h',
    'map',
    'memory',
    'pair.h',
    'pthread_alloc',
    'queue',
    'set',
    'set.h',
    'sstream',
    'stack',
    'stl_alloc.h',
    'stl_relops.h',
    'type_traits.h',
    'utility',
    'vector',
    'vector.h',
])

# Non-STL C++ system headers.
_CPP_HEADERS = frozenset([
    'algo.h',
    'builtinbuf.h',
    'bvector.h',
    'cassert',
    'cctype',
    'cerrno',
    'cfloat',
    'ciso646',
    'climits',
    'clocale',
    'cmath',
    'complex',
    'complex.h',
    'csetjmp',
    'csignal',
    'cstdarg',
    'cstddef',
    'cstdio',
    'cstdlib',
    'cstring',
    'ctime',
    'cwchar',
    'cwctype',
    'defalloc.h',
    'deque.h',
    'editbuf.h',
    'exception',
    'fstream',
    'fstream.h',
    'hashtable.h',
    'heap.h',
    'indstream.h',
    'iomanip',
    'iomanip.h',
    'ios',
    'iosfwd',
    'iostream',
    'iostream.h',
    'istream.h',
    'iterator.h',
    'limits',
    'map.h',
    'multimap.h',
    'multiset.h',
    'numeric',
    'ostream.h',
    'parsestream.h',
    'pfstream.h',
    'PlotFile.h',
    'procbuf.h',
    'pthread_alloc.h',
    'rope',
    'rope.h',
    'ropeimpl.h',
    'SFile.h',
    'slist',
    'slist.h',
    'stack.h',
    'stdexcept',
    'stdiostream.h',
    'streambuf.h',
    'stream.h',
    'strfile.h',
    'string',
    'strstream',
    'strstream.h',
    'tempbuf.h',
    'tree.h',
    'typeinfo',
    'valarray',
])

# Assertion macros.  These are defined in base/logging.h and
# testing/base/gunit.h.  Note that the _M versions need to come first
# for substring matching to work.
_CHECK_MACROS = [
    'DCHECK',
    'CHECK',
    'EXPECT_TRUE_M',
    'EXPECT_TRUE',
    'ASSERT_TRUE_M',
    'ASSERT_TRUE',
    'EXPECT_FALSE_M',
    'EXPECT_FALSE',
    'ASSERT_FALSE_M',
    'ASSERT_FALSE',
]

# Replacement macros for CHECK/DCHECK/EXPECT_TRUE/EXPECT_FALSE
_CHECK_REPLACEMENT = dict([(m, {}) for m in _CHECK_MACROS])

for op, replacement in [('==', 'EQ'), ('!=', 'NE'), ('>=', 'GE'), ('>', 'GT'),
                        ('<=', 'LE'), ('<', 'LT')]:
    _CHECK_REPLACEMENT['DCHECK'][op] = 'DCHECK_%s' % replacement
    _CHECK_REPLACEMENT['CHECK'][op] = 'CHECK_%s' % replacement
    _CHECK_REPLACEMENT['EXPECT_TRUE'][op] = 'EXPECT_%s' % replacement
    _CHECK_REPLACEMENT['ASSERT_TRUE'][op] = 'ASSERT_%s' % replacement
    _CHECK_REPLACEMENT['EXPECT_TRUE_M'][op] = 'EXPECT_%s_M' % replacement
    _CHECK_REPLACEMENT['ASSERT_TRUE_M'][op] = 'ASSERT_%s_M' % replacement

for op, inv_replacement in [('==', 'NE'), ('!=', 'EQ'), ('>=', 'LT'),
                            ('>', 'LE'), ('<=', 'GT'), ('<', 'GE')]:
    _CHECK_REPLACEMENT['EXPECT_FALSE'][op] = 'EXPECT_%s' % inv_replacement
    _CHECK_REPLACEMENT['ASSERT_FALSE'][op] = 'ASSERT_%s' % inv_replacement
    _CHECK_REPLACEMENT['EXPECT_FALSE_M'][op] = 'EXPECT_%s_M' % inv_replacement
    _CHECK_REPLACEMENT['ASSERT_FALSE_M'][op] = 'ASSERT_%s_M' % inv_replacement

# The regexp compilation caching is inlined in all regexp functions for
# performance reasons; factoring it out into a separate function turns out
# to be noticeably expensive.
_regexp_compile_cache = {}


def match(pattern, s):
    """Matches the string with the pattern, caching the compiled regexp."""
    if not pattern in _regexp_compile_cache:
        _regexp_compile_cache[pattern] = sre_compile.compile(pattern)
    return _regexp_compile_cache[pattern].match(s)


def search(pattern, s):
    """Searches the string for the pattern, caching the compiled regexp."""
    if not pattern in _regexp_compile_cache:
        _regexp_compile_cache[pattern] = sre_compile.compile(pattern)
    return _regexp_compile_cache[pattern].search(s)


def sub(pattern, replacement, s):
    """Substitutes occurrences of a pattern, caching the compiled regexp."""
    if not pattern in _regexp_compile_cache:
        _regexp_compile_cache[pattern] = sre_compile.compile(pattern)
    return _regexp_compile_cache[pattern].sub(replacement, s)


def subn(pattern, replacement, s):
    """Substitutes occurrences of a pattern, caching the compiled regexp."""
    if not pattern in _regexp_compile_cache:
        _regexp_compile_cache[pattern] = sre_compile.compile(pattern)
    return _regexp_compile_cache[pattern].subn(replacement, s)


def iteratively_replace_matches_with_char(pattern, char_replacement, s):
    """Returns the string with replacement done.

    Every character in the match is replaced with char.
    Due to the iterative nature, pattern should not match char or
    there will be an infinite loop.

    Example:
      pattern = r'<[^>]>' # template parameters
      char_replacement =  '_'
      s =     'A<B<C, D>>'
      Returns 'A_________'

    Args:
      pattern: The regex to match.
      char_replacement: The character to put in place of every
                        character of the match.
      s: The string on which to do the replacements.

    Returns:
      True, if the given line is blank.
    """
    while True:
        matched = search(pattern, s)
        if not matched:
            return s
        start_match_index = matched.start(0)
        end_match_index = matched.end(0)
        match_length = end_match_index - start_match_index
        s = (s[:start_match_index] + char_replacement * match_length +
             s[end_match_index:])


def _find_in_lines(regex, lines, start_position, not_found_position):
    """Does a find starting at start position and going forward until
    a match is found.

    Returns the position where the regex started.
    """
    current_row = start_position.row

    # Start with the given row and trim off everything before what should be matched.
    current_line = lines[start_position.row][start_position.column:]
    starting_offset = start_position.column
    while True:
        found_match = search(regex, current_line)
        if found_match:
            return Position(current_row, starting_offset + found_match.start())

        # A match was not found so continue forward.
        current_row += 1
        starting_offset = 0
        if current_row >= len(lines):
            return not_found_position
        current_line = lines[current_row]


def _rfind_in_lines(regex, lines, start_position, not_found_position):
    """Does a reverse find starting at start position and going backwards until
    a match is found.

    Returns the position where the regex ended.
    """
    # Put the regex in a group and proceed it with a greedy expression that
    # matches anything to ensure that we get the last possible match in a line.
    last_in_line_regex = r'.*(' + regex + ')'
    current_row = start_position.row

    # Start with the given row and trim off everything past what may be matched.
    current_line = lines[start_position.row][:start_position.column]
    while True:
        found_match = match(last_in_line_regex, current_line)
        if found_match:
            return Position(current_row, found_match.end(1))

        # A match was not found so continue backward.
        current_row -= 1
        if current_row < 0:
            return not_found_position
        current_line = lines[current_row]


def up_to_unmatched_closing_paren(s):
    """Splits a string into two parts up to first unmatched ')'.

    Args:
      s: a string which is a substring of line after '('
      (e.g., "a == (b + c))").

    Returns:
      A pair of strings (prefix before first unmatched ')',
      remainder of s after first unmatched ')'), e.g.,
      up_to_unmatched_closing_paren("a == (b + c)) { ")
      returns "a == (b + c)", " {".
      Returns None, None if there is no unmatched ')'
    """
    i = 1
    for pos, c in enumerate(s):
        if c == '(':
            i += 1
        elif c == ')':
            i -= 1
            if i == 0:
                return s[:pos], s[pos + 1:]
    return None, None


class _IncludeState(dict):
    """Tracks line numbers for includes, and the order in which includes appear.

    As a dict, an _IncludeState object serves as a mapping between include
    filename and line number on which that file was included.
    """
    # self._section will move monotonically through this set. If it ever
    # needs to move backwards, check_next_include_order will raise an error.
    _INITIAL_SECTION = 0
    _PRIMARY_SECTION = 1
    _OTHER_SECTION = 2

    _SECTION_NAMES = {
        _INITIAL_SECTION: '... nothing.',
        _PRIMARY_SECTION: 'a header this file implements.',
        _OTHER_SECTION: 'other header.',
    }

    def __init__(self):
        dict.__init__(self)
        self._section = self._INITIAL_SECTION
        self._visited_primary_section = False
        self.header_types = dict()

    def visited_primary_section(self):
        return self._visited_primary_section


@total_ordering
class Position(object):
    """Holds the position of something."""

    def __init__(self, row, column):
        self.row = row
        self.column = column

    def __str__(self):
        return '(%s, %s)' % (self.row, self.column)

    def __cmp__(self, other):
        return self.row.__cmp__(other.row) or self.column.__cmp__(other.column)

    def __eq__(self, other):
        return (self.row, self.column) == (other.row, other.column)

    def __gt__(self, other):
        return (self.row, self.column) > (other.row, other.column)


class SingleLineView(object):
    """Converts multiple lines into a single line (with line breaks replaced by a
       space) to allow for easier searching.
    """

    def __init__(self, lines, start_position, end_position):
        """Create a SingleLineView instance.

        Args:
          lines: a list of multiple lines to combine into a single line.
          start_position: offset within lines of where to start the single line.
          end_position: just after where to end (like a slice operation).
        """
        # Get the rows of interest.
        trimmed_lines = lines[start_position.row:end_position.row + 1]

        # Remove the columns on the last line that aren't included.
        trimmed_lines[-1] = trimmed_lines[-1][:end_position.column]

        # Remove the columns on the first line that aren't included.
        trimmed_lines[0] = trimmed_lines[0][start_position.column:]

        # Create a single line with all of the parameters.
        self.single_line = ' '.join(trimmed_lines)
        self.single_line = _RE_PATTERN_CLEANSE_MULTIPLE_STRINGS.sub(
            '""', self.single_line)

        # Keep the row lengths, so we can calculate the original row number
        # given a column in the single line (adding 1 due to the space added
        # during the join).
        self._row_lengths = [len(line) + 1 for line in trimmed_lines]
        self._starting_row = start_position.row


class _FunctionState(object):
    """Tracks current function name and the number of lines in its body.

    Attributes:
      min_confidence: The minimum confidence level to use while checking style.
    """

    _NORMAL_TRIGGER = 250  # for --v=0, 500 for --v=1, etc.
    _TEST_TRIGGER = 400  # about 50% more than _NORMAL_TRIGGER.

    def __init__(self, min_confidence):
        self.min_confidence = min_confidence
        self.current_function = ''
        self.in_a_function = False
        self.lines_in_function = 0
        # Make sure these will not be mistaken for real positions (even when a
        # small amount is added to them).
        self.body_start_position = Position(-1000, 0)
        self.end_position = Position(-1000, 0)

    def begin(self, function_name, function_name_start_position,
              body_start_position, end_position, parameter_start_position,
              parameter_end_position, clean_lines):
        """Start analyzing function body.

        Args:
            function_name: The name of the function being tracked.
            function_name_start_position: Position in elided where the function name starts.
            body_start_position: Position in elided of the { or the ; for a prototype.
            end_position: Position in elided just after the final } (or ; is.
            parameter_start_position: Position in elided of the '(' for the parameters.
            parameter_end_position: Position in elided just after the ')' for the parameters.
            clean_lines: A CleansedLines instance containing the file.
        """
        self.in_a_function = True
        self.lines_in_function = -1  # Don't count the open brace line.
        self.current_function = function_name
        self.function_name_start_position = function_name_start_position
        self.body_start_position = body_start_position
        self.end_position = end_position
        self.is_declaration = clean_lines.elided[body_start_position.row][
            body_start_position.column] == ';'
        self.parameter_start_position = parameter_start_position
        self.parameter_end_position = parameter_end_position
        self.is_pure = False
        if self.is_declaration:
            characters_after_parameters = SingleLineView(
                clean_lines.elided, parameter_end_position,
                body_start_position).single_line
            self.is_pure = bool(
                match(r'\s*=\s*0\s*', characters_after_parameters))
        self._clean_lines = clean_lines

    def count(self, line_number):
        """Count line in current function body."""
        if self.in_a_function and line_number >= self.body_start_position.row:
            self.lines_in_function += 1

    def check(self, error, line_number):
        """Report if too many lines in function body.

        Args:
          error: The function to call with any errors found.
          line_number: The number of the line to check.
        """
        if match(r'T(EST|est)', self.current_function):
            base_trigger = self._TEST_TRIGGER
        else:
            base_trigger = self._NORMAL_TRIGGER
        trigger = base_trigger * 2**self.min_confidence

        if self.lines_in_function > trigger:
            error_level = int(
                math.log(self.lines_in_function / base_trigger, 2))
            # 50 => 0, 100 => 1, 200 => 2, 400 => 3, 800 => 4, 1600 => 5, ...
            if error_level > 5:
                error_level = 5
            error(
                line_number, 'readability/fn_size', error_level,
                'Small and focused functions are preferred:'
                ' %s has %d non-comment lines'
                ' (error triggered by exceeding %d lines).' %
                (self.current_function, self.lines_in_function, trigger))

    def end(self):
        """Stop analyzing function body."""
        self.in_a_function = False


class _IncludeError(Exception):
    """Indicates a problem with the include order in a file."""


class FileInfo:
    """Provides utility functions for filenames.

    FileInfo provides easy access to the components of a file's path
    relative to the project root.
    """

    def __init__(self, filename):
        self._filename = filename

    def full_name(self):
        """Make Windows paths like Unix."""
        return os.path.abspath(self._filename).replace('\\', '/')

    def repository_name(self):
        """Full name after removing the local path to the repository.

        If we have a real absolute path name here we can try to do something smart:
        detecting the root of the checkout and truncating /path/to/checkout from
        the name so that we get header guards that don't include things like
        "C:\Documents and Settings\..." or "/home/username/..." in them and thus
        people on different computers who have checked the source out to different
        locations won't see bogus errors.
        """
        fullname = self.full_name()

        if os.path.exists(fullname):
            project_dir = os.path.dirname(fullname)

            # Try to find a git top level directory by searching up from the current path.
            root_dir = os.path.dirname(fullname)
            while (root_dir != os.path.dirname(root_dir)
                   and not os.path.exists(os.path.join(root_dir, '.git'))):
                root_dir = os.path.dirname(root_dir)
                if os.path.exists(os.path.join(root_dir, '.git')):
                    prefix = os.path.commonprefix([root_dir, project_dir])
                    return fullname[len(prefix) + 1:]

        # Don't know what to do; header guard warnings may be wrong...
        return fullname

    def split(self):
        """Splits the file into the directory, basename, and extension.

        For 'chrome/browser/browser.cpp', Split() would
        return ('chrome/browser', 'browser', '.cpp')

        Returns:
          A tuple of (directory, basename, extension).
        """

        googlename = self.repository_name()
        project, rest = os.path.split(googlename)
        return (project, ) + os.path.splitext(rest)

    def base_name(self):
        """File base name - text after the final slash, before the final period."""
        return self.split()[1]

    def extension(self):
        """File extension - text following the final period."""
        return self.split()[2]

    def no_extension(self):
        """File has no source file extension."""
        return '/'.join(self.split()[0:2])

    def is_source(self):
        """File has a source file extension."""
        return self.extension()[1:] in ('c', 'cc', 'cpp', 'cxx')


# Matches standard C++ escape esequences per 2.13.2.3 of the C++ standard.
_RE_PATTERN_CLEANSE_LINE_ESCAPES = re.compile(
    r'\\([abfnrtv?"\\\']|\d+|x[0-9a-fA-F]+)')
# Matches strings.  Escape codes should already be removed by ESCAPES.
_RE_PATTERN_CLEANSE_LINE_DOUBLE_QUOTES = re.compile(r'"[^"]*"')
# Matches characters.  Escape codes should already be removed by ESCAPES.
_RE_PATTERN_CLEANSE_LINE_SINGLE_QUOTES = re.compile(r"'.'")
# Matches multiple strings (after the above cleanses) which can be concatenated.
_RE_PATTERN_CLEANSE_MULTIPLE_STRINGS = re.compile(r'"("\s*")+"')

# Matches multi-line C++ comments.
# This RE is a little bit more complicated than one might expect, because we
# have to take care of space removals tools so we can handle comments inside
# statements better.
# The current rule is: We only clear spaces from both sides when we're at the
# end of the line. Otherwise, we try to remove spaces from the right side,
# if this doesn't work we try on left side but only if there's a non-character
# on the right.
_RE_PATTERN_CLEANSE_LINE_C_COMMENTS = re.compile(
    r"""(\s*/\*.*\*/\s*$|
            /\*.*\*/\s+|
         \s+/\*.*\*/(?=\W)|
            /\*.*\*/)""", re.VERBOSE)


def is_cpp_string(line):
    """Does line terminate so, that the next symbol is in string constant.

    This function does not consider single-line nor multi-line comments.

    Args:
      line: is a partial line of code starting from the 0..n.

    Returns:
      True, if next character appended to 'line' is inside a
      string constant.
    """

    line = line.replace(r'\\', 'XX')  # after this, \\" does not match to \"
    return (
        (line.count('"') - line.count(r'\"') - line.count("'\"'")) & 1) == 1


def cleanse_raw_strings(raw_lines):
    """Removes C++11 raw strings from lines.

      Before:
        static const char kData[] = R"(
            multi-line string
            )";

      After:
        static const char kData[] = ""
            (replaced by blank line)
            "";

    Args:
      raw_lines: list of raw lines.

    Returns:
      list of lines with C++11 raw strings replaced by empty strings.
    """

    delimiter = None
    lines_without_raw_strings = []
    for line in raw_lines:
        if delimiter:
            # Inside a raw string, look for the end
            end = line.find(delimiter)
            if end >= 0:
                # Found the end of the string, match leading space for this
                # line and resume copying the original lines, and also insert
                # a "" on the last line.
                leading_space = match(r'^(\s*)\S', line)
                line = (leading_space.group(1) + '""' +
                        line[end + len(delimiter):])
                delimiter = None
            else:
                # Haven't found the end yet, append a blank line.
                line = '""'

        # Look for beginning of a raw string, and replace them with
        # empty strings.  This is done in a loop to handle multiple raw
        # strings on the same line.
        while delimiter is None:
            # Look for beginning of a raw string.
            # See 2.14.15 [lex.string] for syntax.
            #
            # Once we have matched a raw string, we check the prefix of the
            # line to make sure that the line is not part of a single line
            # comment.  It's done this way because we remove raw strings
            # before removing comments as opposed to removing comments
            # before removing raw strings.  This is because there are some
            # cpplint checks that requires the comments to be preserved, but
            # we don't want to check comments that are inside raw strings.
            matched = match(r'^(.*?)\b(?:R|u8R|uR|UR|LR)"([^\s\\()]*)\((.*)$',
                            line)
            if matched and not match(
                    r'^([^\'"]|\'(\\.|[^\'])*\'|"(\\.|[^"])*")*//',
                    matched.group(1)):
                delimiter = ')' + matched.group(2) + '"'

                end = matched.group(3).find(delimiter)
                if end >= 0:
                    # Raw string ended on same line
                    line = (matched.group(1) + '""' +
                            matched.group(3)[end + len(delimiter):])
                    delimiter = None
                else:
                    # Start of a multi-line raw string
                    line = matched.group(1) + '""'
            else:
                break

        lines_without_raw_strings.append(line)

    # TODO(unknown): if delimiter is not None here, we might want to
    # emit a warning for unterminated string.
    return lines_without_raw_strings


def find_next_multi_line_comment_start(lines, line_index):
    """Find the beginning marker for a multiline comment."""
    while line_index < len(lines):
        if lines[line_index].strip().startswith('/*'):
            # Only return this marker if the comment goes beyond this line
            if lines[line_index].strip().find('*/', 2) < 0:
                return line_index
        line_index += 1
    return len(lines)


def find_next_multi_line_comment_end(lines, line_index):
    """We are inside a comment, find the end marker."""
    while line_index < len(lines):
        if lines[line_index].strip().endswith('*/'):
            return line_index
        line_index += 1
    return len(lines)


def remove_multi_line_comments_from_range(lines, begin, end):
    """Clears a range of lines for multi-line comments."""
    # Having // dummy comments makes the lines non-empty, so we will not get
    # unnecessary blank line warnings later in the code.
    for i in range(begin, end):
        lines[i] = '// dummy'


def remove_multi_line_comments(lines, error):
    """Removes multiline (c-style) comments from lines."""
    line_index = 0
    while line_index < len(lines):
        line_index_begin = find_next_multi_line_comment_start(
            lines, line_index)
        if line_index_begin >= len(lines):
            return
        line_index_end = find_next_multi_line_comment_end(
            lines, line_index_begin)
        if line_index_end >= len(lines):
            return
        remove_multi_line_comments_from_range(lines, line_index_begin,
                                              line_index_end + 1)
        line_index = line_index_end + 1


def cleanse_comments(line):
    """Removes //-comments and single-line C-style /* */ comments.

    Args:
      line: A line of C++ source.

    Returns:
      The line with single-line comments removed.
    """
    comment_position = line.find('//')
    if comment_position != -1 and not is_cpp_string(line[:comment_position]):
        line = line[:comment_position]
    # get rid of /* ... */
    return _RE_PATTERN_CLEANSE_LINE_C_COMMENTS.sub('', line)


class CleansedLines(object):
    """Holds 4 copies of all lines with different preprocessing applied to them.

    1) elided member contains lines without strings and comments.
    2) lines member contains lines without comments.
    3) raw_lines member contains all the lines without processing.
    4) lines_without_raw_strings member is same as raw_lines, but with C++11 raw
       strings removed.
    All these members are of <type 'list'>, and of the same length.
    """

    def __init__(self, lines):
        self.elided = []
        self.lines = []
        self.raw_lines = lines
        self._num_lines = len(lines)
        self.lines_without_raw_strings = cleanse_raw_strings(lines)
        for line_number in range(len(self.lines_without_raw_strings)):
            self.lines.append(
                cleanse_comments(self.lines_without_raw_strings[line_number]))
            elided = self.collapse_strings(
                self.lines_without_raw_strings[line_number])
            self.elided.append(cleanse_comments(elided))

    def num_lines(self):
        """Returns the number of lines represented."""
        return self._num_lines

    @staticmethod
    def collapse_strings(elided):
        """Collapses strings and chars on a line to simple "" or '' blocks.

        We nix strings first so we're not fooled by text like '"http://"'

        Args:
          elided: The line being processed.

        Returns:
          The line with collapsed strings.
        """
        if not _RE_PATTERN_INCLUDE.match(elided):
            # Remove escaped characters first to make quote/single quote collapsing
            # basic.  Things that look like escaped characters shouldn't occur
            # outside of strings and chars.
            elided = _RE_PATTERN_CLEANSE_LINE_ESCAPES.sub('', elided)
            elided = _RE_PATTERN_CLEANSE_LINE_SINGLE_QUOTES.sub("''", elided)
            elided = _RE_PATTERN_CLEANSE_LINE_DOUBLE_QUOTES.sub('""', elided)
            elided = _RE_PATTERN_CLEANSE_MULTIPLE_STRINGS.sub('""', elided)
        return elided


def close_expression(elided, position):
    """If input points to ( or { or [, finds the position that closes it.

    If elided[position.row][position.column] points to a '(' or '{' or '[',
    finds the line_number/pos that correspond to the closing of the expression.

     Args:
       elided: A CleansedLines.elided instance containing the file.
       position: The position of the opening item.

     Returns:
      The Position *past* the closing brace, or Position(len(elided), -1)
      if we never find a close. Note we ignore strings and comments when matching.
    """
    line = elided[position.row]
    start_character = line[position.column]
    if start_character == '(':
        enclosing_character_regex = r'[\(\)]'
    elif start_character == '[':
        enclosing_character_regex = r'[\[\]]'
    elif start_character == '{':
        enclosing_character_regex = r'[\{\}]'
    else:
        return Position(len(elided), -1)

    current_column = position.column + 1
    line_number = position.row
    net_open = 1
    for line in elided[position.row:]:
        line = line[current_column:]

        # Search the current line for opening and closing characters.
        while True:
            next_enclosing_character = search(enclosing_character_regex, line)
            # No more on this line.
            if not next_enclosing_character:
                break
            current_column += next_enclosing_character.end(0)
            line = line[next_enclosing_character.end(0):]
            if next_enclosing_character.group(0) == start_character:
                net_open += 1
            else:
                net_open -= 1
                if not net_open:
                    return Position(line_number, current_column)

        # Proceed to the next line.
        line_number += 1
        current_column = 0

    # The given item was not closed.
    return Position(len(elided), -1)


def check_for_copyright(lines, error):
    """Logs an error if no Copyright message appears at the top of the file."""

    # We'll say it should occur by line 10. Don't forget there's a
    # dummy line at the front.
    for line in xrange(1, min(len(lines), 11)):
        if re.search(r'Copyright', lines[line], re.I):
            break
    else:  # means no copyright line was found
        error(
            0, 'legal/copyright', 5, 'No copyright message found.  '
            'You should have a line: "Copyright [year] <Copyright Owner>"')


def get_header_guard_cpp_variable(filename):
    """Returns the CPP variable that should be used as a header guard in Chromium-style.

    Args:
      filename: The name of a C++ header file.

    Returns:
      The CPP variable that should be used as a header guard in the
      named file in Chromium-style.
    """

    # Restores original filename in case that style checker is invoked from Emacs's
    # flymake.
    filename = re.sub(r'_flymake\.h$', '.h', filename)

    return sub(r'[-.\s\/]', '_', filename).upper() + '_'


def check_for_header_guard(filename, clean_lines, error):
    """Checks that the file contains a header guard.

    Logs an error if no #ifndef header guard is present.  For other
    headers, checks that the full pathname is used.

    Args:
      filename: The name of the C++ header file.
      lines: An array of strings, each representing a line of the file.
      error: The function to call with any errors found.
    """
    raw_lines = clean_lines.lines_without_raw_strings

    cpp_var = get_header_guard_cpp_variable(filename)

    ifndef = None
    ifndef_line_number = 0
    define = None
    for line_number, line in enumerate(raw_lines):
        line_split = line.split()
        if len(line_split) >= 2:
            # find the first occurrence of #ifndef and #define, save arg
            if not ifndef and line_split[0] == '#ifndef':
                # set ifndef to the header guard presented on the #ifndef line.
                ifndef = line_split[1]
                ifndef_line_number = line_number
            if not define and line_split[0] == '#define':
                define = line_split[1]
            if define and ifndef:
                break

    if not ifndef or not define or ifndef != define:
        error(
            0, 'build/header_guard', 5,
            'No #ifndef header guard found, suggested CPP variable is: %s' %
            cpp_var)
        return

    # The guard should be File_h or, for Chromium style, BLINK_PATH_TO_FILE_H_.
    if ifndef != cpp_var:
        error(ifndef_line_number, 'build/header_guard', 5,
              '#ifndef header guard has wrong style, please use: %s' % cpp_var)


def check_for_unicode_replacement_characters(lines, error):
    """Logs an error for each line containing Unicode replacement characters.

    These indicate that either the file contained invalid UTF-8 (likely)
    or Unicode replacement characters (which it shouldn't).  Note that
    it's possible for this to throw off line numbering if the invalid
    UTF-8 occurred adjacent to a newline.

    Args:
      lines: An array of strings, each representing a line of the file.
      error: The function to call with any errors found.
    """
    for line_number, line in enumerate(lines):
        if u'\ufffd' in line:
            error(
                line_number, 'readability/utf8', 5,
                'Line contains invalid UTF-8 (or Unicode replacement character).'
            )


def check_for_new_line_at_eof(lines, error):
    """Logs an error if there is no newline char at the end of the file.

    Args:
      lines: An array of strings, each representing a line of the file.
      error: The function to call with any errors found.
    """

    # The array lines() was created by adding two newlines to the
    # original file (go figure), then splitting on \n.
    # To verify that the file ends in \n, we just have to make sure the
    # last-but-two element of lines() exists and is empty.
    if len(lines) < 3 or lines[-2]:
        error(
            len(lines) - 2, 'whitespace/ending_newline', 5,
            'Could not find a newline character at the end of the file.')


_THREADING_LIST = (
    ('asctime(', 'asctime_r('),
    ('ctime(', 'ctime_r('),
    ('getgrgid(', 'getgrgid_r('),
    ('getgrnam(', 'getgrnam_r('),
    ('getlogin(', 'getlogin_r('),
    ('getpwnam(', 'getpwnam_r('),
    ('getpwuid(', 'getpwuid_r('),
    ('gmtime(', 'gmtime_r('),
    ('localtime(', 'localtime_r('),
    ('rand(', 'rand_r('),
    ('readdir(', 'readdir_r('),
    ('strtok(', 'strtok_r('),
    ('ttyname(', 'ttyname_r('),
)


def check_posix_threading(clean_lines, line_number, error):
    """Checks for calls to thread-unsafe functions.

    Much code has been originally written without consideration of
    multi-threading. Also, engineers are relying on their old experience;
    they have learned posix before threading extensions were added. These
    tests guide the engineers to use thread-safe functions (when using
    posix directly).

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """
    line = clean_lines.elided[line_number]
    for single_thread_function, multithread_safe_function in _THREADING_LIST:
        index = line.find(single_thread_function)
        # Comparisons made explicit for clarity
        if index >= 0 and (index == 0 or
                           (not line[index - 1].isalnum()
                            and line[index - 1] not in ('_', '.', '>'))):
            error(
                line_number, 'runtime/threadsafe_fn', 2, 'Consider using ' +
                multithread_safe_function + '...) instead of ' +
                single_thread_function + '...) for improved thread safety.')


# Matches invalid increment: *count++, which moves pointer instead of
# incrementing a value.
_RE_PATTERN_INVALID_INCREMENT = re.compile(r'^\s*\*\w+(\+\+|--);')


def check_invalid_increment(clean_lines, line_number, error):
    """Checks for invalid increment *count++.

    For example following function:
    void increment_counter(int* count) {
        *count++;
    }
    is invalid, because it effectively does count++, moving pointer, and should
    be replaced with ++*count, (*count)++ or *count += 1.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """
    line = clean_lines.elided[line_number]
    if _RE_PATTERN_INVALID_INCREMENT.match(line):
        error(
            line_number, 'runtime/invalid_increment', 5,
            'Changing pointer instead of value (or unused value of operator*).'
        )


class _ClassInfo(object):
    """Stores information about a class."""

    def __init__(self, name, line_number):
        self.name = name
        self.line_number = line_number
        self.seen_open_brace = False
        self.is_derived = False
        self.virtual_method_line_number = None
        self.has_virtual_destructor = False
        self.brace_depth = 0
        self.unsigned_bitfields = []
        self.bool_bitfields = []


class _ClassState(object):
    """Holds the current state of the parse relating to class declarations.

    It maintains a stack of _ClassInfos representing the parser's guess
    as to the current nesting of class declarations. The innermost class
    is at the top (back) of the stack. Typically, the stack will either
    be empty or have exactly one entry.
    """

    def __init__(self):
        self.classinfo_stack = []


class _FileState(object):
    def __init__(self, clean_lines, file_extension):
        self._clean_lines = clean_lines
        if file_extension in ['m', 'mm']:
            self._is_objective_c = True
            self._is_c = False
        elif file_extension == 'h':
            # In the case of header files, it is unknown if the file
            # is c / objective c or not, so set this value to None and then
            # if it is requested, use heuristics to guess the value.
            self._is_objective_c = None
            self._is_c = None
        elif file_extension == 'c':
            self._is_c = True
            self._is_objective_c = False
        else:
            self._is_objective_c = False
            self._is_c = False

    def is_objective_c(self):
        if self._is_objective_c is None:
            for line in self._clean_lines.elided:
                # Starting with @ or #import seem like the best indications
                # that we have an Objective C file.
                if line.startswith('@') or line.startswith('#import'):
                    self._is_objective_c = True
                    break
            else:
                self._is_objective_c = False
        return self._is_objective_c

    def is_c(self):
        if self._is_c is None:
            for line in self._clean_lines.lines:
                # if extern "C" is found, then it is a good indication
                # that we have a C header file.
                if line.startswith('extern "C"'):
                    self._is_c = True
                    break
            else:
                self._is_c = False
        return self._is_c

    def is_c_or_objective_c(self):
        """Return whether the file extension corresponds to C or Objective-C."""
        return self.is_c() or self.is_objective_c()


def check_for_non_standard_constructs(clean_lines, line_number, class_state,
                                      error):
    """Logs an error if we see certain non-ANSI constructs ignored by gcc-2.

    Complain about several constructs which gcc-2 accepts, but which are
    not standard C++.  Warning about these in lint is one way to ease the
    transition to new compilers.
    - put storage class first (e.g. "static const" instead of "const static").
    - "%lld" instead of %qd" in printf-type functions.
    - "%1$d" is non-standard in printf-type functions.
    - "\%" is an undefined character escape sequence.
    - text after #endif is not allowed.
    - invalid inner-style forward declaration.
    - >? and <? operators, and their >?= and <?= cousins.
    - classes with virtual methods need virtual destructors (compiler warning
        available, but not turned on yet.)

    Additionally, check for constructor/destructor style violations as it
    is very convenient to do so while checking for gcc-2 compliance.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      class_state: A _ClassState instance which maintains information about
                   the current stack of nested class declarations being parsed.
      error: A callable to which errors are reported, which takes parameters:
             line number, error level, and message
    """
    # Work with both comments and strings removed.
    line = clean_lines.elided[line_number]

    # Track class entry and exit, and attempt to find cases within the
    # class declaration that don't meet the C++ style
    # guidelines. Tracking is very dependent on the code matching Google
    # style guidelines, but it seems to perform well enough in testing
    # to be a worthwhile addition to the checks.
    classinfo_stack = class_state.classinfo_stack
    # Look for a class declaration
    class_decl_match = match(
        r'\s*(template\s*<[\w\s<>,:]*>\s*)?(class|struct)\s+(\w+(::\w+)*)',
        line)
    if class_decl_match:
        classinfo_stack.append(
            _ClassInfo(class_decl_match.group(3), line_number))

    # Everything else in this function uses the top of the stack if it's
    # not empty.
    if not classinfo_stack:
        return

    classinfo = classinfo_stack[-1]

    # If the opening brace hasn't been seen look for it and also
    # parent class declarations.
    if not classinfo.seen_open_brace:
        # If the line has a ';' in it, assume it's a forward declaration or
        # a single-line class declaration, which we won't process.
        if ';' in line:
            classinfo_stack.pop()
            return
        classinfo.seen_open_brace = ('{' in line)
        # Look for a bare ':'
        if search('(^|[^:]):($|[^:])', line):
            classinfo.is_derived = True
        if not classinfo.seen_open_brace:
            return  # Everything else in this function is for after open brace

    # The class may have been declared with namespace or classname qualifiers.
    # The constructor and destructor will not have those qualifiers.
    base_classname = classinfo.name.split('::')[-1]

    # Look for single-argument constructors that aren't marked explicit.
    # Technically a valid construct, but against style.
    args = match(
        r'(?<!explicit)\s+%s\s*\(([^,()]+)\)' % re.escape(base_classname),
        line)
    if (args and args.group(1) != 'void'
            and not match(r'(const\s+)?%s\s*&' % re.escape(base_classname),
                          args.group(1).strip())):
        error(line_number, 'runtime/explicit', 5,
              'Single-argument constructors should be marked explicit.')

    # Look for methods declared virtual.
    if search(r'\bvirtual\b', line):
        classinfo.virtual_method_line_number = line_number
        # Only look for a destructor declaration on the same line. It would
        # be extremely unlikely for the destructor declaration to occupy
        # more than one line.
        if search(r'~%s\s*\(' % base_classname, line):
            classinfo.has_virtual_destructor = True

    # Look for class end.
    brace_depth = classinfo.brace_depth
    brace_depth = brace_depth + line.count('{') - line.count('}')
    if brace_depth <= 0:
        classinfo = classinfo_stack.pop()
        # Try to detect missing virtual destructor declarations.
        # For now, only warn if a non-derived class with virtual methods lacks
        # a virtual destructor. This is to make it less likely that people will
        # declare derived virtual destructors without declaring the base
        # destructor virtual.
        if ((classinfo.virtual_method_line_number is not None)
                and (not classinfo.has_virtual_destructor)
                and (not classinfo.is_derived)):  # Only warn for base classes
            error(
                classinfo.line_number, 'runtime/virtual', 4,
                'The class %s probably needs a virtual destructor due to '
                'having virtual method(s), one declared at line %d.' %
                (classinfo.name, classinfo.virtual_method_line_number))
    else:
        classinfo.brace_depth = brace_depth

    well_typed_bitfield = False
    # Look for bool <name> : 1 declarations.
    args = search(r'\bbool\s+(\S*)\s*:\s*\d+\s*;', line)
    if args:
        classinfo.bool_bitfields.append(
            '%d: %s' % (line_number, args.group(1)))
        well_typed_bitfield = True

    # Look for unsigned <name> : n declarations.
    args = search(r'\bunsigned\s+(?:int\s+)?(\S+)\s*:\s*\d+\s*;', line)
    if args:
        classinfo.unsigned_bitfields.append(
            '%d: %s' % (line_number, args.group(1)))
        well_typed_bitfield = True

    # Look for other bitfield declarations. We don't care about those in
    # size-matching structs.
    if not (well_typed_bitfield or classinfo.name.startswith('SameSizeAs')
            or classinfo.name.startswith('Expected')):
        args = match(r'\s*(\S+)\s+(\S+)\s*:\s*\d+\s*;', line)
        if args:
            error(
                line_number, 'runtime/bitfields', 4,
                'Member %s of class %s defined as a bitfield of type %s. '
                'Please declare all bitfields as unsigned.' %
                (args.group(2), classinfo.name, args.group(1)))


def is_blank_line(line):
    """Returns true if the given line is blank.

    We consider a line to be blank if the line is empty or consists of
    only white spaces.

    Args:
      line: A line of a string.

    Returns:
      True, if the given line is blank.
    """
    return not line or line.isspace()


def detect_functions(clean_lines, line_number, function_state, error):
    """Finds where functions start and end.

    Uses a simplistic algorithm assuming other style guidelines
    (especially spacing) are followed.
    Trivial bodies are unchecked, so constructors with huge initializer lists
    may be missed.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      function_state: Current function name and lines in body so far.
      error: The function to call with any errors found.
    """
    # Are we now past the end of a function?
    if function_state.end_position.row + 1 == line_number:
        function_state.end()

    # If we're in a function, don't try to detect a new one.
    if function_state.in_a_function:
        return

    lines = clean_lines.lines
    line = lines[line_number]
    raw = clean_lines.raw_lines
    raw_line = raw[line_number]

    # Lines ending with a \ indicate a macro. Don't try to check them.
    if raw_line.endswith('\\'):
        return

    regexp = r'\s*(\w(\w|::|\*|\&|\s|<|>|,|~|(operator\s*(/|-|=|!|\+)+))*)\('  # decls * & space::name( ...
    match_result = match(regexp, line)
    if not match_result:
        return

    # If the name is all caps and underscores, figure it's a macro and
    # ignore it, unless it's TEST or TEST_F.
    function_name = match_result.group(1).split()[-1]
    if (function_name != 'TEST' and function_name != 'TEST_F'
            and match(r'[A-Z_]+$', function_name)):
        return

    joined_line = ''
    for start_line_number in xrange(line_number, clean_lines.num_lines()):
        start_line = clean_lines.elided[start_line_number]
        joined_line += ' ' + start_line.lstrip()
        body_match = search(r'{|;', start_line)
        if body_match:
            body_start_position = Position(start_line_number,
                                           body_match.start(0))

            # Replace template constructs with _ so that no spaces remain in the function name,
            # while keeping the column numbers of other characters the same as "line".
            line_with_no_templates = iteratively_replace_matches_with_char(
                r'<[^<>]*>', '_', line)
            match_function = search(
                r'((\w|:|<|>|,|~|(operator\s*(/|-|=|!|\+)+))*)\(',
                line_with_no_templates)
            if not match_function:
                return  # The '(' must have been inside of a template.

            # Use the column numbers from the modified line to find the
            # function name in the original line.
            function = line[match_function.start(1):match_function.end(1)]
            function_name_start_position = Position(line_number,
                                                    match_function.start(1))

            if match(r'TEST', function):  # Handle TEST... macros
                parameter_regexp = search(r'(\(.*\))', joined_line)
                if parameter_regexp:  # Ignore bad syntax
                    function += parameter_regexp.group(1)
            else:
                function += '()'

            parameter_start_position = Position(line_number,
                                                match_function.end(1))
            parameter_end_position = close_expression(
                clean_lines.elided, parameter_start_position)
            if parameter_end_position.row == len(clean_lines.elided):
                # No end was found.
                return

            if start_line[body_start_position.column] == ';':
                end_position = Position(body_start_position.row,
                                        body_start_position.column + 1)
            else:
                end_position = close_expression(clean_lines.elided,
                                                body_start_position)

            # Check for nonsensical positions. (This happens in test cases which check code snippets.)
            if parameter_end_position > body_start_position:
                return

            function_state.begin(function, function_name_start_position,
                                 body_start_position, end_position,
                                 parameter_start_position,
                                 parameter_end_position, clean_lines)
            return

    # No body for the function (or evidence of a non-function) was found.
    error(line_number, 'readability/fn_size', 5,
          'Lint failed to find start of function body.')


def check_for_function_lengths(clean_lines, line_number, function_state,
                               error):
    """Reports for long function bodies.

    For an overview why this is done, see:
    https://google.github.io/styleguide/cppguide.html#Write_Short_Functions

    Blank/comment lines are not counted so as to avoid encouraging the removal
    of vertical space and comments just to get through a lint check.
    NOLINT *on the last line of a function* disables this check.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      function_state: Current function name and lines in body so far.
      error: The function to call with any errors found.
    """
    lines = clean_lines.lines
    line = lines[line_number]
    raw = clean_lines.raw_lines
    raw_line = raw[line_number]

    if function_state.end_position.row == line_number:  # last line
        if not search(r'\bNOLINT\b', raw_line):
            function_state.check(error, line_number)
    elif not match(r'^\s*$', line):
        function_state.count(line_number)  # Count non-blank/non-comment lines.


def check_pass_ptr_usage(clean_lines, line_number, function_state, error):
    """Check for proper usage of Pass*Ptr.

    Currently this is limited to detecting declarations of Pass*Ptr
    variables inside of functions.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      function_state: Current function name and lines in body so far.
      error: The function to call with any errors found.
    """
    if not function_state.in_a_function:
        return

    lines = clean_lines.lines
    line = lines[line_number]
    if line_number > function_state.body_start_position.row:
        matched_pass_ptr = match(r'^\s*Pass([A-Z][A-Za-z]*)Ptr<', line)
        if matched_pass_ptr:
            type_name = 'Pass%sPtr' % matched_pass_ptr.group(1)
            error(
                line_number, 'readability/pass_ptr', 5,
                'Local variables should never be %s (see '
                'http://webkit.org/coding/RefPtr.html).' % type_name)


def get_previous_non_blank_line(clean_lines, line_number):
    """Return the most recent non-blank line and its line number.

    Args:
      clean_lines: A CleansedLines instance containing the file contents.
      line_number: The number of the line to check.

    Returns:
      A tuple with two elements.  The first element is the contents of the last
      non-blank line before the current line, or the empty string if this is the
      first non-blank line.  The second is the line number of that line, or -1
      if this is the first non-blank line.
    """

    previous_line_number = line_number - 1
    while previous_line_number >= 0:
        previous_line = clean_lines.elided[previous_line_number]
        if not is_blank_line(previous_line):  # if not a blank line...
            return (previous_line, previous_line_number)
        previous_line_number -= 1
    return ('', -1)


def check_ctype_functions(clean_lines, line_number, file_state, error):
    """Looks for use of the standard functions in ctype.h and suggest they be replaced
       by use of equivalent ones in "wtf/text/ascii_ctype.h"?.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    line = clean_lines.elided[line_number]  # Get rid of comments and strings.

    ctype_function_search = search((
        r'\b(?P<ctype_function>(isalnum|isalpha|isascii|isblank|iscntrl|isdigit|isgraph|'
        r'islower|isprint|ispunct|isspace|isupper|isxdigit|toascii|tolower|toupper))\s*\('
    ), line)
    if not ctype_function_search:
        return

    ctype_function = ctype_function_search.group('ctype_function')
    error(
        line_number, 'runtime/ctype_function', 4, 'Use equivalent function in '
        '"third_party/blink/renderer/platform/wtf/text/ascii_ctype.h" instead '
        'of the %s() function.' % (ctype_function))


def replaceable_check(operator, macro, line):
    """Determine whether a basic CHECK can be replaced with a more specific one.

    For example suggest using CHECK_EQ instead of CHECK(a == b) and
    similarly for CHECK_GE, CHECK_GT, CHECK_LE, CHECK_LT, CHECK_NE.

    Args:
      operator: The C++ operator used in the CHECK.
      macro: The CHECK or EXPECT macro being called.
      line: The current source line.

    Returns:
      True if the CHECK can be replaced with a more specific one.
    """

    # This matches decimal and hex integers, strings, and chars (in that order).
    match_constant = r'([-+]?(\d+|0[xX][0-9a-fA-F]+)[lLuU]{0,3}|".*"|\'.*\')'

    # Expression to match two sides of the operator with something that
    # looks like a literal, since CHECK(x == iterator) won't compile.
    # This means we can't catch all the cases where a more specific
    # CHECK is possible, but it's less annoying than dealing with
    # extraneous warnings.
    match_this = (r'\s*' + macro + r'\((\s*' + match_constant + r'\s*' +
                  operator + r'[^<>].*|'
                  r'.*[^<>]' + operator + r'\s*' + match_constant + r'\s*\))')

    # Don't complain about CHECK(x == NULL) or similar because
    # CHECK_EQ(x, NULL) won't compile (requires a cast).
    # Also, don't complain about more complex boolean expressions
    # involving && or || such as CHECK(a == b || c == d).
    return match(match_this, line) and not search(r'NULL|&&|\|\|', line)


def check_check(clean_lines, line_number, error):
    """Checks the use of CHECK and EXPECT macros.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    # Decide the set of replacement macros that should be suggested
    raw_lines = clean_lines.raw_lines
    current_macro = ''
    for macro in _CHECK_MACROS:
        if raw_lines[line_number].find(macro) >= 0:
            current_macro = macro
            break
    if not current_macro:
        # Don't waste time here if line doesn't contain 'CHECK' or 'EXPECT'
        return

    line = clean_lines.elided[line_number]  # get rid of comments and strings

    # Encourage replacing plain CHECKs with CHECK_EQ/CHECK_NE/etc.
    for operator in ['==', '!=', '>=', '>', '<=', '<']:
        if replaceable_check(operator, current_macro, line):
            error(
                line_number, 'readability/check', 2,
                'Consider using %s(a, b) instead of %s(a %s b)' %
                (_CHECK_REPLACEMENT[current_macro][operator], current_macro,
                 operator))
            break


def get_line_width(line):
    """Determines the width of the line in column positions.

    Args:
      line: A string, which may be a Unicode string.

    Returns:
      The width of the line in column positions, accounting for Unicode
      combining characters and wide characters.
    """
    if isinstance(line, six.text_type):
        width = 0
        for c in unicodedata.normalize('NFC', line):
            if unicodedata.east_asian_width(c) in ('W', 'F'):
                width += 2
            elif not unicodedata.combining(c):
                width += 1
        return width
    return len(line)


def check_conditional_and_loop_bodies_for_brace_violations(
        clean_lines, line_number, error):
    """Scans the bodies of conditionals and loops, and in particular
    all the arms of conditionals, for violations in the use of braces.

    Specifically:

    (1) If an arm omits braces, then the following statement must be on one
    physical line.
    (2) If any arm uses braces, all arms must use them.

    These checks are only done here if we find the start of an
    'if/for/foreach/while' statement, because this function fails fast
    if it encounters constructs it doesn't understand. Checks
    elsewhere validate other constraints, such as requiring '}' and
    'else' to be on the same line.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      error: The function to call with any errors found.
    """

    # We work with the elided lines. Comments have been removed, but line
    # numbers are preserved, so we can still find situations where
    # single-expression control clauses span multiple lines, or when a
    # comment preceded the expression.
    lines = clean_lines.elided
    line = lines[line_number]

    # Match control structures.
    control_match = match(r'\s*(if|foreach|for|while)\s*\(', line)
    if not control_match:
        return

    # Found the start of a conditional or loop.

    # The following loop handles all potential arms of the control clause.
    # The initial conditions are the following:
    #   - We start on the opening paren '(' of the condition, *unless* we are
    #     handling an 'else' block, in which case there is no condition.
    #   - In the latter case, we start at the position just beyond the 'else'
    #     token.
    expect_conditional_expression = True
    know_whether_using_braces = False
    using_braces = False
    search_for_else_clause = control_match.group(1) == 'if'
    current_pos = Position(line_number, control_match.end() - 1)

    while True:
        if expect_conditional_expression:
            # Try to find the end of the conditional expression,
            # potentially spanning multiple lines.
            open_paren_pos = current_pos
            close_paren_pos = close_expression(lines, open_paren_pos)
            if close_paren_pos.column < 0:
                return
            current_pos = close_paren_pos

        end_line_of_conditional = current_pos.row

        # Find the start of the body.
        current_pos = _find_in_lines(r'\S', lines, current_pos, None)
        if not current_pos:
            return

        current_arm_uses_brace = False
        if lines[current_pos.row][current_pos.column] == '{':
            current_arm_uses_brace = True
        if know_whether_using_braces:
            if using_braces != current_arm_uses_brace:
                error(
                    current_pos.row, 'whitespace/braces', 4,
                    'If one part of an if-else statement uses curly braces, the other part must too.'
                )
                return
        know_whether_using_braces = True
        using_braces = current_arm_uses_brace

        if using_braces:
            # Skip over the entire arm.
            current_pos = close_expression(lines, current_pos)
            if current_pos.column < 0:
                return
        else:
            # Skip over the current expression.
            current_pos = _find_in_lines(r';', lines, current_pos, None)
            if not current_pos:
                return
            # If the end of the expression is beyond the line just after
            # the close parenthesis or control clause, we've found a
            # single-expression arm that spans multiple lines. (We don't
            # fire this error for expressions ending on the same line; that
            # is a different error, handled elsewhere.)
            if current_pos.row > 1 + end_line_of_conditional:
                error(
                    current_pos.row, 'whitespace/braces', 4,
                    'A conditional or loop body must use braces if the statement is more than one line long.'
                )
                return
            current_pos = Position(current_pos.row, 1 + current_pos.column)

        # At this point current_pos points just past the end of the last
        # arm. If we just handled the last control clause, we're done.
        if not search_for_else_clause:
            return

        # Scan forward for the next non-whitespace character, and see
        # whether we are continuing a conditional (with an 'else' or
        # 'else if'), or are done.
        current_pos = _find_in_lines(r'\S', lines, current_pos, None)
        if not current_pos:
            return
        next_nonspace_string = lines[current_pos.row][current_pos.column:]
        next_conditional = match(r'(else\s*if|else)', next_nonspace_string)
        if not next_conditional:
            # Done processing this 'if' and all arms.
            return
        if next_conditional.group(1) == 'else if':
            current_pos = _find_in_lines(r'\(', lines, current_pos, None)
        else:
            current_pos.column += 4  # skip 'else'
            expect_conditional_expression = False
            search_for_else_clause = False
    # End while loop


def check_redundant_virtual(clean_lines, linenum, error):
    """Checks if line contains a redundant "virtual" function-specifier.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      linenum: The number of the line to check.
      error: The function to call with any errors found.
    """

    # Look for "virtual" on current line.
    line = clean_lines.elided[linenum]
    virtual = match(r'^(.*)(\bvirtual\b)(.*)$', line)
    if not virtual:
        return

    # Ignore "virtual" keywords that are near access-specifiers.  These
    # are only used in class base-specifier and do not apply to member
    # functions.
    if (search(r'\b(public|protected|private)\s+$', virtual.group(1))
            or match(r'^\s+(public|protected|private)\b', virtual.group(3))):
        return

    # Ignore the "virtual" keyword from virtual base classes.  Usually
    # there is a column on the same line in these cases (virtual base
    # classes are rare in google3 because multiple inheritance is rare).
    if match(r'^.*[^:]:[^:].*$', line):
        return

    # Look for the next opening parenthesis.  This is the start of the
    # parameter list (possibly on the next line shortly after virtual).
    # TODO(unknown): doesn't work if there are virtual functions with
    # decltype() or other things that use parentheses, but csearch suggests
    # that this is rare.
    end_position = Position(-1, -1)
    start_col = len(virtual.group(2))
    for start_line in xrange(linenum, min(linenum + 3,
                                          clean_lines.num_lines())):
        line = clean_lines.elided[start_line][start_col:]
        parameter_list = match(r'^([^(]*)\(', line)
        if parameter_list:
            # Match parentheses to find the end of the parameter list
            end_position = close_expression(
                clean_lines.elided,
                Position(start_line, start_col + len(parameter_list.group(1))))
            break
        start_col = 0

    if end_position.column < 0:
        return  # Couldn't find end of parameter list, give up

    # Look for "override" or "final" after the parameter list
    # (possibly on the next few lines).
    for i in xrange(end_position.row,
                    min(end_position.row + 3, clean_lines.num_lines())):
        line = clean_lines.elided[i][end_position.column:]
        override_or_final = search(r'\b(override|final)\b', line)
        if override_or_final:
            error(linenum, 'readability/inheritance', 4,
                  ('"virtual" is redundant since function is '
                   'already declared as "%s"' % override_or_final.group(1)))

        if search(r'[^\w]\s*$', line):
            break


def check_redundant_override(clean_lines, linenum, error):
    """Checks if line contains a redundant "override" virt-specifier.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      linenum: The number of the line to check.
      error: The function to call with any errors found.
    """
    # Look for closing parenthesis nearby.  We need one to confirm where
    # the declarator ends and where the virt-specifier starts to avoid
    # false positives.
    line = clean_lines.elided[linenum]
    declarator_end = line.rfind(')')
    if declarator_end >= 0:
        fragment = line[declarator_end:]
    else:
        if linenum > 1 and clean_lines.elided[linenum - 1].rfind(')') >= 0:
            fragment = line
        else:
            return

        # Check that at most one of "override" or "final" is present, not both
    if search(r'\boverride\b', fragment) and search(r'\bfinal\b', fragment):
        error(linenum, 'readability/inheritance', 4,
              ('"override" is redundant since function is '
               'already declared as "final"'))


def check_style(clean_lines, line_number, file_state, error):
    """Checks rules from the 'C++ style rules' section of cppguide.html.

    Most of these rules are hard to test (naming, comment style), but we
    do what we can.  In particular we check for 4-space indents, line lengths,
    tab usage, spaces inside code, etc.

    Args:
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """

    # Don't use "elided" lines here, otherwise we can't check commented lines.
    # Don't want to use "raw" either, because we don't want to check inside C++11
    # raw strings,
    raw_lines = clean_lines.lines_without_raw_strings
    line = raw_lines[line_number]

    # Some more style checks
    check_ctype_functions(clean_lines, line_number, file_state, error)
    check_check(clean_lines, line_number, error)


_RE_PATTERN_INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]*)[>"].*$')
# Matches the first component of a filename delimited by -s and _s. That is:
#  _RE_FIRST_COMPONENT.match('foo').group(0) == 'foo'
#  _RE_FIRST_COMPONENT.match('foo.cpp').group(0) == 'foo'
#  _RE_FIRST_COMPONENT.match('foo-bar_baz.cpp').group(0) == 'foo'
#  _RE_FIRST_COMPONENT.match('foo_bar-baz.cpp').group(0) == 'foo'
_RE_FIRST_COMPONENT = re.compile(r'^[^-_.]+')


def check_include_line(filename, file_extension, clean_lines, line_number,
                       include_state, error):
    """Check rules that are applicable to #include lines.

    Strings on #include lines are NOT removed from elided line, to make
    certain tasks easier. However, to prevent false positives, checks
    applicable to #include lines in CheckLanguage must be put here.

    Args:
      filename: The name of the current file.
      file_extension: The current file extension, without the leading dot.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      include_state: An _IncludeState instance in which the headers are inserted.
      error: The function to call with any errors found.
    """
    # FIXME: For readability or as a possible optimization, consider
    #        exiting early here by checking whether the "build/include"
    #        category should be checked for the given filename.  This
    #        may involve having the error handler classes expose a
    #        should_check() method, in addition to the usual __call__
    #        method.
    line = clean_lines.lines[line_number]

    matched = _RE_PATTERN_INCLUDE.search(line)
    if not matched:
        return

    include = matched.group(2)
    is_system = (matched.group(1) == '<')

    duplicate_header = include in include_state
    if not duplicate_header:
        include_state[include] = line_number


def check_language(filename, clean_lines, line_number, file_extension,
                   include_state, file_state, error):
    """Checks rules from the 'C++ language rules' section of cppguide.html.

    Some of these rules are hard to test (function overloading, using
    uint32 inappropriately), but we do the best we can.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      line_number: The number of the line to check.
      file_extension: The extension (without the dot) of the filename.
      include_state: An _IncludeState instance in which the headers are inserted.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """
    # If the line is empty or consists of entirely a comment, no need to
    # check it.
    line = clean_lines.elided[line_number]
    if not line:
        return

    matched = _RE_PATTERN_INCLUDE.search(line)
    if matched:
        check_include_line(filename, file_extension, clean_lines, line_number,
                           include_state, error)
        return

    # FIXME: figure out if they're using default arguments in fn proto.

    # Check if they're using a precise-width integer type.
    matched = search(r'\b((un)?signed\s+)?(short|(long\s+)?long)\b', line)
    if matched:
        error(
            line_number, 'runtime/int', 1,
            'Use a precise-width integer type from <stdint.h> or <cstdint>'
            ' such as uint16_t instead of %s' % matched.group(0))

    # Check to see if they're using an conversion function cast.
    # I just try to capture the most common basic types, though there are more.
    # Parameterless conversion functions, such as bool(), are allowed as they are
    # probably a member operator declaration or default constructor.
    matched = search(
        r'\b(int|float|double|bool|char|int32|uint32|int64|uint64)\([^)]',
        line)
    if matched:
        # gMock methods are defined using some variant of MOCK_METHODx(name, type)
        # where type may be float(), int(string), etc.  Without context they are
        # virtually indistinguishable from int(x) casts.
        if not match(r'^\s*MOCK_(CONST_)?METHOD\d+(_T)?\(', line):
            error(
                line_number, 'readability/casting', 4,
                'Using deprecated casting style.  '
                'Use static_cast<%s>(...) instead' % matched.group(1))

    check_c_style_cast(
        line_number, line, clean_lines.raw_lines[line_number], 'static_cast',
        r'\((int|float|double|bool|char|u?int(16|32|64))\)', error)
    # This doesn't catch all cases.  Consider (const char * const)"hello".
    check_c_style_cast(line_number, line, clean_lines.raw_lines[line_number],
                       'reinterpret_cast', r'\((\w+\s?\*+\s?)\)', error)

    if file_extension == 'h':
        # FIXME: check that 1-arg constructors are explicit.
        #        How to tell it's a constructor?
        #        (handled in check_for_non_standard_constructs for now)
        pass

    # When snprintf is used, the second argument shouldn't be a literal.
    matched = search(r'snprintf\s*\(([^,]*),\s*([0-9]*)\s*,', line)
    if matched:
        error(
            line_number, 'runtime/printf', 3,
            'If you can, use sizeof(%s) instead of %s as the 2nd arg '
            'to snprintf.' % (matched.group(1), matched.group(2)))

    # Check if some verboten C functions are being used.
    if search(r'\bsprintf\b', line):
        error(line_number, 'runtime/printf', 5,
              'Never use sprintf.  Use snprintf instead.')
    matched = search(r'\b(strcpy|strcat)\b', line)
    if matched:
        error(line_number, 'runtime/printf', 4,
              'Almost always, snprintf is better than %s' % matched.group(1))

    if search(r'\bsscanf\b', line):
        error(line_number, 'runtime/printf', 1,
              'sscanf can be ok, but is slow and can overflow buffers.')

    # Check for potential format string bugs like printf(foo).
    # We constrain the pattern not to pick things like DocidForPrintf(foo).
    # Not perfect but it can catch printf(foo.c_str()) and printf(foo->c_str())
    matched = re.search(r'\b((?:string)?printf)\s*\(([\w.\->()]+)\)', line,
                        re.I)
    if matched:
        error(
            line_number, 'runtime/printf', 4,
            'Potential format string bug. Do %s("%%s", %s) instead.' %
            (matched.group(1), matched.group(2)))

    # Check for potential memset bugs like memset(buf, sizeof(buf), 0).
    matched = search(r'memset\s*\(([^,]*),\s*([^,]*),\s*0\s*\)', line)
    if matched and not match(r"^''|-?[0-9]+|0x[0-9A-Fa-f]$", matched.group(2)):
        error(
            line_number, 'runtime/memset', 4,
            'Did you mean "memset(%s, 0, %s)"?' % (matched.group(1),
                                                   matched.group(2)))

    # Detect variable-length arrays.
    matched = match(r'\s*(.+::)?(\w+) [a-z]\w*\[(.+)];', line)
    if (matched and matched.group(2) != 'return'
            and matched.group(2) != 'delete'
            and matched.group(3).find(']') == -1):
        # Split the size using space and arithmetic operators as delimiters.
        # If any of the resulting tokens are not compile time constants then
        # report the error.
        tokens = re.split(r'\s|\+|\-|\*|\/|<<|>>]', matched.group(3))
        is_const = True
        skip_next = False
        for tok in tokens:
            if skip_next:
                skip_next = False
                continue

            if search(r'sizeof\(.+\)', tok):
                continue
            if search(r'arraysize\(\w+\)', tok):
                continue

            tok = tok.lstrip('(')
            tok = tok.rstrip(')')
            if not tok:
                continue
            if match(r'\d+', tok):
                continue
            if match(r'0[xX][0-9a-fA-F]+', tok):
                continue
            if match(r'k[A-Z0-9]\w*', tok):
                continue
            if match(r'(.+::)?k[A-Z0-9]\w*', tok):
                continue
            if match(r'(.+::)?[A-Z][A-Z0-9_]*', tok):
                continue
            # A catch all for tricky sizeof cases, including 'sizeof expression',
            # 'sizeof(*type)', 'sizeof(const type)', 'sizeof(struct StructName)'
            # requires skipping the next token because we split on ' ' and '*'.
            if tok.startswith('sizeof'):
                skip_next = True
                continue
            is_const = False
            break
        if not is_const:
            error(
                line_number, 'runtime/arrays', 1,
                'Do not use variable-length arrays.  Use an appropriately named '
                "('k' followed by CamelCase) compile-time constant for the size."
            )

    # Check for plain bitfields declared without either "singed" or "unsigned".
    # Most compilers treat such bitfields as signed, but there are still compilers like
    # RVCT 4.0 that use unsigned by default.
    matched = re.match(
        r'\s*((const|mutable)\s+)?(char|(short(\s+int)?)|int|long(\s+(long|int))?)\s+[a-zA-Z_][a-zA-Z0-9_]*\s*:\s*\d+\s*;',
        line)
    if matched:
        error(
            line_number, 'runtime/bitfields', 5,
            'Please declare integral type bitfields with either signed or unsigned.'
        )

    check_identifier_name_in_declaration(filename, line_number, line,
                                         file_state, error)

    # Check for usage of static_cast<Classname*>.
    check_for_object_static_cast(filename, line_number, line, error)


def check_identifier_name_in_declaration(filename, line_number, line,
                                         file_state, error):
    """Checks if identifier names contain any underscores.

    As identifiers in libraries we are using have a bunch of
    underscores, we only warn about the declarations of identifiers
    and don't check use of identifiers.

    Args:
      filename: The name of the current file.
      line_number: The number of the line to check.
      line: The line of code to check.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: The function to call with any errors found.
    """
    # We don't check return and delete statements and conversion operator declarations.
    if match(r'\s*(return|delete|operator)\b', line):
        return

    # Basically, a declaration is a type name followed by whitespaces
    # followed by an identifier. The type name can be complicated
    # due to type adjectives and templates. We remove them first to
    # simplify the process to find declarations of identifiers.

    # Convert "long long", "long double", and "long long int" to
    # simple types, but don't remove simple "long".
    line = sub(r'long (long )?(?=long|double|int)', '', line)
    # Convert unsigned/signed types to simple types, too.
    line = sub(r'(unsigned|signed) (?=char|short|int|long)', '', line)
    line = sub(
        r'\b(inline|using|static|const|volatile|auto|register|extern|typedef|restrict|struct|class|virtual)(?=\W)',
        '', line)

    # Remove "new" and "new (expr)" to simplify, too.
    line = sub(r'new\s*(\([^)]*\))?', '', line)

    # Remove all template parameters by removing matching < and >.
    # Loop until no templates are removed to remove nested templates.
    while True:
        line, number_of_replacements = subn(r'<([\w\s:]|::)+\s*[*&]*\s*>', '',
                                            line)
        if not number_of_replacements:
            break

    # Declarations of local variables can be in condition expressions
    # of control flow statements (e.g., "if (LayoutObject* p = o->parent())").
    # We remove the keywords and the first parenthesis.
    #
    # Declarations in "while", "if", and "switch" are different from
    # other declarations in two aspects:
    #
    # - There can be only one declaration between the parentheses.
    #   (i.e., you cannot write "if (int i = 0, j = 1) {}")
    # - The variable must be initialized.
    #   (i.e., you cannot write "if (int i) {}")
    #
    # and we will need different treatments for them.
    line = sub(r'^\s*for\s*\(', '', line)
    line, control_statement = subn(r'^\s*(while|else if|if|switch)\s*\(', '',
                                   line)

    # Detect variable and functions.
    type_regexp = r'\w([\w]|\s*[*&]\s*|::)+'
    identifier_regexp = r'(?P<identifier>[\w:]+)'
    maybe_bitfield_regexp = r'(:\s*\d+\s*)?'
    character_after_identifier_regexp = r'(?P<character_after_identifier>[\[;()=,])(?!=)'
    declaration_without_type_regexp = r'\s*' + identifier_regexp + \
        r'\s*' + maybe_bitfield_regexp + character_after_identifier_regexp
    declaration_with_type_regexp = r'\s*' + type_regexp + r'\s' + declaration_without_type_regexp
    is_function_arguments = False
    number_of_identifiers = 0
    while True:
        # If we are seeing the first identifier or arguments of a
        # function, there should be a type name before an identifier.
        if not number_of_identifiers or is_function_arguments:
            declaration_regexp = declaration_with_type_regexp
        else:
            declaration_regexp = declaration_without_type_regexp

        matched = match(declaration_regexp, line)
        if not matched:
            return
        identifier = matched.group('identifier')
        character_after_identifier = matched.group(
            'character_after_identifier')

        # If we removed a non-for-control statement, the character after
        # the identifier should be '='. With this rule, we can avoid
        # warning for cases like "if (val & INT_MAX) {".
        if control_statement and character_after_identifier != '=':
            return

        is_function_arguments = is_function_arguments or character_after_identifier == '('

        # There can be only one declaration in non-for-control statements.
        if control_statement:
            return
        # We should continue checking if this is a function
        # declaration because we need to check its arguments.
        # Also, we need to check multiple declarations.
        if character_after_identifier != '(' and character_after_identifier != ',':
            return

        number_of_identifiers += 1
        line = line[matched.end():]


def check_for_toFoo_definition(filename, pattern, error):
    """Reports for using static_cast instead of toFoo convenience function.

    This function will output warnings to make sure you are actually using
    the added toFoo conversion functions rather than directly hard coding
    the static_cast<Classname*> call. For example, you should toHTMLELement(Node*)
    to convert Node* to HTMLElement*, instead of static_cast<HTMLElement*>(Node*)

    Args:
      filename: The name of the header file in which to check for toFoo definition.
      pattern: The conversion function pattern to grep for.
      error: The function to call with any errors found.
    """

    def get_abs_filepath(filename):
        fileSystem = FileSystem()
        base_dir = fileSystem.path_to_module(FileSystem.__module__).split(
            'WebKit', 1)[0]
        base_dir = ''.join((base_dir, 'WebKit/Source'))
        for root, _, names in os.walk(base_dir):
            if filename in names:
                return os.path.join(root, filename)
        return None

    def grep(lines, pattern, error):
        matches = []
        function_state = None
        for line_number in xrange(lines.num_lines()):
            line = (lines.elided[line_number]).rstrip()
            try:
                if pattern in line:
                    if not function_state:
                        function_state = _FunctionState(1)
                    detect_functions(lines, line_number, function_state, error)
                    # Exclude the match of dummy conversion function. Dummy function is just to
                    # catch invalid conversions and shouldn't be part of possible alternatives.
                    result = re.search(r'%s(\s+)%s' % ('void', pattern), line)
                    if not result:
                        matches.append([
                            line, function_state.body_start_position.row,
                            function_state.end_position.row + 1
                        ])
                        function_state = None
            except UnicodeDecodeError:
                # There would be no non-ascii characters in the codebase ever. The only exception
                # would be comments/copyright text which might have non-ascii characters. Hence,
                # it is perfectly safe to catch the UnicodeDecodeError and just pass the line.
                pass

        return matches

    def check_in_mock_header(filename, matches=None):
        if not filename == 'Foo.h':
            return False

        header_file = None
        try:
            header_file = CppChecker.fs.read_text_file(filename)
        except IOError:
            return False
        line_number = 0
        for line in header_file:
            line_number += 1
            matched = re.search(r'\btoFoo\b', line)
            if matched:
                matches.append(['toFoo', line_number, line_number + 3])
        return True

    # For unit testing only, avoid header search and lookup locally.
    matches = []
    mock_def_found = check_in_mock_header(filename, matches)
    if mock_def_found:
        return matches

    # Regular style check flow. Search for actual header file & defs.
    file_path = get_abs_filepath(filename)
    if not file_path:
        return None
    try:
        f = open(file_path)
        clean_lines = CleansedLines(f.readlines())
    finally:
        f.close()

    # Make a list of all genuine alternatives to static_cast.
    matches = grep(clean_lines, pattern, error)
    return matches


def check_for_object_static_cast(processing_file, line_number, line, error):
    """Checks for a Cpp-style static cast on objects by looking for the pattern.

    Args:
      processing_file: The name of the processing file.
      line_number: The number of the line to check.
      line: The line of code to check.
      error: The function to call with any errors found.
    """
    matched = search(r'\bstatic_cast<(\s*\w*:?:?\w+\s*\*+\s*)>', line)
    if not matched:
        return

    class_name = re.sub(r'[\*]', '', matched.group(1))
    class_name = class_name.strip()
    # Ignore (for now) when the casting is to void*,
    if class_name == 'void':
        return

    namespace_pos = class_name.find(':')
    if not namespace_pos == -1:
        class_name = class_name[namespace_pos + 2:]

    header_file = ''.join((class_name, '.h'))
    matches = check_for_toFoo_definition(header_file, ''.join(
        ('to', class_name)), error)
    # Ignore (for now) if not able to find the header where toFoo might be defined.
    # TODO: Handle cases where Classname might be defined in some other header or cpp file.
    if matches is None:
        return

    report_error = True
    # Ensure found static_cast instance is not from within toFoo definition itself.
    if os.path.basename(processing_file) == header_file:
        for item in matches:
            if line_number in range(item[1], item[2]):
                report_error = False
                break

    if report_error:
        if len(matches):
            # toFoo is defined - enforce using it.
            # TODO: Suggest an appropriate toFoo from the alternatives present in matches.
            error(
                line_number, 'runtime/casting', 4,
                'static_cast of class objects is not allowed. Use to%s defined in %s.'
                % (class_name, header_file))
        else:
            # No toFoo defined - enforce definition & usage.
            # TODO: Automate the generation of toFoo() to avoid any slippages ever.
            error(
                line_number, 'runtime/casting', 4,
                'static_cast of class objects is not allowed. Add to%s in %s and use it instead.'
                % (class_name, header_file))


def check_c_style_cast(line_number, line, raw_line, cast_type, pattern, error):
    """Checks for a C-style cast by looking for the pattern.

    This also handles sizeof(type) warnings, due to similarity of content.

    Args:
      line_number: The number of the line to check.
      line: The line of code to check.
      raw_line: The raw line of code to check, with comments.
      cast_type: The string for the C++ cast to recommend.  This is either
                 reinterpret_cast or static_cast, depending.
      pattern: The regular expression used to find C-style casts.
      error: The function to call with any errors found.
    """
    matched = search(pattern, line)
    if not matched:
        return

    # e.g., sizeof(int)
    sizeof_match = match(r'.*sizeof\s*$', line[0:matched.start(1) - 1])
    if sizeof_match:
        error(line_number, 'runtime/sizeof', 1,
              'Using sizeof(type).  Use sizeof(varname) instead if possible')
        return

    remainder = line[matched.end(0):]

    # The close paren is for function pointers as arguments to a function.
    # eg, void foo(void (*bar)(int));
    # The semicolon check is a more basic function check; also possibly a
    # function pointer typedef.
    # eg, void foo(int); or void foo(int) const;
    # The equals check is for function pointer assignment.
    # eg, void *(*foo)(int) = ...
    #
    # Right now, this will only catch cases where there's a single argument, and
    # it's unnamed.  It should probably be expanded to check for multiple
    # arguments with some unnamed.
    function_match = match(r'\s*(\)|=|(const)?\s*(;|\{|throw\(\)))', remainder)
    if function_match:
        return

    # At this point, all that should be left is actual casts.
    error(
        line_number, 'readability/casting', 4,
        'Using C-style cast.  Use %s<%s>(...) instead' % (cast_type,
                                                          matched.group(1)))


_HEADERS_CONTAINING_TEMPLATES = (
    ('<deque>', ('deque', )),
    ('<functional>', (
        'unary_function',
        'binary_function',
        'plus',
        'minus',
        'multiplies',
        'divides',
        'modulus',
        'negate',
        'equal_to',
        'not_equal_to',
        'greater',
        'less',
        'greater_equal',
        'less_equal',
        'logical_and',
        'logical_or',
        'logical_not',
        'unary_negate',
        'not1',
        'binary_negate',
        'not2',
        'bind1st',
        'bind2nd',
        'pointer_to_unary_function',
        'pointer_to_binary_function',
        'ptr_fun',
        'mem_fun_t',
        'mem_fun',
        'mem_fun1_t',
        'mem_fun1_ref_t',
        'mem_fun_ref_t',
        'const_mem_fun_t',
        'const_mem_fun1_t',
        'const_mem_fun_ref_t',
        'const_mem_fun1_ref_t',
        'mem_fun_ref',
    )),
    ('<limits>', ('numeric_limits', )),
    ('<list>', ('list', )),
    ('<map>', (
        'map',
        'multimap',
    )),
    ('<memory>', ('allocator', )),
    ('<queue>', (
        'queue',
        'priority_queue',
    )),
    ('<set>', (
        'set',
        'multiset',
    )),
    ('<stack>', ('stack', )),
    ('<string>', (
        'char_traits',
        'basic_string',
    )),
    ('<utility>', ('pair', )),
    ('<vector>', ('vector', )),

    # gcc extensions.
    # Note: std::hash is their hash, ::hash is our hash
    ('<hash_map>', (
        'hash_map',
        'hash_multimap',
    )),
    ('<hash_set>', (
        'hash_set',
        'hash_multiset',
    )),
    ('<slist>', ('slist', )),
)

_HEADERS_ACCEPTED_BUT_NOT_PROMOTED = {
    # We can trust with reasonable confidence that map gives us pair<>, too.
    'pair<>': ('map', 'multimap', 'hash_map', 'hash_multimap')
}

_RE_PATTERN_STRING = re.compile(r'\bstring\b')

_re_pattern_algorithm_header = []
for _template in ('copy', 'max', 'min', 'min_element', 'sort', 'swap',
                  'transform'):
    # Match max<type>(..., ...), max(..., ...), but not foo->max, foo.max,
    # or type::max().
    _re_pattern_algorithm_header.append(
        (re.compile(r'[^>.]\b' + _template + r'(<.*?>)?\([^\)]'), _template,
         '<algorithm>'))

_re_pattern_templates = []
for _header, _templates in _HEADERS_CONTAINING_TEMPLATES:
    for _template in _templates:
        _re_pattern_templates.append(
            (re.compile(r'(\<|\b)' + _template + r'\s*\<'), _template + '<>',
             _header))


def files_belong_to_same_module(filename_cpp, filename_h):
    """Check if these two filenames belong to the same module.

    The concept of a 'module' here is a as follows:
    foo.h, foo-inl.h, foo.cpp, foo_test.cpp and foo_unittest.cpp belong to the
    same 'module' if they are in the same directory.
    some/path/public/xyzzy and some/path/internal/xyzzy are also considered
    to belong to the same module here.

    If the filename_cpp contains a longer path than the filename_h, for example,
    '/absolute/path/to/base/sysinfo.cpp', and this file would include
    'base/sysinfo.h', this function also produces the prefix needed to open the
    header. This is used by the caller of this function to more robustly open the
    header file. We don't have access to the real include paths in this context,
    so we need this guesswork here.

    Known bugs: tools/base/bar.cpp and base/bar.h belong to the same module
    according to this implementation. Because of this, this function gives
    some false positives. This should be sufficiently rare in practice.

    Args:
      filename_cpp: is the path for the .cpp file
      filename_h: is the path for the header path

    Returns:
      Tuple with a bool and a string:
      bool: True if filename_cpp and filename_h belong to the same module.
      string: the additional prefix needed to open the header file.
    """

    if not filename_cpp.endswith('.cpp'):
        return (False, '')
    filename_cpp = filename_cpp[:-len('.cpp')]
    if filename_cpp.endswith('_unittest'):
        filename_cpp = filename_cpp[:-len('_unittest')]
    elif filename_cpp.endswith('_test'):
        filename_cpp = filename_cpp[:-len('_test')]
    filename_cpp = filename_cpp.replace('/public/', '/')
    filename_cpp = filename_cpp.replace('/internal/', '/')

    if not filename_h.endswith('.h'):
        return (False, '')
    filename_h = filename_h[:-len('.h')]
    if filename_h.endswith('-inl'):
        filename_h = filename_h[:-len('-inl')]
    filename_h = filename_h.replace('/public/', '/')
    filename_h = filename_h.replace('/internal/', '/')

    files_belong_to_same_module = filename_cpp.endswith(filename_h)
    common_path = ''
    if files_belong_to_same_module:
        common_path = filename_cpp[:-len(filename_h)]
    return files_belong_to_same_module, common_path


def update_include_state(filename, include_state):
    """Fill up the include_state with new includes found from the file.

    Args:
      filename: the name of the header to read.
      include_state: an _IncludeState instance in which the headers are inserted.
      io: The io factory to use to read the file. Provided for testability.

    Returns:
      True if a header was succesfully added. False otherwise.
    """
    header_file = None
    try:
        header_file = CppChecker.fs.read_text_file(filename)
    except IOError:
        return False
    line_number = 0
    for line in header_file:
        line_number += 1
        clean_line = cleanse_comments(line)
        matched = _RE_PATTERN_INCLUDE.search(clean_line)
        if matched:
            include = matched.group(2)
            # The value formatting is cute, but not really used right now.
            # What matters here is that the key is in include_state.
            include_state.setdefault(include,
                                     '%s:%d' % (filename, line_number))
    return True


def check_for_include_what_you_use(filename, clean_lines, include_state,
                                   error):
    """Reports for missing stl includes.

    This function will output warnings to make sure you are including the headers
    necessary for the stl containers and functions that you use. We only give one
    reason to include a header. For example, if you use both equal_to<> and
    less<> in a .h file, only one (the latter in the file) of these will be
    reported as a reason to include the <functional>.

    Args:
      filename: The name of the current file.
      clean_lines: A CleansedLines instance containing the file.
      include_state: An _IncludeState instance.
      error: The function to call with any errors found.
    """
    # A map of header name to line_number and the template entity.
    required = {}
    # Example of required: { '<functional>': (1219, 'less<>') }

    for line_number in xrange(clean_lines.num_lines()):
        line = clean_lines.elided[line_number]
        if not line or line[0] == '#':
            continue

        # String is special -- it is a non-templatized type in STL.
        if _RE_PATTERN_STRING.search(line):
            required['<string>'] = (line_number, 'string')

        for pattern, template, header in _re_pattern_algorithm_header:
            if pattern.search(line):
                required[header] = (line_number, template)

        # The following function is just a speed up, no semantics are changed.
        if not '<' in line:  # Reduces the cpu time usage by skipping lines.
            continue

        for pattern, template, header in _re_pattern_templates:
            if pattern.search(line):
                required[header] = (line_number, template)

    # The policy is that if you #include something in foo.h you don't need to
    # include it again in foo.cpp. Here, we will look at possible includes.
    # Let's copy the include_state so it is only messed up within this function.
    include_state = include_state.copy()

    # Did we find the header for this file (if any) and succesfully load it?
    header_found = False

    # Use the absolute path so that matching works properly.
    abs_filename = os.path.abspath(filename)

    # For Emacs's flymake.
    # If cpp_style is invoked from Emacs's flymake, a temporary file is generated
    # by flymake and that file name might end with '_flymake.cpp'. In that case,
    # restore original file name here so that the corresponding header file can be
    # found.
    # e.g. If the file name is 'foo_flymake.cpp', we should search for 'foo.h'
    # instead of 'foo_flymake.h'
    abs_filename = re.sub(r'_flymake\.cpp$', '.cpp', abs_filename)

    # include_state is modified during iteration, so we iterate over a copy of
    # the keys.
    for header in list(include_state):  # NOLINT
        (same_module, common_path) = files_belong_to_same_module(
            abs_filename, header)
        fullpath = common_path + header
        if same_module and update_include_state(fullpath, include_state):
            header_found = True

    # If we can't find the header file for a .cpp, assume it's because we don't
    # know where to look. In that case we'll give up as we're not sure they
    # didn't include it in the .h file.
    # FIXME: Do a better job of finding .h files so we are confident that
    #        not having the .h file means there isn't one.
    if filename.endswith('.cpp') and not header_found:
        return

    # All the lines have been processed, report the errors found.
    for required_header_unstripped in required:
        template = required[required_header_unstripped][1]
        if template in _HEADERS_ACCEPTED_BUT_NOT_PROMOTED:
            headers = _HEADERS_ACCEPTED_BUT_NOT_PROMOTED[template]
            if [True for header in headers if header in include_state]:
                continue
        if required_header_unstripped.strip('<>"') not in include_state:
            error(
                required[required_header_unstripped][0],
                'build/include_what_you_use', 4, 'Add #include ' +
                required_header_unstripped + ' for ' + template)


def process_line(filename, file_extension, clean_lines, line, include_state,
                 function_state, class_state, file_state, error):
    """Processes a single line in the file.

    Args:
      filename: Filename of the file that is being processed.
      file_extension: The extension (dot not included) of the file.
      clean_lines: An array of strings, each representing a line of the file,
                   with comments stripped.
      line: Number of line being processed.
      include_state: An _IncludeState instance in which the headers are inserted.
      function_state: A _FunctionState instance which counts function lines, etc.
      class_state: A _ClassState instance which maintains information about
                   the current stack of nested class declarations being parsed.
      file_state: A _FileState instance which maintains information about
                  the state of things in the file.
      error: A callable to which errors are reported, which takes arguments:
             line number, error level, and message
    """
    raw_lines = clean_lines.raw_lines
    detect_functions(clean_lines, line, function_state, error)
    check_for_function_lengths(clean_lines, line, function_state, error)
    if search(r'\bNOLINT\b', raw_lines[line]):  # ignore nolint lines
        return
    # Ignore asm lines as they format differently.
    if match(r'\s*\b__asm\b', raw_lines[line]):
        return
    check_pass_ptr_usage(clean_lines, line, function_state, error)
    check_style(clean_lines, line, file_state, error)
    check_language(filename, clean_lines, line, file_extension, include_state,
                   file_state, error)
    check_for_non_standard_constructs(clean_lines, line, class_state, error)
    check_posix_threading(clean_lines, line, error)
    check_invalid_increment(clean_lines, line, error)
    check_conditional_and_loop_bodies_for_brace_violations(
        clean_lines, line, error)
    check_redundant_virtual(clean_lines, line, error)
    check_redundant_override(clean_lines, line, error)


def _process_lines(filename, file_extension, lines, error, min_confidence):
    """Performs lint checks and reports any errors to the given error function.

    Args:
      filename: Filename of the file that is being processed.
      file_extension: The extension (dot not included) of the file.
      lines: An array of strings, each representing a line of the file, with the
             last element being empty if the file is terminated with a newline.
      error: A callable to which errors are reported, which takes 4 arguments:
    """
    lines = (['// marker so line numbers and indices both start at 1'] + lines
             + ['// marker so line numbers end in a known way'])

    include_state = _IncludeState()
    function_state = _FunctionState(min_confidence)
    class_state = _ClassState()

    check_for_copyright(lines, error)
    remove_multi_line_comments(lines, error)
    clean_lines = CleansedLines(lines)

    if file_extension == 'h':
        check_for_header_guard(filename, clean_lines, error)

    file_state = _FileState(clean_lines, file_extension)
    for line in xrange(clean_lines.num_lines()):
        process_line(filename, file_extension, clean_lines, line,
                     include_state, function_state, class_state, file_state,
                     error)

    check_for_include_what_you_use(filename, clean_lines, include_state, error)

    # We check here rather than inside process_line so that we see raw
    # lines rather than "cleaned" lines.
    check_for_unicode_replacement_characters(lines, error)

    check_for_new_line_at_eof(lines, error)


class CppChecker(object):
    """Processes C++ lines for checking style."""

    # This list is used to--
    #
    # (1) generate an explicit list of all possible categories,
    # (2) unit test that all checked categories have valid names, and
    # (3) unit test that all categories are getting unit tested.
    #
    categories = set([
        'build/header_guard',
        'build/include_what_you_use',
        'legal/copyright',
        'readability/casting',
        'readability/check',
        'readability/control_flow',
        'readability/enum_casing',
        'readability/fn_size',
        # TODO(dcheng): Turn on the clang plugin checks and remove this.
        'readability/inheritance',
        'readability/pass_ptr',
        'readability/utf8',
        'runtime/arrays',
        'runtime/bitfields',
        'runtime/casting',
        'runtime/ctype_function',
        'runtime/explicit',
        'runtime/int',
        'runtime/invalid_increment',
        'runtime/max_min_macros',
        'runtime/memset',
        'runtime/printf',
        'runtime/sizeof',
        'runtime/threadsafe_fn',
        'runtime/virtual',
        'whitespace/braces',
        'whitespace/ending_newline',
    ])

    fs = None

    def __init__(self,
                 file_path,
                 file_extension,
                 handle_style_error,
                 min_confidence,
                 fs=None):
        """Create a CppChecker instance.

        Args:
          file_extension: A string that is the file extension, without
                          the leading dot.
        """
        self.file_extension = file_extension
        self.file_path = file_path
        self.handle_style_error = handle_style_error
        self.min_confidence = min_confidence
        CppChecker.fs = fs or FileSystem()

    # Useful for unit testing.
    def __eq__(self, other):
        """Return whether this CppChecker instance is equal to another."""
        if self.file_extension != other.file_extension:
            return False
        if self.file_path != other.file_path:
            return False
        if self.handle_style_error != other.handle_style_error:
            return False
        if self.min_confidence != other.min_confidence:
            return False

        return True

    # Useful for unit testing.
    def __ne__(self, other):
        # Python does not automatically deduce __ne__() from __eq__().
        return not self.__eq__(other)

    def check(self, lines):
        _process_lines(self.file_path, self.file_extension, lines,
                       self.handle_style_error, self.min_confidence)


# FIXME: Remove this function (requires refactoring unit tests).
def process_file_data(filename,
                      file_extension,
                      lines,
                      error,
                      min_confidence,
                      fs=None):
    checker = CppChecker(filename, file_extension, error, min_confidence, fs)
    checker.check(lines)
