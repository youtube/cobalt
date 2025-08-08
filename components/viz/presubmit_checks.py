# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit checks used in viz"""

import re

def CheckChangeLintsClean(input_api, output_api, allowlist, denylist=None):
  source_filter = lambda x: input_api.FilterSourceFile(x, allowlist, denylist)

  return input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api, source_filter, lint_filters=[], verbose_level=1)

def CheckAsserts(input_api, output_api, allowlist, denylist=None):
  denylist = tuple(denylist or input_api.DEFAULT_FILES_TO_SKIP)
  source_file_filter = lambda x: input_api.FilterSourceFile(x, allowlist, denylist)

  assert_files = []

  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    # WebKit ASSERT() is not allowed.
    if re.search(r"\bASSERT\(", contents):
      assert_files.append(f.LocalPath())

  if assert_files:
    return [output_api.PresubmitError(
      'These files use ASSERT instead of using DCHECK:',
      items=assert_files)]
  return []

def CheckStdAbs(input_api, output_api,
                allowlist, denylist=None):
  denylist = tuple(denylist or input_api.DEFAULT_FILES_TO_SKIP)
  source_file_filter = lambda x: input_api.FilterSourceFile(x,
                                                            allowlist,
                                                            denylist)

  using_std_abs_files = []
  found_fabs_files = []
  missing_std_prefix_files = []

  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    if re.search(r"using std::f?abs;", contents):
      using_std_abs_files.append(f.LocalPath())
    if re.search(r"\bfabsf?\(", contents):
      found_fabs_files.append(f.LocalPath());

    no_std_prefix = r"(?<!std::)"
    # Matches occurrences of abs/absf/fabs/fabsf without a "std::" prefix.
    abs_without_prefix = r"%s(\babsf?\()" % no_std_prefix
    fabs_without_prefix = r"%s(\bfabsf?\()" % no_std_prefix
    # Skips matching any lines that have "// NOLINT".
    no_nolint = r"(?![^\n]*//\s+NOLINT)"

    expression = re.compile("(%s|%s)%s" %
        (abs_without_prefix, fabs_without_prefix, no_nolint))
    if expression.search(contents):
      missing_std_prefix_files.append(f.LocalPath())

  result = []
  if using_std_abs_files:
    result.append(output_api.PresubmitError(
        'These files have "using std::abs" which is not permitted.',
        items=using_std_abs_files))
  if found_fabs_files:
    result.append(output_api.PresubmitError(
        'std::abs() should be used instead of std::fabs() for consistency.',
        items=found_fabs_files))
  if missing_std_prefix_files:
    result.append(output_api.PresubmitError(
        'These files use abs(), absf(), fabs(), or fabsf() without qualifying '
        'the std namespace. Please use std::abs() in all places.',
        items=missing_std_prefix_files))
  return result

def CheckPassByValue(input_api,
                     output_api,
                     allowlist,
                     denylist=None):
  denylist = tuple(denylist or input_api.DEFAULT_FILES_TO_SKIP)
  source_file_filter = lambda x: input_api.FilterSourceFile(x,
                                                            allowlist,
                                                            denylist)

  local_errors = []

  # Well-defined simple classes the same size as a primitive type.
  pass_by_value_types = ['base::Time',
                         'base::TimeTicks',
                         ]

  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    match = re.search(
      r'\bconst +' + '(?P<type>(%s))&' %
        '|'.join(pass_by_value_types),
      contents)
    if match:
      local_errors.append(output_api.PresubmitError(
        '%s passes %s by const ref instead of by value.' %
        (f.LocalPath(), match.group('type'))))
  return local_errors

def CheckTodos(input_api, output_api):
  errors = []

  source_file_filter = lambda x: x
  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    if ('FIX'+'ME') in contents:
      errors.append(f.LocalPath())

  if errors:
    return [output_api.PresubmitError(
      'All TODO comments should be of the form TODO(name/bug). ' +
      'Use TODO instead of FIX' + 'ME',
      items=errors)]
  return []

def CheckDoubleAngles(input_api, output_api, allowlist,
                      denylist=None):
  errors = []

  source_file_filter = lambda x: input_api.FilterSourceFile(x,
                                                            allowlist,
                                                            denylist)
  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    if ('> >') in contents:
      errors.append(f.LocalPath())

  if errors:
    return [output_api.PresubmitError('Use >> instead of > >:', items=errors)]
  return []

def FindUnquotedQuote(contents, pos):
  match = re.search(r"(?<!\\)(?P<quote>\")", contents[pos:])
  return -1 if not match else match.start("quote") + pos

def FindUselessIfdefs(input_api, output_api):
  errors = []
  source_file_filter = lambda x: x
  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    if re.search(r'#if\s*0\s', contents):
      errors.append(f.LocalPath())
  if errors:
    return [output_api.PresubmitError(
      'Don\'t use #if '+'0; just delete the code.',
      items=errors)]
  return []

def FindNamespaceInBlock(pos, namespace, contents, allowlist=[]):
  open_brace = -1
  close_brace = -1
  quote = -1
  name = -1
  brace_count = 1
  quote_count = 0
  while pos < len(contents) and brace_count > 0:
    if open_brace < pos: open_brace = contents.find("{", pos)
    if close_brace < pos: close_brace = contents.find("}", pos)
    if quote < pos: quote = FindUnquotedQuote(contents, pos)
    if name < pos: name = contents.find(("%s::" % namespace), pos)

    if name < 0:
      return False # The namespace is not used at all.
    if open_brace < 0:
      open_brace = len(contents)
    if close_brace < 0:
      close_brace = len(contents)
    if quote < 0:
      quote = len(contents)

    next = min(open_brace, min(close_brace, min(quote, name)))

    if next == open_brace:
      brace_count += 1
    elif next == close_brace:
      brace_count -= 1
    elif next == quote:
      quote_count = 0 if quote_count else 1
    elif next == name and not quote_count:
      in_allowlist = False
      for w in allowlist:
        if re.match(w, contents[next:]):
          in_allowlist = True
          break
      if not in_allowlist:
        return True
    pos = next + 1
  return False

# Checks for the use of viz:: within the viz namespace, which is usually
# redundant.
def CheckNamespace(input_api, output_api):
  errors = []

  source_file_filter = lambda x: x
  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    match = re.search(r'namespace\s*viz\s*{', contents)
    if match:
      allowlist = []
      if FindNamespaceInBlock(match.end(), 'viz', contents, allowlist=allowlist):
        errors.append(f.LocalPath())

  if errors:
    return [output_api.PresubmitError(
      'Do not use viz:: inside of the viz namespace.',
      items=errors)]
  return []

# Verifies that we use the right module name (viz.mojom) in mojom files and we
# don't specify module name viz.mojom when referring to types in viz.mojom.
def CheckMojoms(input_api, output_api):
  source_file_filter = lambda x: input_api.FilterSourceFile(x,
                                                            ['.*\.mojom$'],
                                                            [])
  wrong_module_name=[]
  omit_module_name=[]
  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    if 'module viz.mojom;' not in contents:
      wrong_module_name.append(f.LocalPath())
    elif 'viz.mojom.' in contents:
      omit_module_name.append(f.LocalPath())

  errors=[]
  if wrong_module_name:
    errors.append(output_api.PresubmitError(
        'Use viz.mojom as the module name in mojom files.',
        items=wrong_module_name))
  if omit_module_name:
    errors.append(output_api.PresubmitError(
      'Do not specify module name viz.mojom when referring to types '
      + 'in the same module.', items=omit_module_name))
  return errors

def CheckForUseOfWrongClock(input_api,
                            output_api,
                            allowlist,
                            denylist=None):
  """Make sure new lines of code don't use a clock susceptible to skew."""
  denylist = tuple(denylist or input_api.DEFAULT_FILES_TO_SKIP)
  source_file_filter = lambda x: input_api.FilterSourceFile(x,
                                                            allowlist,
                                                            denylist)
  # Regular expression that should detect any explicit references to the
  # base::Time type (or base::Clock/DefaultClock), whether in using decls,
  # typedefs, or to call static methods.
  base_time_type_pattern = r'(^|\W)base::(Time|Clock|DefaultClock)(\W|$)'

  # Regular expression that should detect references to the base::Time class
  # members, such as a call to base::Time::Now.
  base_time_member_pattern = r'(^|\W)(Time|Clock|DefaultClock)::'

  # Regular expression to detect "using base::Time" declarations.  We want to
  # prevent these from triggerring a warning.  For example, it's perfectly
  # reasonable for code to be written like this:
  #
  #   using base::Time;
  #   ...
  #   int64 foo_us = foo_s * Time::kMicrosecondsPerSecond;
  using_base_time_decl_pattern = r'^\s*using\s+(::)?base::Time\s*;'

  # Regular expression to detect references to the kXXX constants in the
  # base::Time class.  We want to prevent these from triggerring a warning.
  base_time_konstant_pattern = r'(^|\W)Time::k\w+'

  problem_re = input_api.re.compile(
      r'(' + base_time_type_pattern + r')|(' + base_time_member_pattern + r')')
  exception_re = input_api.re.compile(
      r'(' + using_base_time_decl_pattern + r')|(' +
      base_time_konstant_pattern + r')')
  problems = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    for line_number, line in f.ChangedContents():
      if problem_re.search(line):
        if not exception_re.search(line):
          problems.append(
              '  %s:%d\n    %s' % (f.LocalPath(), line_number, line.strip()))

  if problems:
    return [output_api.PresubmitPromptOrNotify(
        'You added one or more references to the base::Time class and/or one\n'
        'of its member functions (or base::Clock/DefaultClock). In cc code,\n'
        'it is most certainly incorrect! Instead use base::TimeTicks.\n\n'
        '\n'.join(problems))]
  else:
    return []

def RunAllChecks(input_api, output_api, allowlist):
  results = []
  results += CheckAsserts(input_api, output_api, allowlist)
  results += CheckStdAbs(input_api, output_api, allowlist)
  results += CheckPassByValue(input_api, output_api, allowlist)
  results += CheckChangeLintsClean(input_api, output_api, allowlist)
  results += CheckTodos(input_api, output_api)
  results += CheckDoubleAngles(input_api, output_api, allowlist)
  results += CheckNamespace(input_api, output_api)
  results += CheckMojoms(input_api, output_api)
  results += CheckForUseOfWrongClock(input_api, output_api, allowlist)
  results += FindUselessIfdefs(input_api, output_api)
  return results
