#!/usr/bin/env python3
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Top-level presubmit script for Skia.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import fnmatch
import os
import re
import subprocess
import sys
import traceback


RELEASE_NOTES_FILE_NAME = 'RELEASE_NOTES.txt'

GOLD_TRYBOT_URL = 'https://gold.skia.org/search?issue='

SERVICE_ACCOUNT_SUFFIX = [
    '@%s.iam.gserviceaccount.com' % project for project in [
        'skia-buildbots.google.com', 'skia-swarming-bots', 'skia-public',
        'skia-corp.google.com', 'chops-service-accounts']]


def _CheckChangeHasEol(input_api, output_api, source_file_filter=None):
  """Checks that files end with at least one \n (LF)."""
  eof_files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    # Check that the file ends in at least one newline character.
    if len(contents) > 1 and contents[-1:] != '\n':
      eof_files.append(f.LocalPath())

  if eof_files:
    return [output_api.PresubmitPromptWarning(
      'These files should end in a newline character:',
      items=eof_files)]
  return []


def _JsonChecks(input_api, output_api):
  """Run checks on any modified json files."""
  failing_files = []
  for affected_file in input_api.AffectedFiles(None):
    affected_file_path = affected_file.LocalPath()
    is_json = affected_file_path.endswith('.json')
    is_metadata = (affected_file_path.startswith('site/') and
                   affected_file_path.endswith('/METADATA'))
    if is_json or is_metadata:
      try:
        input_api.json.load(open(affected_file_path, 'r'))
      except ValueError:
        failing_files.append(affected_file_path)

  results = []
  if failing_files:
    results.append(
        output_api.PresubmitError(
            'The following files contain invalid json:\n%s\n\n' %
                '\n'.join(failing_files)))
  return results


def _IfDefChecks(input_api, output_api):
  """Ensures if/ifdef are not before includes. See skbug/3362 for details."""
  comment_block_start_pattern = re.compile('^\s*\/\*.*$')
  comment_block_middle_pattern = re.compile('^\s+\*.*')
  comment_block_end_pattern = re.compile('^\s+\*\/.*$')
  single_line_comment_pattern = re.compile('^\s*//.*$')
  def is_comment(line):
    return (comment_block_start_pattern.match(line) or
            comment_block_middle_pattern.match(line) or
            comment_block_end_pattern.match(line) or
            single_line_comment_pattern.match(line))

  empty_line_pattern = re.compile('^\s*$')
  def is_empty_line(line):
    return empty_line_pattern.match(line)

  failing_files = []
  for affected_file in input_api.AffectedSourceFiles(None):
    affected_file_path = affected_file.LocalPath()
    if affected_file_path.endswith('.cpp') or affected_file_path.endswith('.h'):
      f = open(affected_file_path)
      for line in f.xreadlines():
        if is_comment(line) or is_empty_line(line):
          continue
        # The below will be the first real line after comments and newlines.
        if line.startswith('#if 0 '):
          pass
        elif line.startswith('#if ') or line.startswith('#ifdef '):
          failing_files.append(affected_file_path)
        break

  results = []
  if failing_files:
    results.append(
        output_api.PresubmitError(
            'The following files have #if or #ifdef before includes:\n%s\n\n'
            'See https://bug.skia.org/3362 for why this should be fixed.' %
                '\n'.join(failing_files)))
  return results


def _CopyrightChecks(input_api, output_api, source_file_filter=None):
  results = []
  year_pattern = r'\d{4}'
  year_range_pattern = r'%s(-%s)?' % (year_pattern, year_pattern)
  years_pattern = r'%s(,%s)*,?' % (year_range_pattern, year_range_pattern)
  copyright_pattern = (
      r'Copyright (\([cC]\) )?%s \w+' % years_pattern)

  for affected_file in input_api.AffectedSourceFiles(source_file_filter):
    if ('third_party/' in affected_file.LocalPath() or
        'tests/sksl/' in affected_file.LocalPath()):
      continue
    contents = input_api.ReadFile(affected_file, 'rb')
    if not re.search(copyright_pattern, contents):
      results.append(output_api.PresubmitError(
          '%s is missing a correct copyright header.' % affected_file))
  return results


def _InfraTests(input_api, output_api):
  """Run the infra tests."""
  results = []
  if not any(f.LocalPath().startswith('infra')
             for f in input_api.AffectedFiles()):
    return results

  cmd = ['python', os.path.join('infra', 'bots', 'infra_tests.py')]
  try:
    subprocess.check_output(cmd)
  except subprocess.CalledProcessError as e:
    results.append(output_api.PresubmitError(
        '`%s` failed:\n%s' % (' '.join(cmd), e.output)))
  return results


def _CheckGNFormatted(input_api, output_api):
  """Make sure any .gn files we're changing have been formatted."""
  files = []
  for f in input_api.AffectedFiles(include_deletes=False):
    if (f.LocalPath().endswith('.gn') or
        f.LocalPath().endswith('.gni')):
      files.append(f)
  if not files:
    return []

  cmd = ['python', os.path.join('bin', 'fetch-gn')]
  try:
    subprocess.check_output(cmd)
  except subprocess.CalledProcessError as e:
    return [output_api.PresubmitError(
        '`%s` failed:\n%s' % (' '.join(cmd), e.output))]

  results = []
  for f in files:
    gn = 'gn.exe' if 'win32' in sys.platform else 'gn'
    gn = os.path.join(input_api.PresubmitLocalPath(), 'bin', gn)
    cmd = [gn, 'format', '--dry-run', f.LocalPath()]
    try:
      subprocess.check_output(cmd)
    except subprocess.CalledProcessError:
      fix = 'bin/gn format ' + f.LocalPath()
      results.append(output_api.PresubmitError(
          '`%s` failed, try\n\t%s' % (' '.join(cmd), fix)))
  return results


def _CheckGitConflictMarkers(input_api, output_api):
  pattern = input_api.re.compile('^(?:<<<<<<<|>>>>>>>) |^=======$')
  results = []
  for f in input_api.AffectedFiles():
    for line_num, line in f.ChangedContents():
      if f.LocalPath().endswith('.md'):
        # First-level headers in markdown look a lot like version control
        # conflict markers. http://daringfireball.net/projects/markdown/basics
        continue
      if pattern.match(line):
        results.append(
            output_api.PresubmitError(
                'Git conflict markers found in %s:%d %s' % (
                    f.LocalPath(), line_num, line)))
  return results


def _CheckIncludesFormatted(input_api, output_api):
  """Make sure #includes in files we're changing have been formatted."""
  files = [str(f) for f in input_api.AffectedFiles() if f.Action() != 'D']
  cmd = ['python',
         'tools/rewrite_includes.py',
         '--dry-run'] + files
  if 0 != subprocess.call(cmd):
    return [output_api.PresubmitError('`%s` failed' % ' '.join(cmd))]
  return []


class _WarningsAsErrors():
  def __init__(self, output_api):
    self.output_api = output_api
    self.old_warning = None
  def __enter__(self):
    self.old_warning = self.output_api.PresubmitPromptWarning
    self.output_api.PresubmitPromptWarning = self.output_api.PresubmitError
    return self.output_api
  def __exit__(self, ex_type, ex_value, ex_traceback):
    self.output_api.PresubmitPromptWarning = self.old_warning


def _CheckDEPSValid(input_api, output_api):
  """Ensure that DEPS contains valid entries."""
  results = []
  script = os.path.join('infra', 'bots', 'check_deps.py')
  relevant_files = ('DEPS', script)
  for f in input_api.AffectedFiles():
    if f.LocalPath() in relevant_files:
      break
  else:
    return results
  cmd = ['python', script]
  try:
    subprocess.check_output(cmd, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    results.append(output_api.PresubmitError(e.output))
  return results


def _RegenerateAllExamplesCPP(input_api, output_api):
  """Regenerates all_examples.cpp if an example was added or deleted."""
  if not any(f.LocalPath().startswith('docs/examples/')
             for f in input_api.AffectedFiles()):
    return []
  command_str = 'tools/fiddle/make_all_examples_cpp.py'
  cmd = ['python', command_str]
  if 0 != subprocess.call(cmd):
    return [output_api.PresubmitError('`%s` failed' % ' '.join(cmd))]

  results = []
  git_diff_output = input_api.subprocess.check_output(
      ['git', 'diff', '--no-ext-diff'])
  if git_diff_output:
    results += [output_api.PresubmitError(
        'Diffs found after running "%s":\n\n%s\n'
        'Please commit or discard the above changes.' % (
            command_str,
            git_diff_output,
        )
    )]
  return results

def _CommonChecks(input_api, output_api):
  """Presubmit checks common to upload and commit."""
  results = []
  sources = lambda x: (x.LocalPath().endswith('.h') or
                       x.LocalPath().endswith('.py') or
                       x.LocalPath().endswith('.sh') or
                       x.LocalPath().endswith('.m') or
                       x.LocalPath().endswith('.mm') or
                       x.LocalPath().endswith('.go') or
                       x.LocalPath().endswith('.c') or
                       x.LocalPath().endswith('.cc') or
                       x.LocalPath().endswith('.cpp'))
  results.extend(_CheckChangeHasEol(
      input_api, output_api, source_file_filter=sources))
  with _WarningsAsErrors(output_api):
    results.extend(input_api.canned_checks.CheckChangeHasNoCR(
        input_api, output_api, source_file_filter=sources))
    results.extend(input_api.canned_checks.CheckChangeHasNoStrayWhitespace(
        input_api, output_api, source_file_filter=sources))
  results.extend(_JsonChecks(input_api, output_api))
  results.extend(_IfDefChecks(input_api, output_api))
  results.extend(_CopyrightChecks(input_api, output_api,
                                  source_file_filter=sources))
  results.extend(_CheckDEPSValid(input_api, output_api))
  results.extend(_CheckIncludesFormatted(input_api, output_api))
  results.extend(_CheckGNFormatted(input_api, output_api))
  results.extend(_CheckGitConflictMarkers(input_api, output_api))
  results.extend(_RegenerateAllExamplesCPP(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  """Presubmit checks for the change on upload."""
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  # Run on upload, not commit, since the presubmit bot apparently doesn't have
  # coverage or Go installed.
  results.extend(_InfraTests(input_api, output_api))
  results.extend(_CheckReleaseNotesForPublicAPI(input_api, output_api))
  return results


class CodeReview(object):
  """Abstracts which codereview tool is used for the specified issue."""

  def __init__(self, input_api):
    self._issue = input_api.change.issue
    self._gerrit = input_api.gerrit

  def GetOwnerEmail(self):
    return self._gerrit.GetChangeOwner(self._issue)

  def GetSubject(self):
    return self._gerrit.GetChangeInfo(self._issue)['subject']

  def GetDescription(self):
    return self._gerrit.GetChangeDescription(self._issue)

  def GetReviewers(self):
    code_review_label = (
        self._gerrit.GetChangeInfo(self._issue)['labels']['Code-Review'])
    return [r['email'] for r in code_review_label.get('all', [])]

  def GetApprovers(self):
    approvers = []
    code_review_label = (
        self._gerrit.GetChangeInfo(self._issue)['labels']['Code-Review'])
    for m in code_review_label.get('all', []):
      if m.get("value") == 1:
        approvers.append(m["email"])
    return approvers


def _CheckReleaseNotesForPublicAPI(input_api, output_api):
  """Checks to see if release notes file is updated with public API changes."""
  results = []
  public_api_changed = False
  release_file_changed = False
  for affected_file in input_api.AffectedFiles():
    affected_file_path = affected_file.LocalPath()
    file_path, file_ext = os.path.splitext(affected_file_path)
    # We only care about files that end in .h and are under the top-level
    # include dir, but not include/private.
    if (file_ext == '.h' and
        file_path.split(os.path.sep)[0] == 'include' and
        'private' not in file_path):
      public_api_changed = True
    elif affected_file_path == RELEASE_NOTES_FILE_NAME:
      release_file_changed = True

  if public_api_changed and not release_file_changed:
    results.append(output_api.PresubmitPromptWarning(
        'If this change affects a client API, please add a summary line '
        'to the %s file.' % RELEASE_NOTES_FILE_NAME))
  return results


def PostUploadHook(gerrit, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook does the following:
  * Adds a link to preview docs changes if there are any docs changes in the CL.
  * Adds 'No-Try: true' if the CL contains only docs changes.
  """
  if not change.issue:
    return []

  # Skip PostUploadHooks for all auto-commit service account bots. New
  # patchsets (caused due to PostUploadHooks) invalidates the CQ+2 vote from
  # the "--use-commit-queue" flag to "git cl upload".
  for suffix in SERVICE_ACCOUNT_SUFFIX:
    if change.author_email.endswith(suffix):
      return []

  results = []
  at_least_one_docs_change = False
  all_docs_changes = True
  for affected_file in change.AffectedFiles():
    affected_file_path = affected_file.LocalPath()
    file_path, _ = os.path.splitext(affected_file_path)
    if 'site' == file_path.split(os.path.sep)[0]:
      at_least_one_docs_change = True
    else:
      all_docs_changes = False
    if at_least_one_docs_change and not all_docs_changes:
      break

  footers = change.GitFootersFromDescription()
  description_changed = False

  # If the change includes only doc changes then add No-Try: true in the
  # CL's description if it does not exist yet.
  if all_docs_changes and 'true' not in footers.get('No-Try', []):
    description_changed = True
    change.AddDescriptionFooter('No-Try', 'true')
    results.append(
        output_api.PresubmitNotifyResult(
            'This change has only doc changes. Automatically added '
            '\'No-Try: true\' to the CL\'s description'))

  # If the description has changed update it.
  if description_changed:
    gerrit.UpdateDescription(
        change.FullDescriptionText(), change.issue)

  return results


def CheckChangeOnCommit(input_api, output_api):
  """Presubmit checks for the change on commit."""
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  # Checks for the presence of 'DO NOT''SUBMIT' in CL description and in
  # content of files.
  results.extend(
      input_api.canned_checks.CheckDoNotSubmit(input_api, output_api))
  return results
