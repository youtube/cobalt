import unittest
from unittest.mock import mock_open, patch

import common


class GetFieldDefPosTest(unittest.TestCase):

  def test_single_line_definition(self):
    deps_content = """
deps = {
    'src/foo': 'url/to/foo',
    'src/bar': 'url/to/bar',
}
"""
    with patch('builtins.open', mock_open(read_data=deps_content)):
      start, end = common.get_field_def_pos('dummy_deps', 'src/foo')
      self.assertEqual((2, 3), (start, end))

  def test_new_line_in_single_line_definition(self):
    deps_content = """
deps = {
    'src/foo': 
        'url/to/foo',
    'src/bar': 'url/to/bar',
}
"""
    with patch('builtins.open', mock_open(read_data=deps_content)):
      start, end = common.get_field_def_pos('dummy_deps', 'src/foo')
      self.assertEqual((2, 4), (start, end))

  def test_space_in_single_line_definition(self):
    deps_content = """
deps = {
    'src/foo':
 
        'url/to/foo',
    'src/bar': 'url/to/bar',
}
"""
    with patch('builtins.open', mock_open(read_data=deps_content)):
      start, end = common.get_field_def_pos('dummy_deps', 'src/foo')
      self.assertEqual((2, 5), (start, end))

  def test_multi_line_definition(self):
    deps_content = """
deps = {
    'src/foo': {
        'url': 'url/to/foo',
        'condition': 'checkout_foo',
    },
    'src/bar': 'url/to/bar',
}
"""
    with patch('builtins.open', mock_open(read_data=deps_content)):
      start, end = common.get_field_def_pos('dummy_deps', 'src/foo')
      self.assertEqual((2, 6), (start, end))

  def test_multi_line_definition_with_comment(self):
    deps_content = """
deps = {
    'src/foo': {
        'url': 'url/to/foo',
        # This is a comment inside the dictionary
        'condition': 'checkout_foo',
    },
    'src/bar': 'url/to/bar',
}
"""
    with patch('builtins.open', mock_open(read_data=deps_content)):
      start, end = common.get_field_def_pos('dummy_deps', 'src/foo')
      self.assertEqual((2, 7), (start, end))

  def test_commented_out_module(self):
    deps_content = """
deps = {
    # 'src/foo': 'url/to/foo',
    'src/bar': 'url/to/bar',
}
"""
    with patch('builtins.open', mock_open(read_data=deps_content)):
      with self.assertRaisesRegex(ValueError,
                                  'Could not find module "src/foo"'):
        common.get_field_def_pos('dummy_deps', 'src/foo')

  def test_module_not_found(self):
    deps_content = """
deps = {
    'src/bar': 'url/to/bar',
}
"""
    with patch('builtins.open', mock_open(read_data=deps_content)):
      with self.assertRaisesRegex(ValueError,
                                  'Could not find module "src/foo"'):
        common.get_field_def_pos('dummy_deps', 'src/foo')

  def test_definition_at_end_of_file_multiline(self):
    deps_content = """
deps = {
    'src/bar': 'url/to/bar',
    'src/foo': {
        'url': 'url/to/foo',
    }
}
"""
    with patch('builtins.open', mock_open(read_data=deps_content)):
      start, end = common.get_field_def_pos('dummy_deps', 'src/foo')
      self.assertEqual((3, 6), (start, end))

  def test_definition_at_end_of_file_singleline(self):
    deps_content = """
deps = {
    'src/bar': 'url/to/bar',
    'src/foo': 'url/to/foo'
}
"""
    with patch('builtins.open', mock_open(read_data=deps_content)):
      start, end = common.get_field_def_pos('dummy_deps', 'src/foo')
      self.assertEqual((3, 4), (start, end))


if __name__ == '__main__':
  unittest.main()
