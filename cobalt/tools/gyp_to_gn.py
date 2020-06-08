#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

"""Tries to translate a GYP file into a GN file.

Output from this script should be piped to ``gn format --stdin``. It most likely
needs to be manually fixed after that as well.
"""

import argparse
import ast
import collections
import itertools
import os
import os.path
import re
import sys

sys.path.append(
    os.path.abspath(
        os.path.join(__file__, os.pardir, os.pardir, os.pardir)))
import cobalt.tools.paths  # pylint: disable=g-import-not-at-top

VariableRewrite = collections.namedtuple('VariableRewrite',
                                         ['name', 'value_rewrites'])

# The directory containing the input file, as a repository-absolute (//foo/bar)
# path
repo_abs_input_file_dir = ''
deps_substitutions = {}
variable_rewrites = {}


class GNException(Exception):
  pass


class GYPCondToGNNodeVisitor(ast.NodeVisitor):
  """An AST NodeVisitor which translates GYP conditions to GN strings.

  Given a GYP condition as an AST with mode eval, outputs a string containing
  the GN equivalent of that condition. Simplifies conditions involving the
  variables OS and os_posix, and performs variable substitutions.

  Example:
  (Assume arm_neon is renamed to arm_use_neon and converted from 0/1 to
  true/false):

  >>> g = GYPCondToGNNodeVisitor()
  >>> g.visit(ast.parse('arm_neon and target_arch=="raspi-2"', mode='eval'))
  '(arm_use_neon && target_cpu == "raspi-2")'
  >>> g.visit(ast.parse('use_system_libjpeg and target_arch=="raspi-2"',
  ...                   mode='eval'))
  '(use_system_libjpeg && target_cpu == "raspi-2")'
  >>> g.visit(ast.parse('arm_neon == 1', mode='eval'))
  'arm_use_neon == true'
  >>> g.visit(ast.parse('1', mode='eval'))
  'true'
  >>> g.visit(ast.parse('0', mode='eval'))
  'false'
  >>> g.visit(ast.parse('arm_neon != 0 and target_arch != "raspi-2" and
  use_system_libjpeg or enable_doom_melon', mode='eval'))
  '((arm_use_neon != false && target_cpu != "raspi-2" && use_system_libjpeg) ||
  enable_doom_melon)'
  >>> g.visit(ast.parse('arm_neon != 0 and target_arch != "raspi-2" or
  use_system_libjpeg and enable_doom_melon', mode='eval'))
  '((arm_use_neon != false && target_cpu != "raspi-2") || (use_system_libjpeg &&
  enable_doom_melon))'
  """

  def visit_Expression(self, expr):  # pylint: disable=invalid-name
    return self.visit(expr.body)

  def visit_Num(self, num):  # pylint: disable=invalid-name
    # A number that doesn't occur inside a Compare is taken in boolean context
    return GYPValueToGNString(bool(num.n))

  def visit_Str(self, string):  # pylint: disable=invalid-name
    return GYPValueToGNString(string.s)

  def visit_Name(self, name):  # pylint: disable=invalid-name
    if name.id in variable_rewrites:
      return variable_rewrites[name.id].name
    else:
      return name.id

  def visit_BoolOp(self, boolop):  # pylint: disable=invalid-name
    glue = ' && ' if isinstance(boolop.op, ast.And) else ' || '
    return '(' + glue.join(itertools.imap(self.visit, boolop.values)) + ')'

  def visit_Compare(self, compare):  # pylint: disable=invalid-name
    if len(compare.ops) != 1:
      raise GNException("This script doesn't support multiple operators in "
                        'comparisons')

    if isinstance(compare.ops[0], ast.Eq):
      op = ' == '
    elif isinstance(compare.ops[0], ast.NotEq):
      op = ' != '
    else:
      raise GNException('Operator ' + str(compare.ops[0]) + ' not supported')

    if isinstance(compare.left, ast.Name):
      if isinstance(compare.comparators[0], ast.Name):  # var1 == var2
        left = self.visit_Name(compare.left)
        right = self.visit_Name(compare.comparators[0])
      elif isinstance(compare.comparators[0], ast.Num):  # var1 == 42
        left, right = TransformVariable(compare.left.id,
                                        compare.comparators[0].n)
      elif isinstance(compare.comparators[0], ast.Str):  # var1 == "some string"
        left, right = TransformVariable(compare.left.id,
                                        compare.comparators[0].s)
      else:
        raise GNException('Unknown RHS type ' + str(compare.comparators[0]))
    else:
      raise GNException('Non-variables on LHS of comparison are not supported')

    if right == 'true' and op == ' == ' or right == 'false' and op == ' != ':
      return left
    elif right == 'false' and op == ' == ' or right == 'true' and op == ' != ':
      return '!(' + left + ')'
    else:
      return left + op + right

  def generic_visit(self, node):
    raise GNException("I don't know how to convert node " + str(node))


class OSComparisonRewriter(ast.NodeTransformer):

  def visit_Compare(self, compare):  # pylint: disable=invalid-name
    """Substitute instances of comparisons involving the OS and os_posix
    variables with their known values in Compare nodes.

    Examples:
      ``OS == "starboard"`` -> ``True``
      ``OS != "starboard"`` -> ``False``
      ``OS == "linux"`` -> ``False``
      ``os_posix == 1`` -> ``False``
      ``os_bsd == 1`` -> ``False``
    """
    if len(compare.ops) != 1:
      raise GNException("This script doesn't support multiple operators in "
                        'comparisons')

    if not isinstance(compare.left, ast.Name):
      return compare

    elif compare.left.id == 'OS':
      if (isinstance(compare.comparators[0], ast.Str) and
          compare.comparators[0].s == 'starboard'):
        # OS == "starboard" -> True, OS != "starboard" -> False
        new_node = ast.Num(1 if isinstance(compare.ops[0], ast.Eq) else 0)
      else:
        # OS == "something else" -> False, OS != "something else" -> True
        new_node = ast.Num(0 if isinstance(compare.ops[0], ast.Eq) else 1)

      return ast.copy_location(new_node, compare)

    elif compare.left.id in {'os_posix', 'os_bsd'}:
      if (isinstance(compare.comparators[0], ast.Num) and
          compare.comparators[0].n == 0):
        # os_posix == 0 -> True, os_posix != 0 -> False
        # ditto for os_bsd
        new_node = ast.Num(1 if isinstance(compare.ops[0], ast.Eq) else 0)
      else:
        # os_posix == (not 0) -> False, os_posix != (not 0) -> True
        # ditto for os_bsd
        new_node = ast.Num(0 if isinstance(compare.ops[0], ast.Eq) else 1)

      return ast.copy_location(new_node, compare)

    else:
      return compare

  def visit_BoolOp(self, boolop):  # pylint: disable=invalid-name
    """Simplify BoolOp nodes by weeding out Falses and Trues resulting from
    the elimination of OS comparisons.
    """
    doing_and = isinstance(boolop.op, ast.And)
    new_values = map(self.visit, boolop.values)
    new_values_filtered = []

    for v in new_values:
      if isinstance(v, ast.Num):
        # "x and False", or "y or True" - short circuit
        if (doing_and and v.n == 0) or (not doing_and and v.n == 1):
          new_values_filtered = None
          break
        # "x and True", or "y or False" - skip the redundant value
        elif (doing_and and v.n == 1) or (not doing_and and v.n == 0):
          pass
        else:
          new_values_filtered.append(v)
      else:
        new_values_filtered.append(v)

    if new_values_filtered is None:
      new_node = ast.Num(0 if doing_and else 1)
    elif len(new_values_filtered) == 0:
      new_node = ast.Num(1 if doing_and else 0)
    elif len(new_values_filtered) == 1:
      new_node = new_values_filtered[0]
    else:
      new_node = ast.BoolOp(boolop.op, new_values_filtered)

    return ast.copy_location(new_node, boolop)


def Warn(msg):
  print >> sys.stderr, '\x1b[1;33mWarning:\x1b[0m', msg


def WarnAboutRemainingKeys(dic):
  for key in dic:
    Warn("I don't know what {} is".format(key))


def TransformVariable(var, value):
  """Given a variable and value, substitutes the variable name/value with
  the new name/new value from the variable_rewrites dict, if applicable.

  Example:
    ``('arm_neon', 1)`` -> ``('arm_use_neon', 'true')``
    ``('gl_type', 'none')`` -> ``('gl_type', 'none')``
  """
  if var in variable_rewrites:
    var, value_rewrites = variable_rewrites[var]
    if value_rewrites is not None:
      value = value_rewrites[value]

  return var, GYPValueToGNString(value)


def GYPValueToGNString(value, allow_dicts=True):
  """Returns a stringified GN equivalent of the Python value.

  allow_dicts indicates if this function will allow converting dictionaries
  to GN scopes. This is only possible at the top level, you can't nest a
  GN scope in a list, so this should be set to False for recursive calls."""

  if isinstance(value, str):
    if value.find('\n') >= 0:
      raise GNException("GN strings don't support newlines")
    # Escape characters
    ret = value.replace('\\', r'\\').replace('"', r'\"') \
               .replace('$', r'\$')
    # Convert variable substitutions
    ret = re.sub(r'[<>]\((\w+)\)', r'$\1', ret)
    return '"' + ret + '"'

  if isinstance(value, unicode):
    if value.find(u'\n') >= 0:
      raise GNException("GN strings don't support newlines")
    # Escape characters
    ret = value.replace(u'\\', ur'\\').replace(u'"', ur'\"') \
               .replace(u'$', ur'\$')
    # Convert variable substitutions
    ret = re.sub(ur'[<>]\((\w+)\)', ur'$\1', ret)
    return (u'"' + ret + u'"').encode('utf-8')

  if isinstance(value, bool):
    if value:
      return 'true'
    return 'false'

  if isinstance(value, list):
    return '[ %s ]' % ', '.join(GYPValueToGNString(v) for v in value)

  if isinstance(value, dict):
    if not allow_dicts:
      raise GNException('Attempting to recursively print a dictionary.')
    result = ''
    for key in sorted(value):
      if not isinstance(key, basestring):
        raise GNException('Dictionary key is not a string.')
      result += '%s = %s\n' % (key, GYPValueToGNString(value[key], False))
    return result

  if isinstance(value, int):
    return str(value)

  raise GNException('Unsupported type when printing to GN.')


def GYPTargetToGNString(target_dict):
  """Given a target dict, output the GN equivalent."""

  target_header_text = ''

  target_name = target_dict.pop('target_name')
  target_type = target_dict.pop('type')
  if target_type in {'executable', 'shared_library', 'static_library'}:
    if target_type == 'static_library':
      Warn('converting static library, check to see if it should be a '
           'source_set')
    target_header_text += '{}("{}") {{\n'.format(target_type, target_name)
  elif target_type == 'none':
    target_header_text += 'group("{}") {{\n'.format(target_name)
  elif target_type == '<(gtest_target_type)':
    target_header_text += 'test("{}") {{\n'.format(target_name)
  elif target_type == '<(final_executable_type)':
    target_header_text += 'final_executable("{}") {{\n'.format(target_name)
  elif target_type == '<(component)' or target_type == '<(library)':
    Warn('converting static library, check to see if it should be a '
         'source_set')
    target_header_text += 'static_library("{}") {{\n'.format(target_name)
  else:
    raise GNException("I don't understand target type {}".format(target_type))

  target_body_text, configs_text = \
      GYPTargetToGNString_Inner(target_dict, target_name)

  return configs_text + target_header_text + target_body_text + '}\n\n'


def ProcessIncludeExclude(dic, param):
  """Translate dic[param] and dic[param + '!'] lists to GN.

  Example input:
    {
      'sources': [ 'foo.cc', 'bar.cc', 'baz.cc' ],
      'sources!': [ 'bar.cc' ]
    }
  Example output:
    sources = [ 'foo.cc', 'bar.cc', 'baz.cc' ]
    sources -= [ 'bar.cc' ]
  """
  ret = ''
  if param in dic:
    value = dic.pop(param)
    ret += '{} = [ {} ]\n'.format(param, ', '.join(
        GYPValueToGNString(s) for s in value))
  if param + '!' in dic:
    value = dic.pop(param + '!')
    ret += '{} -= [ {} ]\n'.format(param, ', '.join(
        GYPValueToGNString(s) for s in value))
  ret += '\n'
  return ret


def ProcessDependentSettings(dependent_settings_dict, target_dict, param,
                             renamed_param, config_name):
  """Translates direct_dependent_settings and all_dependent_settings blocks
  to their GN equivalents. This is done by creating a new GN config, putting
  the settings in that config, and adding the config to the target's
  public_configs/all_dependent_configs. Returns a tuple of
  (target_text, config_text).

  Also eliminates the translated settings from the target_dict so they aren't
  translated twice.

  Example input:
    {
      'target_name': 'abc',
      'direct_dependent_settings': {
        'defines': [ "FOO" ]
      }
    }

  Example target text output:
    public_configs = [ ":abc_direct_config" ]

  Example config text output:
    config("abc_direct_config") {
      defines = [ "FOO" ]
    }
  """
  ret_target = ''
  ret_config = 'config("{}") {{\n'.format(config_name)

  def FilterList(key):
    if key in target_dict and key in dependent_settings_dict:
      filtered_list = sorted(
          set(target_dict[key]) - set(dependent_settings_dict[key]))
      if filtered_list:
        target_dict[key] = filtered_list
      else:
        del target_dict[key]

  for inner_param in [
      'include_dirs', 'defines', 'asmflags', 'cflags', 'cflags_c', 'cflags_cc',
      'cflags_objc', 'cflags_objcc', 'ldflags'
  ]:
    FilterList(inner_param)
    FilterList(inner_param + '!')
    ret_config += ProcessIncludeExclude(dependent_settings_dict, inner_param)

  if 'variables' in dependent_settings_dict:
    Warn("variables block inside {}. You'll need to handle that manually."
         .format(param))
    del dependent_settings_dict['variables']

  if 'conditions' in dependent_settings_dict:
    for i, cond_block in enumerate(dependent_settings_dict['conditions']):
      cond_config_name = '{}_{}'.format(config_name, i)
      t, c = GYPConditionToGNString(cond_block,
               lambda dsd: ProcessDependentSettings(dsd, target_dict, param,
                                                    renamed_param,
                                                    cond_config_name))
      ret_config += c
      ret_target += t
    del dependent_settings_dict['conditions']

  if 'target_conditions' in dependent_settings_dict:
    for i, cond_block in \
        enumerate(dependent_settings_dict['target_conditions']):
      cond_config_name = '{}_t{}'.format(config_name, i)
      t, c = GYPConditionToGNString(cond_block,
               lambda dsd: ProcessDependentSettings(dsd, target_dict, param,
                                                    renamed_param,
                                                    cond_config_name))
      ret_config += c
      ret_target += t
    del dependent_settings_dict['target_conditions']

  ret_config += '}\n\n'
  ret_target += '{} = [ ":{}" ]\n\n'.format(renamed_param, config_name)
  WarnAboutRemainingKeys(dependent_settings_dict)
  return ret_target, ret_config


def GYPTargetToGNString_Inner(target_dict, target_name):
  """Translates the body of a GYP target into a GN string."""
  configs_text = ''
  target_text = ''
  dependent_text = ''

  if 'variables' in target_dict:
    target_text += GYPVariablesToGNString(target_dict['variables'])
    del target_dict['variables']

  target_text += ProcessIncludeExclude(target_dict, 'sources')

  for param, renamed_param, config_name_elem in \
      [('direct_dependent_settings', 'public_configs', 'direct'),
       ('all_dependent_settings', 'all_dependent_configs', 'dependent')]:
    if param in target_dict:
      config_name = '{}_{}_config'.format(target_name, config_name_elem)
      # Append dependent_text to target_text after include_dirs/defines/etc.
      dependent_text, c = ProcessDependentSettings(
          target_dict[param], target_dict, param, renamed_param, config_name)
      configs_text += c
      del target_dict[param]

  for param in [
      'include_dirs', 'defines', 'asmflags', 'cflags', 'cflags_c', 'cflags_cc',
      'cflags_objc', 'cflags_objcc', 'ldflags'
  ]:
    target_text += ProcessIncludeExclude(target_dict, param)

  target_text += dependent_text

  if 'export_dependent_settings' in target_dict:
    target_text += GYPDependenciesToGNString(
        'public_deps', target_dict['export_dependent_settings'])

    # Remove dependencies covered here from the ordinary dependencies list
    target_dict['dependencies'] = sorted(
        set(target_dict['dependencies']) -
        set(target_dict['export_dependent_settings']))
    if not target_dict['dependencies']:
      del target_dict['dependencies']

    del target_dict['export_dependent_settings']

  if 'dependencies' in target_dict:
    target_text += GYPDependenciesToGNString('deps',
                                             target_dict['dependencies'])
    del target_dict['dependencies']

  if 'conditions' in target_dict:
    for cond_block in target_dict['conditions']:
      t, c = GYPConditionToGNString(
          cond_block, lambda td: GYPTargetToGNString_Inner(td, target_name))
      configs_text += c
      target_text += t
    del target_dict['conditions']

  if 'target_conditions' in target_dict:
    for cond_block in target_dict['target_conditions']:
      t, c = GYPConditionToGNString(
          cond_block, lambda td: GYPTargetToGNString_Inner(td, target_name))
      configs_text += c
      target_text += t
    del target_dict['target_conditions']

  WarnAboutRemainingKeys(target_dict)
  return target_text, configs_text


def GYPDependenciesToGNString(param, dep_list):
  r"""Translates a GYP dependency into a GN dependency. Tries to intelligently
  perform target renames as according to GN style conventions.

  Examples:
  (Note that <(DEPTH) has already been translated into // by the time this
  function is called)
  >>> GYPDependenciesToGNString('deps', ['//cobalt/math/math.gyp:math'])
  'deps = [ "//cobalt/math" ]\n\n'
  >>> GYPDependenciesToGNString('public_deps',
  ...                           ['//testing/gtest.gyp:gtest'])
  'public_deps = [ "//testing/gtest" ]\n\n'
  >>> GYPDependenciesToGNString('deps', ['//third_party/icu/icu.gyp:icui18n'])
  'deps = [ "//third_party/icu:icui18n" ]\n\n'
  >>> GYPDependenciesToGNString('deps',
  ...     ['//cobalt/browser/browser_bindings_gen.gyp:generated_types'])
  'deps = [ "//cobalt/browser/browser_bindings_gen:generated_types" ]\n\n'
  >>> GYPDependenciesToGNString('deps', ['allocator'])
  'deps = [ ":allocator" ]\n\n'
  >>> # Suppose the input file is in //cobalt/foo/bar/
  >>> GYPDependenciesToGNString('deps', ['../baz.gyp:quux'])
  'deps = [ "//cobalt/foo/baz:quux" ]\n\n'
  """
  new_dep_list = []
  for dep in dep_list:
    if dep in deps_substitutions:
      new_dep_list.append(deps_substitutions[dep])
    else:
      m1 = re.match(r'(.*/)?(\w+)/\2\.gyp:\2$', dep)
      m2 = re.match(r'(.*/)?(\w+)\.gyp:\2$', dep)
      m3 = re.match(r'(.*/)?(\w+)/\2.gyp:(\w+)$', dep)
      m4 = re.match(r'(.*)\.gyp:(\w+)$', dep)
      m5 = re.match(r'\w+', dep)
      if m1:
        new_dep = '{}{}'.format(m1.group(1) or '', m1.group(2))
      elif m2:
        new_dep = '{}{}'.format(m2.group(1) or '', m2.group(2))
      elif m3:
        new_dep = '{}{}:{}'.format(m3.group(1) or '', m3.group(2), m3.group(3))
      elif m4:
        new_dep = '{}:{}'.format(m4.group(1), m4.group(2))
      elif m5:
        new_dep = ':' + dep
      else:
        Warn("I don't know how to translate dependency " + dep)
        continue

      if not (new_dep.startswith('//') or new_dep.startswith(':') or
              os.path.isabs(new_dep)):
        # Rebase new_dep to be repository-absolute
        new_dep = os.path.normpath(repo_abs_input_file_dir + new_dep)

      new_dep_list.append(new_dep)

  quoted_deps = ['"{}"'.format(d) for d in new_dep_list]
  return '{} = [ {} ]\n\n'.format(param, ', '.join(quoted_deps))


def GYPVariablesToGNString(varblock):
  """Translates a GYP variables block into its GN equivalent, performing
  variable substitutions as necessary."""
  ret = ''

  if 'variables' in varblock:
    # Recursively flatten nested variables dicts.
    ret += GYPVariablesToGNString(varblock['variables'])

  for k, v in varblock.viewitems():
    if k in {'variables', 'conditions'}:
      continue

    if k.endswith('%'):
      k = k[:-1]

    if not k.endswith('!'):
      ret += '{} = {}\n'.format(*TransformVariable(k, v))
    else:
      ret += '{} -= {}\n'.format(*TransformVariable(k[:-1], v))

  if 'conditions' in varblock:
    for cond_block in varblock['conditions']:
      ret += GYPConditionToGNString(cond_block, GYPVariablesToGNString)[0]

  ret += '\n'
  return ret


def GYPConditionToGNString(cond_block, recursive_translate):
  """Translates a GYP condition block into a GN string. The recursive_translate
  param is a function that is called with the true subdict and the false
  subdict if applicable. The recursive_translate function returns either a
  single string that should go inside the if/else block, or a tuple of
  (target_text, config_text), in which the target_text goes inside the if/else
  block and the config_text goes outside.

  Returns a tuple (target_text, config_text), where config_text is the empty
  string if recursive_translate only returns one string.
  """
  cond = cond_block[0]
  iftrue = cond_block[1]
  iffalse = cond_block[2] if len(cond_block) == 3 else None

  ast_cond = ast.parse(cond, mode='eval')
  ast_cond = OSComparisonRewriter().visit(ast_cond)
  translated_cond = GYPCondToGNNodeVisitor().visit(ast_cond)

  if translated_cond == 'false' and iffalse is None:
    # if (false) { ... } -> nothing
    # Special cased to avoid printing warnings from the unnecessary translation
    # of the true clause
    return '', ''

  translated_iftrue = recursive_translate(iftrue)
  if isinstance(translated_iftrue, tuple):
    iftrue_text, aux_iftrue_text = translated_iftrue
  else:
    iftrue_text, aux_iftrue_text = translated_iftrue, ''

  if translated_cond == 'true':  # Tautology - just return the body
    return iftrue_text, aux_iftrue_text

  elif iffalse is None:  # Non-tautology, non-contradiction, no else clause
    return ('if (' + translated_cond + ') {\n' + iftrue_text + '}\n\n',
            aux_iftrue_text)

  else:  # Non-tautology, else clause present
    translated_iffalse = recursive_translate(iffalse)
    if isinstance(translated_iffalse, tuple):
      iffalse_text, aux_iffalse_text = translated_iffalse
    else:
      iffalse_text, aux_iffalse_text = translated_iffalse, ''

    if translated_cond == 'false':  # if (false) { blah } else { ... } -> ...
      return iffalse_text, aux_iffalse_text

    else:
      return ('if (' + translated_cond + ') {\n' + iftrue_text + '} else {\n' +
              iffalse_text + '}\n\n', aux_iftrue_text + aux_iffalse_text)


def ToplevelGYPToGNString(toplevel_dict):
  """Translates a toplevel GYP dict to GN. This is the main function which is
  called to perform the GYP to GN translation.
  """
  ret = ''

  if 'variables' in toplevel_dict:
    ret += GYPVariablesToGNString(toplevel_dict['variables'])
    del toplevel_dict['variables']

  if 'targets' in toplevel_dict:
    for t in toplevel_dict['targets']:
      ret += GYPTargetToGNString(t)
    del toplevel_dict['targets']

  if 'conditions' in toplevel_dict:
    for cond_block in toplevel_dict['conditions']:
      ret += GYPConditionToGNString(cond_block, ToplevelGYPToGNString)[0]
    del toplevel_dict['conditions']

  if 'target_conditions' in toplevel_dict:
    for cond_block in toplevel_dict['target_conditions']:
      ret += GYPConditionToGNString(cond_block, ToplevelGYPToGNString)[0]
    del toplevel_dict['target_conditions']

  WarnAboutRemainingKeys(toplevel_dict)
  return ret


def LoadPythonDictionary(path):
  with open(path, 'r') as fin:
    file_string = fin.read()
  try:
    file_data = eval(file_string, {'__builtins__': None}, None)  # pylint: disable=eval-used
  except SyntaxError, e:
    e.filename = path
    raise
  except Exception, e:
    raise Exception('Unexpected error while reading %s: %s' % (path, str(e)))

  assert isinstance(file_data, dict), '%s does not eval to a dictionary' % path

  return file_data


def ReplaceSubstrings(values, search_for, replace_with):
  """Recursively replaces substrings in a value.

  Replaces all substrings of the "search_for" with "replace_with" for all
  strings occurring in "values". This is done by recursively iterating into
  lists as well as the keys and values of dictionaries.
  """
  if isinstance(values, str):
    return values.replace(search_for, replace_with)

  if isinstance(values, list):
    return [ReplaceSubstrings(v, search_for, replace_with) for v in values]

  if isinstance(values, dict):
    # For dictionaries, do the search for both the key and values.
    result = {}
    for key, value in values.viewitems():
      new_key = ReplaceSubstrings(key, search_for, replace_with)
      new_value = ReplaceSubstrings(value, search_for, replace_with)
      result[new_key] = new_value
    return result

  # Assume everything else is unchanged.
  return values


def CalculatePaths(filename):
  global repo_abs_input_file_dir

  abs_input_file_dir = os.path.abspath(os.path.dirname(filename))
  abs_repo_root = cobalt.tools.paths.REPOSITORY_ROOT + '/'
  if not abs_input_file_dir.startswith(abs_repo_root):
    raise ValueError('Input file is not in repository')

  repo_abs_input_file_dir = '//' + abs_input_file_dir[len(abs_repo_root):] + '/'


def LoadDepsSubstitutions():
  dirname = os.path.dirname(__file__)
  if dirname:
    dirname += '/'

  with open(dirname + 'deps_substitutions.txt', 'r') as f:
    for line in itertools.ifilter(lambda lin: lin.strip(), f):
      dep, new_dep = line.split()
      deps_substitutions[dep.replace('<(DEPTH)/', '//')] = new_dep


def LoadVariableRewrites():
  dirname = os.path.dirname(__file__)
  if dirname:
    dirname += '/'

  vr = LoadPythonDictionary(dirname + 'variable_rewrites.dict')

  if 'rewrites' in vr:
    for old_name, rewrite in vr['rewrites'].viewitems():
      variable_rewrites[old_name] = VariableRewrite(*rewrite)

  if 'renames' in vr:
    for old_name, new_name in vr['renames'].viewitems():
      variable_rewrites[old_name] = VariableRewrite(new_name, None)

  if 'booleans' in vr:
    bool_mapping = {0: False, 1: True}
    for v in vr['booleans']:
      if v in variable_rewrites:
        variable_rewrites[v] = \
            variable_rewrites[v]._replace(value_rewrites=bool_mapping)
      else:
        variable_rewrites[v] = VariableRewrite(v, bool_mapping)


def main():
  parser = argparse.ArgumentParser(description='Convert a subset of GYP to GN.')
  parser.add_argument(
      '-r',
      '--replace',
      action='append',
      help='Replaces substrings. If passed a=b, replaces all '
      'substrs a with b.')
  parser.add_argument('file', help='The GYP file to read.')
  args = parser.parse_args()

  CalculatePaths(args.file)

  data = LoadPythonDictionary(args.file)
  if args.replace:
    # Do replacements for all specified patterns.
    for replace in args.replace:
      split = replace.split('=')
      # Allow 'foo=' to replace with nothing.
      if len(split) == 1:
        split.append('')
      assert len(split) == 2, "Replacement must be of the form 'key=value'."
      data = ReplaceSubstrings(data, split[0], split[1])
  # Also replace <(DEPTH)/ with //
  data = ReplaceSubstrings(data, '<(DEPTH)/', '//')

  LoadDepsSubstitutions()
  LoadVariableRewrites()

  print ToplevelGYPToGNString(data)


if __name__ == '__main__':
  main()
