# Copyright (C) 2022 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

<<<<<<< HEAD
import os
import sys
import unittest
=======
import unittest
import os
import sys
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

ROOT_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(os.path.join(ROOT_DIR))

<<<<<<< HEAD
from python.generators.sql_processing.docs_parse import Arg, parse_file
=======
from generators.stdlib_docs.stdlib import FunctionDocs, ViewFunctionDocs, TableViewDocs

DESC = """--
-- First line.
-- Second line."""

COLS_STR = """--
-- @column slice_id           Id of slice.
-- @column slice_name         Name of slice."""

COLS_SQL_STR = "slice_id INT, slice_name STRING"

ARGS_STR = """--
-- @arg utid INT              Utid of thread.
-- @arg name STRING           String name.
"""

ARGS_SQL_STR = "utid INT, name STRING"

RET_STR = """--
-- @ret BOOL                  Exists.
"""

RET_SQL_STR = "BOOL"

SQL_STR = "SELECT * FROM slice"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))


class TestStdlib(unittest.TestCase):

<<<<<<< HEAD
  # Checks that custom prefixes (cr for chrome/util) are allowed.
  def test_custom_module_prefix(self):
    res = parse_file(
        'chrome/util/test.sql', f'''
-- Comment
CREATE PERFETTO TABLE cr_table(
    -- Column.
    x LONG
) AS
SELECT 1;
    '''.strip())
    self.assertListEqual(res.errors, [])

    fn = res.table_views[0]
    self.assertEqual(fn.name, 'cr_table')
    self.assertEqual(fn.desc, 'Comment')
    self.assertEqual(fn.cols, {
        'x': Arg('LONG', 'LONG', 'Column.'),
    })

  # Checks that when custom prefixes (cr for chrome/util) are present,
  # the full module name (chrome) is still accepted.
  def test_custom_module_prefix_full_module_name(self):
    res = parse_file(
        'chrome/util/test.sql', f'''
-- Comment
CREATE PERFETTO TABLE chrome_table(
    -- Column.
    x LONG
) AS
SELECT 1;
    '''.strip())
    self.assertListEqual(res.errors, [])

    fn = res.table_views[0]
    self.assertEqual(fn.name, 'chrome_table')
    self.assertEqual(fn.desc, 'Comment')
    self.assertEqual(fn.cols, {
        'x': Arg('LONG', 'LONG', 'Column.'),
    })

  # Checks that when custom prefixes (cr for chrome/util) are present,
  # the incorrect prefixes (foo) are not accepted.
  def test_custom_module_prefix_incorrect(self):
    res = parse_file(
        'chrome/util/test.sql', f'''
-- Comment
CREATE PERFETTO TABLE foo_table(
    -- Column.
    x LONG
) AS
SELECT 1;
    '''.strip())
    # Expecting an error: table prefix (foo) is not allowed for a given path
    # (allowed: chrome, cr).
    self.assertEqual(len(res.errors), 1)

  # Checks that when custom prefixes (cr for chrome/util) are present,
  # they do not apply outside of the path scope.
  def test_custom_module_prefix_does_not_apply_outside(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Comment
CREATE PERFETTO TABLE cr_table(
    -- Column.
    x LONG
) AS
SELECT 1;
    '''.strip())
    # Expecting an error: table prefix (foo) is not allowed for a given path
    # (allowed: foo).
    self.assertEqual(len(res.errors), 1)

  def test_ret_no_desc(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Comment
CREATE PERFETTO FUNCTION foo_fn()
--
RETURNS BOOL
AS
SELECT TRUE;
    '''.strip())
    # Expecting an error: return value is missing a description.
    self.assertEqual(len(res.errors), 1)

  def test_multiline_desc(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- This
-- is
--
-- a
--      very
--
-- long
--
-- description.
CREATE PERFETTO FUNCTION foo_fn()
-- Exists.
RETURNS BOOL
AS
SELECT 1;
    '''.strip())
    self.assertListEqual(res.errors, [])

    fn = res.functions[0]
    self.assertEqual(fn.desc,
                     'This\n is\n\n a\n      very\n\n long\n\n description.')

  def test_function_name_style(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Function comment.
CREATE PERFETTO FUNCTION foo_SnakeCase()
-- Exists.
RETURNS BOOL
AS
SELECT 1;
    '''.strip())
    # Expecting an error: function name should be using hacker_style.
    self.assertEqual(len(res.errors), 1)

  def test_table_with_schema(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Table comment.
CREATE PERFETTO TABLE foo_table(
    -- Id of slice.
    id LONG
) AS
SELECT 1 as id;
    '''.strip())
    self.assertListEqual(res.errors, [])

    table = res.table_views[0]
    self.assertEqual(table.name, 'foo_table')
    self.assertEqual(table.desc, 'Table comment.')
    self.assertEqual(table.type, 'TABLE')
    self.assertEqual(table.cols, {
        'id': Arg('LONG', 'LONG', 'Id of slice.'),
    })

  def test_perfetto_view_with_schema(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- View comment.
CREATE PERFETTO VIEW foo_table(
    -- Foo.
    foo LONG,
    -- Bar.
    bar STRING
) AS
SELECT 1;
    '''.strip())
    self.assertListEqual(res.errors, [])

    table = res.table_views[0]
    self.assertEqual(table.name, 'foo_table')
    self.assertEqual(table.desc, 'View comment.')
    self.assertEqual(table.type, 'VIEW')
    self.assertEqual(table.cols, {
        'foo': Arg('LONG', 'LONG', 'Foo.'),
        'bar': Arg('STRING', 'STRING', 'Bar.'),
    })

  def test_function_with_new_style_docs(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Function foo.
CREATE PERFETTO FUNCTION foo_fn(
    -- Utid of thread.
    utid LONG,
    -- String name.
    name STRING)
-- Exists.
RETURNS BOOL
AS
SELECT 1;
    '''.strip())
    self.assertListEqual(res.errors, [])

    fn = res.functions[0]
    self.assertEqual(fn.name, 'foo_fn')
    self.assertEqual(fn.desc, 'Function foo.')
    self.assertEqual(
        fn.args, {
            'utid': Arg('LONG', 'LONG', 'Utid of thread.'),
            'name': Arg('STRING', 'STRING', 'String name.'),
        })
    self.assertEqual(fn.return_type, 'BOOL')
    self.assertEqual(fn.return_desc, 'Exists.')

  def test_function_returns_table_with_new_style_docs(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Function foo.
CREATE PERFETTO FUNCTION foo_fn(
    -- Utid of thread.
    utid LONG)
-- Impl comment.
RETURNS TABLE(
    -- Count.
    count LONG
)
AS
SELECT 1;
    '''.strip())
    self.assertListEqual(res.errors, [])

    fn = res.table_functions[0]
    self.assertEqual(fn.name, 'foo_fn')
    self.assertEqual(fn.desc, 'Function foo.')
    self.assertEqual(fn.args, {
        'utid': Arg('LONG', 'LONG', 'Utid of thread.'),
    })
    self.assertEqual(fn.cols, {
        'count': Arg('LONG', 'LONG', 'Count.'),
    })

  def test_function_with_new_style_docs_multiline_comment(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Function foo.
CREATE PERFETTO FUNCTION foo_fn(
    -- Multi
    -- line
    --
    -- comment.
    arg LONG)
-- Exists.
RETURNS BOOL
AS
SELECT 1;
    '''.strip())
    self.assertListEqual(res.errors, [])

    fn = res.functions[0]
    self.assertEqual(fn.name, 'foo_fn')
    self.assertEqual(fn.desc, 'Function foo.')
    self.assertEqual(fn.args, {
        'arg': Arg('LONG', 'LONG', 'Multi line  comment.'),
    })
    self.assertEqual(fn.return_type, 'BOOL')
    self.assertEqual(fn.return_desc, 'Exists.')

  def test_function_with_multiline_return_comment(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Function foo.
CREATE PERFETTO FUNCTION foo_fn(
    -- Arg
    arg LONG)
-- Multi
-- line
-- return
-- comment.
RETURNS BOOL
AS
SELECT 1;
    '''.strip())
    self.assertListEqual(res.errors, [])

    fn = res.functions[0]
    self.assertEqual(fn.name, 'foo_fn')
    self.assertEqual(fn.desc, 'Function foo.')
    self.assertEqual(fn.args, {
        'arg': Arg('LONG', 'LONG', 'Arg'),
    })
    self.assertEqual(fn.return_type, 'BOOL')
    self.assertEqual(fn.return_desc, 'Multi line return comment.')

  def test_create_or_replace_table_banned(self):
    res = parse_file(
        'common/bar.sql', f'''
-- Table.
CREATE OR REPLACE PERFETTO TABLE foo(
    -- Column.
    x LONG
)
AS
SELECT 1;

    '''.strip())
    # Expecting an error: CREATE OR REPLACE is not allowed in stdlib.
    self.assertEqual(len(res.errors), 1)

  def test_create_or_replace_view_banned(self):
    res = parse_file(
        'common/bar.sql', f'''
-- Table.
CREATE OR REPLACE PERFETTO VIEW foo(
    -- Column.
    x LONG
)
AS
SELECT 1;

    '''.strip())
    # Expecting an error: CREATE OR REPLACE is not allowed in stdlib.
    self.assertEqual(len(res.errors), 1)

  def test_create_or_replace_function_banned(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Function foo.
CREATE OR REPLACE PERFETTO FUNCTION foo_fn()
-- Exists.
RETURNS BOOL
AS
SELECT 1;
    '''.strip())
    # Expecting an error: CREATE OR REPLACE is not allowed in stdlib.
    self.assertEqual(len(res.errors), 1)

  def test_function_with_new_style_docs_with_parenthesis(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Function foo.
CREATE PERFETTO FUNCTION foo_fn(
    -- Utid of thread (important).
    utid LONG)
-- Exists.
RETURNS BOOL
AS
SELECT 1;
    '''.strip())
    self.assertListEqual(res.errors, [])

    fn = res.functions[0]
    self.assertEqual(fn.name, 'foo_fn')
    self.assertEqual(fn.desc, 'Function foo.')
    self.assertEqual(fn.args, {
        'utid': Arg('LONG', 'LONG', 'Utid of thread (important).'),
    })
    self.assertEqual(fn.return_type, 'BOOL')
    self.assertEqual(fn.return_desc, 'Exists.')

  def test_macro(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Macro
CREATE OR REPLACE PERFETTO FUNCTION foo_fn()
-- Exists.
RETURNS BOOL
AS
SELECT 1;
    '''.strip())
    # Expecting an error: CREATE OR REPLACE is not allowed in stdlib.
    self.assertEqual(len(res.errors), 1)

  def test_create_or_replace_macro_smoke(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Macro
CREATE PERFETTO MACRO foo_macro(
  -- x Arg.
  x TableOrSubquery
)
-- Exists.
RETURNS TableOrSubquery
AS
SELECT 1;
    '''.strip())

    macro = res.macros[0]
    self.assertEqual(macro.name, 'foo_macro')
    self.assertEqual(macro.desc, 'Macro')
    self.assertEqual(macro.args, {
        'x': Arg('TableOrSubquery', 'TableOrSubquery', 'x Arg.'),
    })
    self.assertEqual(macro.return_type, 'TableOrSubquery')
    self.assertEqual(macro.return_desc, 'Exists.')

  def test_create_or_replace_macro_banned(self):
    res = parse_file(
        'foo/bar.sql', f'''
-- Macro
CREATE OR REPLACE PERFETTO MACRO foo_macro(
  -- x Arg.
  x TableOrSubquery
)
-- Exists.
RETURNS TableOrSubquery
AS
SELECT 1;
    '''.strip())
    # Expecting an error: CREATE OR REPLACE is not allowed in stdlib.
    self.assertEqual(len(res.errors), 1)
=======
  # Valid schemas

  def test_valid_table(self):
    valid_table_comment = f"{DESC}\n{COLS_STR}".split('\n')

    docs, create_errors = TableViewDocs.create_from_comment(
        "", valid_table_comment, 'common', ('table', 'tab_name', 'to_ignore'))
    self.assertFalse(create_errors)

    validation_errors = docs.check_comment()
    self.assertFalse(validation_errors)

  def test_valid_function(self):
    valid_function = f"{DESC}\n{ARGS_STR}\n{RET_STR}".split('\n')
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, RET_SQL_STR, SQL_STR)
    docs, create_errors = FunctionDocs.create_from_comment(
        "", valid_function, 'common', valid_regex_matches)
    self.assertFalse(create_errors)

    validation_errors = docs.check_comment()
    self.assertFalse(validation_errors)

  def test_valid_view_function(self):
    valid_view_function = f"{DESC}\n{ARGS_STR}\n{COLS_STR}".split('\n')
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, COLS_SQL_STR, SQL_STR)
    docs, create_errors = ViewFunctionDocs.create_from_comment(
        "", valid_view_function, 'common', valid_regex_matches)
    self.assertFalse(create_errors)

    validation_errors = docs.check_comment()
    self.assertFalse(validation_errors)

  # Missing modules in names

  def test_missing_module_in_table_name(self):
    valid_table_comment = f"{DESC}\n{COLS_STR}".split('\n')

    _, create_errors = TableViewDocs.create_from_comment(
        "", valid_table_comment, 'android', ('table', 'tab_name', 'to_ignore'))
    self.assertTrue(create_errors)

  def test_missing_module_in_function_name(self):
    valid_function = f"{DESC}\n{ARGS_STR}\n{RET_STR}".split('\n')
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, RET_SQL_STR, SQL_STR)
    _, create_errors = FunctionDocs.create_from_comment("", valid_function,
                                                        'android',
                                                        valid_regex_matches)
    self.assertTrue(create_errors)

  def test_missing_module_in_view_function_name(self):
    valid_view_function = f"{DESC}\n{ARGS_STR}\n{COLS_STR}".split('\n')
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, COLS_SQL_STR, SQL_STR)
    _, create_errors = ViewFunctionDocs.create_from_comment(
        "", valid_view_function, 'android', valid_regex_matches)
    self.assertTrue(create_errors)

  # Missing part of schemas

  def test_missing_desc_in_table_name(self):
    comment = f"{COLS_STR}".split('\n')

    _, create_errors = TableViewDocs.create_from_comment(
        "", comment, 'common', ('table', 'tab_name', 'to_ignore'))
    self.assertTrue(create_errors)

  def test_missing_cols_in_table(self):
    comment = f"{DESC}".split('\n')

    _, create_errors = TableViewDocs.create_from_comment(
        "", comment, 'common', ('table', 'tab_name', 'to_ignore'))
    self.assertTrue(create_errors)

  def test_missing_desc_in_function(self):
    comment = f"{ARGS_STR}\n{RET_STR}".split('\n')
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, RET_SQL_STR, SQL_STR)
    _, create_errors = FunctionDocs.create_from_comment("", comment, 'common',
                                                        valid_regex_matches)
    self.assertTrue(create_errors)

  def test_missing_args_in_function(self):
    comment = f"{DESC}\n{RET_STR}".split('\n')
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, RET_SQL_STR, SQL_STR)
    _, create_errors = FunctionDocs.create_from_comment("", comment, 'common',
                                                        valid_regex_matches)
    self.assertTrue(create_errors)

  def test_missing_ret_in_function(self):
    comment = f"{DESC}\n{ARGS_STR}".split('\n')
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, RET_SQL_STR, SQL_STR)
    _, create_errors = FunctionDocs.create_from_comment("", comment, 'common',
                                                        valid_regex_matches)
    self.assertTrue(create_errors)

  def test_missing_desc_in_view_function(self):
    comment = f"{ARGS_STR}\n{COLS_STR}".split('\n')
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, COLS_SQL_STR, SQL_STR)
    _, create_errors = ViewFunctionDocs.create_from_comment(
        "", comment, 'common', valid_regex_matches)
    self.assertTrue(create_errors)

  def test_missing_args_in_view_function(self):
    comment = f"{DESC}\n{COLS_STR}".split('\n')
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, COLS_SQL_STR, SQL_STR)
    _, create_errors = ViewFunctionDocs.create_from_comment(
        "", comment, 'common', valid_regex_matches)
    self.assertTrue(create_errors)

  def test_missing_cols_in_view_function(self):
    comment = f"{DESC}\n{ARGS_STR}".split('\n')
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, COLS_SQL_STR, SQL_STR)
    _, create_errors = ViewFunctionDocs.create_from_comment(
        "", comment, 'common', valid_regex_matches)
    self.assertTrue(create_errors)

  # Validate elements

  def test_invalid_table_columns(self):
    invalid_cols = "-- @column slice_id"
    comment = f"{DESC}\n{invalid_cols}".split('\n')

    docs, create_errors = TableViewDocs.create_from_comment(
        "", comment, 'common', ('table', 'tab_name', 'to_ignore'))
    self.assertFalse(create_errors)

    validation_errors = docs.check_comment()
    self.assertTrue(validation_errors)

  def test_invalid_view_function_columns(self):
    comment = f"{DESC}\n{ARGS_STR}\n{COLS_STR}".split('\n')
    cols_sql_str = "slice_id INT"
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, cols_sql_str, SQL_STR)
    docs, create_errors = ViewFunctionDocs.create_from_comment(
        "", comment, 'common', valid_regex_matches)
    self.assertFalse(create_errors)

    validation_errors = docs.check_comment()
    self.assertTrue(validation_errors)

  def test_invalid_arguments(self):
    valid_function = f"{DESC}\n{ARGS_STR}\n{RET_STR}".split('\n')
    args_sql_str = "utid BOOL"
    valid_regex_matches = ('fun_name', args_sql_str, RET_SQL_STR, SQL_STR)
    docs, create_errors = FunctionDocs.create_from_comment(
        "", valid_function, 'common', valid_regex_matches)
    self.assertFalse(create_errors)

    validation_errors = docs.check_comment()
    self.assertTrue(validation_errors)

  def test_invalid_ret(self):
    valid_function = f"{DESC}\n{ARGS_STR}\n{RET_STR}".split('\n')
    ret_sql_str = "utid BOOL"
    valid_regex_matches = ('fun_name', ARGS_SQL_STR, ret_sql_str, SQL_STR)
    docs, create_errors = FunctionDocs.create_from_comment(
        "", valid_function, 'common', valid_regex_matches)
    self.assertFalse(create_errors)

    validation_errors = docs.check_comment()
    self.assertTrue(validation_errors)
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
