#!/usr/bin/env vpython3
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copyright (C) 2008 Evan Martin <martine@danga.com>

"""A git-command for integrating reviews on Gerrit."""

from __future__ import print_function

import base64
import collections
import datetime
import fnmatch
import httplib2
import itertools
import json
import logging
import multiprocessing
import optparse
import os
import re
import shutil
import stat
import sys
import tempfile
import textwrap
import time
import uuid
import webbrowser
import zlib

from third_party import colorama
import auth
import clang_format
import fix_encoding
import gclient_utils
import gerrit_util
import git_common
import git_footers
import git_new_branch
import metrics
import metrics_utils
import owners_client
import owners_finder
import presubmit_canned_checks
import presubmit_support
import rustfmt
import scm
import setup_color
import split_cl
import subcommand
import subprocess2
import swift_format
import watchlists

from third_party import six
from six.moves import urllib


if sys.version_info.major == 3:
  basestring = (str,)  # pylint: disable=redefined-builtin


__version__ = '2.0'

# Traces for git push will be stored in a traces directory inside the
# depot_tools checkout.
DEPOT_TOOLS = os.path.dirname(os.path.abspath(__file__))
TRACES_DIR = os.path.join(DEPOT_TOOLS, 'traces')
PRESUBMIT_SUPPORT = os.path.join(DEPOT_TOOLS, 'presubmit_support.py')

# When collecting traces, Git hashes will be reduced to 6 characters to reduce
# the size after compression.
GIT_HASH_RE = re.compile(r'\b([a-f0-9]{6})[a-f0-9]{34}\b', flags=re.I)
# Used to redact the cookies from the gitcookies file.
GITCOOKIES_REDACT_RE = re.compile(r'1/.*')

MAX_ATTEMPTS = 3

# The maximum number of traces we will keep. Multiplied by 3 since we store
# 3 files per trace.
MAX_TRACES = 3 * 10
# Message to be displayed to the user to inform where to find the traces for a
# git-cl upload execution.
TRACES_MESSAGE = (
'\n'
'The traces of this git-cl execution have been recorded at:\n'
'  %(trace_name)s-traces.zip\n'
'Copies of your gitcookies file and git config have been recorded at:\n'
'  %(trace_name)s-git-info.zip\n')
# Format of the message to be stored as part of the traces to give developers a
# better context when they go through traces.
TRACES_README_FORMAT = (
'Date: %(now)s\n'
'\n'
'Change: https://%(gerrit_host)s/q/%(change_id)s\n'
'Title: %(title)s\n'
'\n'
'%(description)s\n'
'\n'
'Execution time: %(execution_time)s\n'
'Exit code: %(exit_code)s\n') + TRACES_MESSAGE

POSTUPSTREAM_HOOK = '.git/hooks/post-cl-land'
DESCRIPTION_BACKUP_FILE = '.git_cl_description_backup'
REFS_THAT_ALIAS_TO_OTHER_REFS = {
    'refs/remotes/origin/lkgr': 'refs/remotes/origin/main',
    'refs/remotes/origin/lkcr': 'refs/remotes/origin/main',
}

DEFAULT_OLD_BRANCH = 'refs/remotes/origin/master'
DEFAULT_NEW_BRANCH = 'refs/remotes/origin/main'

DEFAULT_BUILDBUCKET_HOST = 'cr-buildbucket.appspot.com'

# Valid extensions for files we want to lint.
DEFAULT_LINT_REGEX = r"(.*\.cpp|.*\.cc|.*\.h)"
DEFAULT_LINT_IGNORE_REGEX = r"$^"

# File name for yapf style config files.
YAPF_CONFIG_FILENAME = '.style.yapf'

# The issue, patchset and codereview server are stored on git config for each
# branch under branch.<branch-name>.<config-key>.
ISSUE_CONFIG_KEY = 'gerritissue'
PATCHSET_CONFIG_KEY = 'gerritpatchset'
CODEREVIEW_SERVER_CONFIG_KEY = 'gerritserver'
# When using squash workflow, _CMDUploadChange doesn't simply push the commit(s)
# you make to Gerrit. Instead, it creates a new commit object that contains all
# changes you've made, diffed against a parent/merge base.
# This is the hash of the new squashed commit and you can find this on Gerrit.
GERRIT_SQUASH_HASH_CONFIG_KEY = 'gerritsquashhash'
# This is the latest uploaded local commit hash.
LAST_UPLOAD_HASH_CONFIG_KEY = 'last-upload-hash'

# Shortcut since it quickly becomes repetitive.
Fore = colorama.Fore

# Initialized in main()
settings = None

# Used by tests/git_cl_test.py to add extra logging.
# Inside the weirdly failing test, add this:
# >>> self.mock(git_cl, '_IS_BEING_TESTED', True)
# And scroll up to see the stack trace printed.
_IS_BEING_TESTED = False

_GOOGLESOURCE = 'googlesource.com'

_KNOWN_GERRIT_TO_SHORT_URLS = {
    'https://chrome-internal-review.googlesource.com': 'https://crrev.com/i',
    'https://chromium-review.googlesource.com': 'https://crrev.com/c',
}
assert len(_KNOWN_GERRIT_TO_SHORT_URLS) == len(
    set(_KNOWN_GERRIT_TO_SHORT_URLS.values())), 'must have unique values'


# Maximum number of branches in a stack that can be traversed and uploaded
# at once. Picked arbitrarily.
_MAX_STACKED_BRANCHES_UPLOAD = 20


# Environment variable to indicate if user is participating in the stcked
# changes dogfood.
DOGFOOD_STACKED_CHANGES_VAR = 'DOGFOOD_STACKED_CHANGES'


# Repo prefixes that are enrolled in the stacked changes dogfood.
DOGFOOD_STACKED_CHANGES_REPOS = [
    'chromium.googlesource.com/infra/',
    'chrome-internal.googlesource.com/infra/'
    'chromium.googlesource.com/chromium/src'
]


class GitPushError(Exception):
  pass


def DieWithError(message, change_desc=None):
  if change_desc:
    SaveDescriptionBackup(change_desc)
    print('\n ** Content of CL description **\n' +
          '='*72 +  '\n' +
          change_desc.description + '\n' +
          '='*72 + '\n')

  print(message, file=sys.stderr)
  sys.exit(1)


def SaveDescriptionBackup(change_desc):
  backup_path = os.path.join(DEPOT_TOOLS, DESCRIPTION_BACKUP_FILE)
  print('\nsaving CL description to %s\n' % backup_path)
  with open(backup_path, 'wb') as backup_file:
    backup_file.write(change_desc.description.encode('utf-8'))


def GetNoGitPagerEnv():
  env = os.environ.copy()
  # 'cat' is a magical git string that disables pagers on all platforms.
  env['GIT_PAGER'] = 'cat'
  return env


def RunCommand(args, error_ok=False, error_message=None, shell=False, **kwargs):
  try:
    stdout = subprocess2.check_output(args, shell=shell, **kwargs)
    return stdout.decode('utf-8', 'replace')
  except subprocess2.CalledProcessError as e:
    logging.debug('Failed running %s', args)
    if not error_ok:
      message = error_message or e.stdout.decode('utf-8', 'replace') or ''
      DieWithError('Command "%s" failed.\n%s' % (' '.join(args), message))
    out = e.stdout.decode('utf-8', 'replace')
    if e.stderr:
      out += e.stderr.decode('utf-8', 'replace')
    return out


def RunGit(args, **kwargs):
  """Returns stdout."""
  return RunCommand(['git'] + args, **kwargs)


def RunGitWithCode(args, suppress_stderr=False):
  """Returns return code and stdout."""
  if suppress_stderr:
    stderr = subprocess2.DEVNULL
  else:
    stderr = sys.stderr
  try:
    (out, _), code = subprocess2.communicate(['git'] + args,
                                             env=GetNoGitPagerEnv(),
                                             stdout=subprocess2.PIPE,
                                             stderr=stderr)
    return code, out.decode('utf-8', 'replace')
  except subprocess2.CalledProcessError as e:
    logging.debug('Failed running %s', ['git'] + args)
    return e.returncode, e.stdout.decode('utf-8', 'replace')


def RunGitSilent(args):
  """Returns stdout, suppresses stderr and ignores the return code."""
  return RunGitWithCode(args, suppress_stderr=True)[1]


def time_sleep(seconds):
  # Use this so that it can be mocked in tests without interfering with python
  # system machinery.
  return time.sleep(seconds)


def time_time():
  # Use this so that it can be mocked in tests without interfering with python
  # system machinery.
  return time.time()


def datetime_now():
  # Use this so that it can be mocked in tests without interfering with python
  # system machinery.
  return datetime.datetime.now()


def confirm_or_exit(prefix='', action='confirm'):
  """Asks user to press enter to continue or press Ctrl+C to abort."""
  if not prefix or prefix.endswith('\n'):
    mid = 'Press'
  elif prefix.endswith('.') or prefix.endswith('?'):
    mid = ' Press'
  elif prefix.endswith(' '):
    mid = 'press'
  else:
    mid = ' press'
  gclient_utils.AskForData(
      '%s%s Enter to %s, or Ctrl+C to abort' % (prefix, mid, action))


def ask_for_explicit_yes(prompt):
  """Returns whether user typed 'y' or 'yes' to confirm the given prompt."""
  result = gclient_utils.AskForData(prompt + ' [Yes/No]: ').lower()
  while True:
    if 'yes'.startswith(result):
      return True
    if 'no'.startswith(result):
      return False
    result = gclient_utils.AskForData('Please, type yes or no: ').lower()


def _get_properties_from_options(options):
  prop_list = getattr(options, 'properties', [])
  properties = dict(x.split('=', 1) for x in prop_list)
  for key, val in properties.items():
    try:
      properties[key] = json.loads(val)
    except ValueError:
      pass  # If a value couldn't be evaluated, treat it as a string.
  return properties


def _call_buildbucket(http, buildbucket_host, method, request):
  """Calls a buildbucket v2 method and returns the parsed json response."""
  headers = {
    'Accept': 'application/json',
    'Content-Type': 'application/json',
  }
  request = json.dumps(request)
  url = 'https://%s/prpc/buildbucket.v2.Builds/%s' % (buildbucket_host, method)

  logging.info('POST %s with %s' % (url, request))

  attempts = 1
  time_to_sleep = 1
  while True:
    response, content = http.request(url, 'POST', body=request, headers=headers)
    if response.status == 200:
      return json.loads(content[4:])
    if attempts >= MAX_ATTEMPTS or 400 <= response.status < 500:
      msg = '%s error when calling POST %s with %s: %s' % (
          response.status, url, request, content)
      raise BuildbucketResponseException(msg)
    logging.debug(
        '%s error when calling POST %s with %s. '
        'Sleeping for %d seconds and retrying...' % (
            response.status, url, request, time_to_sleep))
    time.sleep(time_to_sleep)
    time_to_sleep *= 2
    attempts += 1

  assert False, 'unreachable'


def _parse_bucket(raw_bucket):
  legacy = True
  project = bucket = None
  if '/' in raw_bucket:
    legacy = False
    project, bucket = raw_bucket.split('/', 1)
  # Assume luci.<project>.<bucket>.
  elif raw_bucket.startswith('luci.'):
    project, bucket = raw_bucket[len('luci.'):].split('.', 1)
  # Otherwise, assume prefix is also the project name.
  elif '.' in raw_bucket:
    project = raw_bucket.split('.')[0]
    bucket = raw_bucket
  # Legacy buckets.
  if legacy and project and bucket:
    print('WARNING Please use %s/%s to specify the bucket.' % (project, bucket))
  return project, bucket


def _canonical_git_googlesource_host(host):
  """Normalizes Gerrit hosts (with '-review') to Git host."""
  assert host.endswith(_GOOGLESOURCE)
  # Prefix doesn't include '.' at the end.
  prefix = host[:-(1 + len(_GOOGLESOURCE))]
  if prefix.endswith('-review'):
    prefix = prefix[:-len('-review')]
  return prefix + '.' + _GOOGLESOURCE


def _canonical_gerrit_googlesource_host(host):
  git_host = _canonical_git_googlesource_host(host)
  prefix = git_host.split('.', 1)[0]
  return prefix + '-review.' + _GOOGLESOURCE


def _get_counterpart_host(host):
  assert host.endswith(_GOOGLESOURCE)
  git = _canonical_git_googlesource_host(host)
  gerrit = _canonical_gerrit_googlesource_host(git)
  return git if gerrit == host else gerrit


def _trigger_tryjobs(changelist, jobs, options, patchset):
  """Sends a request to Buildbucket to trigger tryjobs for a changelist.

  Args:
    changelist: Changelist that the tryjobs are associated with.
    jobs: A list of (project, bucket, builder).
    options: Command-line options.
  """
  print('Scheduling jobs on:')
  for project, bucket, builder in jobs:
    print('  %s/%s: %s' % (project, bucket, builder))
  print('To see results here, run:        git cl try-results')
  print('To see results in browser, run:  git cl web')

  requests = _make_tryjob_schedule_requests(changelist, jobs, options, patchset)
  if not requests:
    return

  http = auth.Authenticator().authorize(httplib2.Http())
  http.force_exception_to_status_code = True

  batch_request = {'requests': requests}
  batch_response = _call_buildbucket(http, DEFAULT_BUILDBUCKET_HOST, 'Batch',
                                     batch_request)

  errors = [
      '  ' + response['error']['message']
      for response in batch_response.get('responses', [])
      if 'error' in response
  ]
  if errors:
    raise BuildbucketResponseException(
        'Failed to schedule builds for some bots:\n%s' % '\n'.join(errors))


def _make_tryjob_schedule_requests(changelist, jobs, options, patchset):
  """Constructs requests for Buildbucket to trigger tryjobs."""
  gerrit_changes = [changelist.GetGerritChange(patchset)]
  shared_properties = {
      'category': options.ensure_value('category', 'git_cl_try')
  }
  if options.ensure_value('clobber', False):
    shared_properties['clobber'] = True
  shared_properties.update(_get_properties_from_options(options) or {})

  shared_tags = [{'key': 'user_agent', 'value': 'git_cl_try'}]
  if options.ensure_value('retry_failed', False):
    shared_tags.append({'key': 'retry_failed',
                        'value': '1'})

  requests = []
  for (project, bucket, builder) in jobs:
    properties = shared_properties.copy()
    if 'presubmit' in builder.lower():
      properties['dry_run'] = 'true'

    requests.append({
        'scheduleBuild': {
            'requestId': str(uuid.uuid4()),
            'builder': {
                'project': getattr(options, 'project', None) or project,
                'bucket': bucket,
                'builder': builder,
            },
            'gerritChanges': gerrit_changes,
            'properties': properties,
            'tags': [
                {'key': 'builder', 'value': builder},
            ] + shared_tags,
        }
    })

    if options.ensure_value('revision', None):
      remote, remote_branch = changelist.GetRemoteBranch()
      requests[-1]['scheduleBuild']['gitilesCommit'] = {
          'host': _canonical_git_googlesource_host(gerrit_changes[0]['host']),
          'project': gerrit_changes[0]['project'],
          'id': options.revision,
          'ref': GetTargetRef(remote, remote_branch, None)
      }

  return requests


def _fetch_tryjobs(changelist, buildbucket_host, patchset=None):
  """Fetches tryjobs from buildbucket.

  Returns list of buildbucket.v2.Build with the try jobs for the changelist.
  """
  fields = ['id', 'builder', 'status', 'createTime', 'tags']
  request = {
      'predicate': {
          'gerritChanges': [changelist.GetGerritChange(patchset)],
      },
      'fields': ','.join('builds.*.' + field for field in fields),
  }

  authenticator = auth.Authenticator()
  if authenticator.has_cached_credentials():
    http = authenticator.authorize(httplib2.Http())
  else:
    print('Warning: Some results might be missing because %s' %
          # Get the message on how to login.
          (str(auth.LoginRequiredError()),))
    http = httplib2.Http()
  http.force_exception_to_status_code = True

  response = _call_buildbucket(http, buildbucket_host, 'SearchBuilds', request)
  return response.get('builds', [])


def _fetch_latest_builds(changelist, buildbucket_host, latest_patchset=None):
  """Fetches builds from the latest patchset that has builds (within
  the last few patchsets).

  Args:
    changelist (Changelist): The CL to fetch builds for
    buildbucket_host (str): Buildbucket host, e.g. "cr-buildbucket.appspot.com"
    lastest_patchset(int|NoneType): the patchset to start fetching builds from.
      If None (default), starts with the latest available patchset.
  Returns:
    A tuple (builds, patchset) where builds is a list of buildbucket.v2.Build,
    and patchset is the patchset number where those builds came from.
  """
  assert buildbucket_host
  assert changelist.GetIssue(), 'CL must be uploaded first'
  assert changelist.GetCodereviewServer(), 'CL must be uploaded first'
  if latest_patchset is None:
    assert changelist.GetMostRecentPatchset()
    ps = changelist.GetMostRecentPatchset()
  else:
    assert latest_patchset > 0, latest_patchset
    ps = latest_patchset

  min_ps = max(1, ps - 5)
  while ps >= min_ps:
    builds = _fetch_tryjobs(changelist, buildbucket_host, patchset=ps)
    if len(builds):
      return builds, ps
    ps -= 1
  return [], 0


def _filter_failed_for_retry(all_builds):
  """Returns a list of buckets/builders that are worth retrying.

  Args:
    all_builds (list): Builds, in the format returned by _fetch_tryjobs,
      i.e. a list of buildbucket.v2.Builds which includes status and builder
      info.

  Returns:
    A dict {(proj, bucket): [builders]}. This is the same format accepted by
    _trigger_tryjobs.
  """
  grouped = {}
  for build in all_builds:
    builder = build['builder']
    key = (builder['project'], builder['bucket'], builder['builder'])
    grouped.setdefault(key, []).append(build)

  jobs = []
  for (project, bucket, builder), builds in grouped.items():
    if 'triggered' in builder:
      print('WARNING: Not scheduling %s. Triggered bots require an initial job '
            'from a parent. Please schedule a manual job for the parent '
            'instead.')
      continue
    if any(b['status'] in ('STARTED', 'SCHEDULED') for b in builds):
      # Don't retry if any are running.
      continue
    # If builder had several builds, retry only if the last one failed.
    # This is a bit different from CQ, which would re-use *any* SUCCESS-full
    # build, but in case of retrying failed jobs retrying a flaky one makes
    # sense.
    builds = sorted(builds, key=lambda b: b['createTime'])
    if builds[-1]['status'] not in ('FAILURE', 'INFRA_FAILURE'):
      continue
    # Don't retry experimental build previously triggered by CQ.
    if any(t['key'] == 'cq_experimental' and t['value'] == 'true'
           for t in builds[-1]['tags']):
      continue
    jobs.append((project, bucket, builder))

  # Sort the jobs to make testing easier.
  return sorted(jobs)


def _print_tryjobs(options, builds):
  """Prints nicely result of _fetch_tryjobs."""
  if not builds:
    print('No tryjobs scheduled.')
    return

  longest_builder = max(len(b['builder']['builder']) for b in builds)
  name_fmt = '{builder:<%d}' % longest_builder
  if options.print_master:
    longest_bucket = max(len(b['builder']['bucket']) for b in builds)
    name_fmt = ('{bucket:>%d} ' % longest_bucket) + name_fmt

  builds_by_status = {}
  for b in builds:
    builds_by_status.setdefault(b['status'], []).append({
        'id': b['id'],
        'name': name_fmt.format(
            builder=b['builder']['builder'], bucket=b['builder']['bucket']),
    })

  sort_key = lambda b: (b['name'], b['id'])

  def print_builds(title, builds, fmt=None, color=None):
    """Pop matching builds from `builds` dict and print them."""
    if not builds:
      return

    fmt = fmt or '{name} https://ci.chromium.org/b/{id}'
    if not options.color or color is None:
      colorize = lambda x: x
    else:
      colorize = lambda x: '%s%s%s' % (color, x, Fore.RESET)

    print(colorize(title))
    for b in sorted(builds, key=sort_key):
      print(' ', colorize(fmt.format(**b)))

  total = len(builds)
  print_builds(
      'Successes:', builds_by_status.pop('SUCCESS', []), color=Fore.GREEN)
  print_builds(
      'Infra Failures:', builds_by_status.pop('INFRA_FAILURE', []),
      color=Fore.MAGENTA)
  print_builds('Failures:', builds_by_status.pop('FAILURE', []), color=Fore.RED)
  print_builds('Canceled:', builds_by_status.pop('CANCELED', []), fmt='{name}',
      color=Fore.MAGENTA)
  print_builds('Started:', builds_by_status.pop('STARTED', []),
      color=Fore.YELLOW)
  print_builds(
      'Scheduled:', builds_by_status.pop('SCHEDULED', []), fmt='{name} id={id}')
  # The last section is just in case buildbucket API changes OR there is a bug.
  print_builds(
      'Other:', sum(builds_by_status.values(), []), fmt='{name} id={id}')
  print('Total: %d tryjobs' % total)


def _ComputeDiffLineRanges(files, upstream_commit):
  """Gets the changed line ranges for each file since upstream_commit.

  Parses a git diff on provided files and returns a dict that maps a file name
  to an ordered list of range tuples in the form (start_line, count).
  Ranges are in the same format as a git diff.
  """
  # If files is empty then diff_output will be a full diff.
  if len(files) == 0:
    return {}

  # Take the git diff and find the line ranges where there are changes.
  diff_cmd = BuildGitDiffCmd('-U0', upstream_commit, files, allow_prefix=True)
  diff_output = RunGit(diff_cmd)

  pattern = r'(?:^diff --git a/(?:.*) b/(.*))|(?:^@@.*\+(.*) @@)'
  # 2 capture groups
  # 0 == fname of diff file
  # 1 == 'diff_start,diff_count' or 'diff_start'
  # will match each of
  # diff --git a/foo.foo b/foo.py
  # @@ -12,2 +14,3 @@
  # @@ -12,2 +17 @@
  # running re.findall on the above string with pattern will give
  # [('foo.py', ''), ('', '14,3'), ('', '17')]

  curr_file = None
  line_diffs = {}
  for match in re.findall(pattern, diff_output, flags=re.MULTILINE):
    if match[0] != '':
      # Will match the second filename in diff --git a/a.py b/b.py.
      curr_file = match[0]
      line_diffs[curr_file] = []
    else:
      # Matches +14,3
      if ',' in match[1]:
        diff_start, diff_count = match[1].split(',')
      else:
        # Single line changes are of the form +12 instead of +12,1.
        diff_start = match[1]
        diff_count = 1

      diff_start = int(diff_start)
      diff_count = int(diff_count)

      # If diff_count == 0 this is a removal we can ignore.
      line_diffs[curr_file].append((diff_start, diff_count))

  return line_diffs


def _FindYapfConfigFile(fpath, yapf_config_cache, top_dir=None):
  """Checks if a yapf file is in any parent directory of fpath until top_dir.

  Recursively checks parent directories to find yapf file and if no yapf file
  is found returns None. Uses yapf_config_cache as a cache for previously found
  configs.
  """
  fpath = os.path.abspath(fpath)
  # Return result if we've already computed it.
  if fpath in yapf_config_cache:
    return yapf_config_cache[fpath]

  parent_dir = os.path.dirname(fpath)
  if os.path.isfile(fpath):
    ret = _FindYapfConfigFile(parent_dir, yapf_config_cache, top_dir)
  else:
    # Otherwise fpath is a directory
    yapf_file = os.path.join(fpath, YAPF_CONFIG_FILENAME)
    if os.path.isfile(yapf_file):
      ret = yapf_file
    elif fpath in (top_dir, parent_dir):
      # If we're at the top level directory, or if we're at root
      # there is no provided style.
      ret = None
    else:
      # Otherwise recurse on the current directory.
      ret = _FindYapfConfigFile(parent_dir, yapf_config_cache, top_dir)
  yapf_config_cache[fpath] = ret
  return ret


def _GetYapfIgnorePatterns(top_dir):
  """Returns all patterns in the .yapfignore file.

  yapf is supposed to handle the ignoring of files listed in .yapfignore itself,
  but this functionality appears to break when explicitly passing files to
  yapf for formatting. According to
  https://github.com/google/yapf/blob/HEAD/README.rst#excluding-files-from-formatting-yapfignore,
  the .yapfignore file should be in the directory that yapf is invoked from,
  which we assume to be the top level directory in this case.

  Args:
    top_dir: The top level directory for the repository being formatted.

  Returns:
    A set of all fnmatch patterns to be ignored.
  """
  yapfignore_file = os.path.join(top_dir, '.yapfignore')
  ignore_patterns = set()
  if not os.path.exists(yapfignore_file):
    return ignore_patterns

  for line in gclient_utils.FileRead(yapfignore_file).split('\n'):
    stripped_line = line.strip()
    # Comments and blank lines should be ignored.
    if stripped_line.startswith('#') or stripped_line == '':
      continue
    ignore_patterns.add(stripped_line)
  return ignore_patterns


def _FilterYapfIgnoredFiles(filepaths, patterns):
  """Filters out any filepaths that match any of the given patterns.

  Args:
    filepaths: An iterable of strings containing filepaths to filter.
    patterns: An iterable of strings containing fnmatch patterns to filter on.

  Returns:
    A list of strings containing all the elements of |filepaths| that did not
    match any of the patterns in |patterns|.
  """
  # Not inlined so that tests can use the same implementation.
  return [f for f in filepaths
          if not any(fnmatch.fnmatch(f, p) for p in patterns)]


def print_stats(args):
  """Prints statistics about the change to the user."""
  # --no-ext-diff is broken in some versions of Git, so try to work around
  # this by overriding the environment (but there is still a problem if the
  # git config key "diff.external" is used).
  env = GetNoGitPagerEnv()
  if 'GIT_EXTERNAL_DIFF' in env:
    del env['GIT_EXTERNAL_DIFF']

  return subprocess2.call(
      ['git', 'diff', '--no-ext-diff', '--stat', '-l100000', '-C50'] + args,
      env=env)


class BuildbucketResponseException(Exception):
  pass


class Settings(object):
  def __init__(self):
    self.cc = None
    self.root = None
    self.tree_status_url = None
    self.viewvc_url = None
    self.updated = False
    self.is_gerrit = None
    self.squash_gerrit_uploads = None
    self.gerrit_skip_ensure_authenticated = None
    self.git_editor = None
    self.format_full_by_default = None
    self.is_status_commit_order_by_date = None

  def _LazyUpdateIfNeeded(self):
    """Updates the settings from a codereview.settings file, if available."""
    if self.updated:
      return

    # The only value that actually changes the behavior is
    # autoupdate = "false". Everything else means "true".
    autoupdate = (
        scm.GIT.GetConfig(self.GetRoot(), 'rietveld.autoupdate', '').lower())

    cr_settings_file = FindCodereviewSettingsFile()
    if autoupdate != 'false' and cr_settings_file:
      LoadCodereviewSettingsFromFile(cr_settings_file)
      cr_settings_file.close()

    self.updated = True

  @staticmethod
  def GetRelativeRoot():
    return scm.GIT.GetCheckoutRoot('.')

  def GetRoot(self):
    if self.root is None:
      self.root = os.path.abspath(self.GetRelativeRoot())
    return self.root

  def GetTreeStatusUrl(self, error_ok=False):
    if not self.tree_status_url:
      self.tree_status_url = self._GetConfig('rietveld.tree-status-url')
      if self.tree_status_url is None and not error_ok:
        DieWithError(
            'You must configure your tree status URL by running '
            '"git cl config".')
    return self.tree_status_url

  def GetViewVCUrl(self):
    if not self.viewvc_url:
      self.viewvc_url = self._GetConfig('rietveld.viewvc-url')
    return self.viewvc_url

  def GetBugPrefix(self):
    return self._GetConfig('rietveld.bug-prefix')

  def GetRunPostUploadHook(self):
    run_post_upload_hook = self._GetConfig(
        'rietveld.run-post-upload-hook')
    return run_post_upload_hook == "True"

  def GetDefaultCCList(self):
    return self._GetConfig('rietveld.cc')

  def GetUsePython3(self):
    return self._GetConfig('rietveld.use-python3')

  def GetSquashGerritUploads(self):
    """Returns True if uploads to Gerrit should be squashed by default."""
    if self.squash_gerrit_uploads is None:
      self.squash_gerrit_uploads = self.GetSquashGerritUploadsOverride()
    if self.squash_gerrit_uploads is None:
      # Default is squash now (http://crbug.com/611892#c23).
      self.squash_gerrit_uploads = self._GetConfig(
          'gerrit.squash-uploads').lower() != 'false'
    return self.squash_gerrit_uploads

  def GetSquashGerritUploadsOverride(self):
    """Return True or False if codereview.settings should be overridden.

    Returns None if no override has been defined.
    """
    # See also http://crbug.com/611892#c23
    result = self._GetConfig('gerrit.override-squash-uploads').lower()
    if result == 'true':
      return True
    if result == 'false':
      return False
    return None

  def GetIsGerrit(self):
    """Return True if gerrit.host is set."""
    if self.is_gerrit is None:
      self.is_gerrit = bool(self._GetConfig('gerrit.host', False))
    return self.is_gerrit

  def GetGerritSkipEnsureAuthenticated(self):
    """Return True if EnsureAuthenticated should not be done for Gerrit
    uploads."""
    if self.gerrit_skip_ensure_authenticated is None:
      self.gerrit_skip_ensure_authenticated = self._GetConfig(
          'gerrit.skip-ensure-authenticated').lower() == 'true'
    return self.gerrit_skip_ensure_authenticated

  def GetGitEditor(self):
    """Returns the editor specified in the git config, or None if none is."""
    if self.git_editor is None:
      # Git requires single quotes for paths with spaces. We need to replace
      # them with double quotes for Windows to treat such paths as a single
      # path.
      self.git_editor = self._GetConfig('core.editor').replace('\'', '"')
    return self.git_editor or None

  def GetLintRegex(self):
    return self._GetConfig('rietveld.cpplint-regex', DEFAULT_LINT_REGEX)

  def GetLintIgnoreRegex(self):
    return self._GetConfig(
        'rietveld.cpplint-ignore-regex', DEFAULT_LINT_IGNORE_REGEX)

  def GetFormatFullByDefault(self):
    if self.format_full_by_default is None:
      self._LazyUpdateIfNeeded()
      result = (
          RunGit(['config', '--bool', 'rietveld.format-full-by-default'],
                 error_ok=True).strip())
      self.format_full_by_default = (result == 'true')
    return self.format_full_by_default

  def IsStatusCommitOrderByDate(self):
    if self.is_status_commit_order_by_date is None:
      result = (RunGit(['config', '--bool', 'cl.date-order'],
                       error_ok=True).strip())
      self.is_status_commit_order_by_date = (result == 'true')
    return self.is_status_commit_order_by_date

  def _GetConfig(self, key, default=''):
    self._LazyUpdateIfNeeded()
    return scm.GIT.GetConfig(self.GetRoot(), key, default)


class _CQState(object):
  """Enum for states of CL with respect to CQ."""
  NONE = 'none'
  QUICK_RUN = 'quick_run'
  DRY_RUN = 'dry_run'
  COMMIT = 'commit'

  ALL_STATES = [NONE, QUICK_RUN, DRY_RUN, COMMIT]


class _ParsedIssueNumberArgument(object):
  def __init__(self, issue=None, patchset=None, hostname=None):
    self.issue = issue
    self.patchset = patchset
    self.hostname = hostname

  @property
  def valid(self):
    return self.issue is not None


def ParseIssueNumberArgument(arg):
  """Parses the issue argument and returns _ParsedIssueNumberArgument."""
  fail_result = _ParsedIssueNumberArgument()

  if isinstance(arg, int):
    return _ParsedIssueNumberArgument(issue=arg)
  if not isinstance(arg, basestring):
    return fail_result

  if arg.isdigit():
    return _ParsedIssueNumberArgument(issue=int(arg))

  url = gclient_utils.UpgradeToHttps(arg)
  if not url.startswith('http'):
    return fail_result
  for gerrit_url, short_url in _KNOWN_GERRIT_TO_SHORT_URLS.items():
    if url.startswith(short_url):
      url = gerrit_url + url[len(short_url):]
      break

  try:
    parsed_url = urllib.parse.urlparse(url)
  except ValueError:
    return fail_result

  # If "https://" was automatically added, fail if `arg` looks unlikely to be a
  # URL.
  if not arg.startswith('http') and '.' not in parsed_url.netloc:
    return fail_result

  # Gerrit's new UI is https://domain/c/project/+/<issue_number>[/[patchset]]
  # But old GWT UI is https://domain/#/c/project/+/<issue_number>[/[patchset]]
  # Short urls like https://domain/<issue_number> can be used, but don't allow
  # specifying the patchset (you'd 404), but we allow that here.
  if parsed_url.path == '/':
    part = parsed_url.fragment
  else:
    part = parsed_url.path

  match = re.match(
      r'(/c(/.*/\+)?)?/(?P<issue>\d+)(/(?P<patchset>\d+)?/?)?$', part)
  if not match:
    return fail_result

  issue = int(match.group('issue'))
  patchset = match.group('patchset')
  return _ParsedIssueNumberArgument(
      issue=issue,
      patchset=int(patchset) if patchset else None,
      hostname=parsed_url.netloc)


def _create_description_from_log(args):
  """Pulls out the commit log to use as a base for the CL description."""
  log_args = []
  if len(args) == 1 and args[0] == None:
    # Handle the case where None is passed as the branch.
    return ''
  if len(args) == 1 and not args[0].endswith('.'):
    log_args = [args[0] + '..']
  elif len(args) == 1 and args[0].endswith('...'):
    log_args = [args[0][:-1]]
  elif len(args) == 2:
    log_args = [args[0] + '..' + args[1]]
  else:
    log_args = args[:]  # Hope for the best!
  return RunGit(['log', '--pretty=format:%B%n'] + log_args)


class GerritChangeNotExists(Exception):
  def __init__(self, issue, url):
    self.issue = issue
    self.url = url
    super(GerritChangeNotExists, self).__init__()

  def __str__(self):
    return 'change %s at %s does not exist or you have no access to it' % (
        self.issue, self.url)


_CommentSummary = collections.namedtuple(
    '_CommentSummary', ['date', 'message', 'sender', 'autogenerated',
                        # TODO(tandrii): these two aren't known in Gerrit.
                        'approval', 'disapproval'])


# TODO(b/265929888): Change `parent` to `pushed_commit_base`.
_NewUpload = collections.namedtuple('NewUpload', [
    'reviewers', 'ccs', 'commit_to_push', 'new_last_uploaded_commit', 'parent',
    'change_desc', 'prev_patchset'
])


class Changelist(object):
  """Changelist works with one changelist in local branch.

  Notes:
    * Not safe for concurrent multi-{thread,process} use.
    * Caches values from current branch. Therefore, re-use after branch change
      with great care.
  """

  def __init__(self,
               branchref=None,
               issue=None,
               codereview_host=None,
               commit_date=None):
    """Create a new ChangeList instance.

    **kwargs will be passed directly to Gerrit implementation.
    """
    # Poke settings so we get the "configure your server" message if necessary.
    global settings
    if not settings:
      # Happens when git_cl.py is used as a utility library.
      settings = Settings()

    self.branchref = branchref
    if self.branchref:
      assert branchref.startswith('refs/heads/')
      self.branch = scm.GIT.ShortBranchName(self.branchref)
    else:
      self.branch = None
    self.commit_date = commit_date
    self.upstream_branch = None
    self.lookedup_issue = False
    self.issue = issue or None
    self.description = None
    self.lookedup_patchset = False
    self.patchset = None
    self.cc = None
    self.more_cc = []
    self._remote = None
    self._cached_remote_url = (False, None)  # (is_cached, value)

    # Lazily cached values.
    self._gerrit_host = None    # e.g. chromium-review.googlesource.com
    self._gerrit_server = None  # e.g. https://chromium-review.googlesource.com
    self._owners_client = None
    # Map from change number (issue) to its detail cache.
    self._detail_cache = {}

    if codereview_host is not None:
      assert not codereview_host.startswith('https://'), codereview_host
      self._gerrit_host = codereview_host
      self._gerrit_server = 'https://%s' % codereview_host

  @property
  def owners_client(self):
    if self._owners_client is None:
      remote, remote_branch = self.GetRemoteBranch()
      branch = GetTargetRef(remote, remote_branch, None)
      self._owners_client = owners_client.GetCodeOwnersClient(
          host=self.GetGerritHost(),
          project=self.GetGerritProject(),
          branch=branch)
    return self._owners_client

  def GetCCList(self):
    """Returns the users cc'd on this CL.

    The return value is a string suitable for passing to git cl with the --cc
    flag.
    """
    if self.cc is None:
      base_cc = settings.GetDefaultCCList()
      more_cc = ','.join(self.more_cc)
      self.cc = ','.join(filter(None, (base_cc, more_cc))) or ''
    return self.cc

  def ExtendCC(self, more_cc):
    """Extends the list of users to cc on this CL based on the changed files."""
    self.more_cc.extend(more_cc)

  def GetCommitDate(self):
    """Returns the commit date as provided in the constructor"""
    return self.commit_date

  def GetBranch(self):
    """Returns the short branch name, e.g. 'main'."""
    if not self.branch:
      branchref = scm.GIT.GetBranchRef(settings.GetRoot())
      if not branchref:
        return None
      self.branchref = branchref
      self.branch = scm.GIT.ShortBranchName(self.branchref)
    return self.branch

  def GetBranchRef(self):
    """Returns the full branch name, e.g. 'refs/heads/main'."""
    self.GetBranch()  # Poke the lazy loader.
    return self.branchref

  def _GitGetBranchConfigValue(self, key, default=None):
    return scm.GIT.GetBranchConfig(
        settings.GetRoot(), self.GetBranch(), key, default)

  def _GitSetBranchConfigValue(self, key, value):
    action = 'set %s to %r' % (key, value)
    if not value:
      action = 'unset %s' % key
    assert self.GetBranch(), 'a branch is needed to ' + action
    return scm.GIT.SetBranchConfig(
        settings.GetRoot(), self.GetBranch(), key, value)

  @staticmethod
  def FetchUpstreamTuple(branch):
    """Returns a tuple containing remote and remote ref,
       e.g. 'origin', 'refs/heads/main'
    """
    remote, upstream_branch = scm.GIT.FetchUpstreamTuple(
        settings.GetRoot(), branch)
    if not remote or not upstream_branch:
      DieWithError(
         'Unable to determine default branch to diff against.\n'
         'Verify this branch is set up to track another \n'
         '(via the --track argument to "git checkout -b ..."). \n'
         'or pass complete "git diff"-style arguments if supported, like\n'
         '  git cl upload origin/main\n')

    return remote, upstream_branch

  def GetCommonAncestorWithUpstream(self):
    upstream_branch = self.GetUpstreamBranch()
    if not scm.GIT.IsValidRevision(settings.GetRoot(), upstream_branch):
      DieWithError('The upstream for the current branch (%s) does not exist '
                   'anymore.\nPlease fix it and try again.' % self.GetBranch())
    return git_common.get_or_create_merge_base(self.GetBranch(),
                                               upstream_branch)

  def GetUpstreamBranch(self):
    if self.upstream_branch is None:
      remote, upstream_branch = self.FetchUpstreamTuple(self.GetBranch())
      if remote != '.':
        upstream_branch = upstream_branch.replace('refs/heads/',
                                                  'refs/remotes/%s/' % remote)
        upstream_branch = upstream_branch.replace('refs/branch-heads/',
                                                  'refs/remotes/branch-heads/')
      self.upstream_branch = upstream_branch
    return self.upstream_branch

  def GetRemoteBranch(self):
    if not self._remote:
      remote, branch = None, self.GetBranch()
      seen_branches = set()
      while branch not in seen_branches:
        seen_branches.add(branch)
        remote, branch = self.FetchUpstreamTuple(branch)
        branch = scm.GIT.ShortBranchName(branch)
        if remote != '.' or branch.startswith('refs/remotes'):
          break
      else:
        remotes = RunGit(['remote'], error_ok=True).split()
        if len(remotes) == 1:
          remote, = remotes
        elif 'origin' in remotes:
          remote = 'origin'
          logging.warning('Could not determine which remote this change is '
                          'associated with, so defaulting to "%s".' %
                          self._remote)
        else:
          logging.warning('Could not determine which remote this change is '
                          'associated with.')
        branch = 'HEAD'
      if branch.startswith('refs/remotes'):
        self._remote = (remote, branch)
      elif branch.startswith('refs/branch-heads/'):
        self._remote = (remote, branch.replace('refs/', 'refs/remotes/'))
      else:
        self._remote = (remote, 'refs/remotes/%s/%s' % (remote, branch))
    return self._remote

  def GetRemoteUrl(self):
    """Return the configured remote URL, e.g. 'git://example.org/foo.git/'.

    Returns None if there is no remote.
    """
    is_cached, value = self._cached_remote_url
    if is_cached:
      return value

    remote, _ = self.GetRemoteBranch()
    url = scm.GIT.GetConfig(settings.GetRoot(), 'remote.%s.url' % remote, '')

    # Check if the remote url can be parsed as an URL.
    host = urllib.parse.urlparse(url).netloc
    if host:
      self._cached_remote_url = (True, url)
      return url

    # If it cannot be parsed as an url, assume it is a local directory,
    # probably a git cache.
    logging.warning('"%s" doesn\'t appear to point to a git host. '
                    'Interpreting it as a local directory.', url)
    if not os.path.isdir(url):
      logging.error(
          'Remote "%(remote)s" for branch "%(branch)s" points to "%(url)s", '
          'but it doesn\'t exist.',
          {'remote': remote, 'branch': self.GetBranch(), 'url': url})
      return None

    cache_path = url
    url = scm.GIT.GetConfig(url, 'remote.%s.url' % remote, '')

    host = urllib.parse.urlparse(url).netloc
    if not host:
      logging.error(
          'Remote "%(remote)s" for branch "%(branch)s" points to '
          '"%(cache_path)s", but it is misconfigured.\n'
          '"%(cache_path)s" must be a git repo and must have a remote named '
          '"%(remote)s" pointing to the git host.', {
              'remote': remote,
              'cache_path': cache_path,
              'branch': self.GetBranch()})
      return None

    self._cached_remote_url = (True, url)
    return url

  def GetIssue(self):
    """Returns the issue number as a int or None if not set."""
    if self.issue is None and not self.lookedup_issue:
      if self.GetBranch():
        self.issue = self._GitGetBranchConfigValue(ISSUE_CONFIG_KEY)
      if self.issue is not None:
        self.issue = int(self.issue)
      self.lookedup_issue = True
    return self.issue

  def GetIssueURL(self, short=False):
    """Get the URL for a particular issue."""
    issue = self.GetIssue()
    if not issue:
      return None
    server = self.GetCodereviewServer()
    if short:
      server = _KNOWN_GERRIT_TO_SHORT_URLS.get(server, server)
    return '%s/%s' % (server, issue)

  def GetUsePython3(self):
    return settings.GetUsePython3()

  def FetchDescription(self, pretty=False):
    assert self.GetIssue(), 'issue is required to query Gerrit'

    if self.description is None:
      data = self._GetChangeDetail(['CURRENT_REVISION', 'CURRENT_COMMIT'])
      current_rev = data['current_revision']
      self.description = data['revisions'][current_rev]['commit']['message']

    if not pretty:
      return self.description

    # Set width to 72 columns + 2 space indent.
    wrapper = textwrap.TextWrapper(width=74, replace_whitespace=True)
    wrapper.initial_indent = wrapper.subsequent_indent = '  '
    lines = self.description.splitlines()
    return '\n'.join([wrapper.fill(line) for line in lines])

  def GetPatchset(self):
    """Returns the patchset number as a int or None if not set."""
    if self.patchset is None and not self.lookedup_patchset:
      if self.GetBranch():
        self.patchset = self._GitGetBranchConfigValue(PATCHSET_CONFIG_KEY)
      if self.patchset is not None:
        self.patchset = int(self.patchset)
      self.lookedup_patchset = True
    return self.patchset

  def GetAuthor(self):
    return scm.GIT.GetConfig(settings.GetRoot(), 'user.email')

  def SetPatchset(self, patchset):
    """Set this branch's patchset. If patchset=0, clears the patchset."""
    assert self.GetBranch()
    if not patchset:
      self.patchset = None
    else:
      self.patchset = int(patchset)
    self._GitSetBranchConfigValue(PATCHSET_CONFIG_KEY, str(self.patchset))

  def SetIssue(self, issue=None):
    """Set this branch's issue. If issue isn't given, clears the issue."""
    assert self.GetBranch()
    if issue:
      issue = int(issue)
      self._GitSetBranchConfigValue(ISSUE_CONFIG_KEY, str(issue))
      self.issue = issue
      codereview_server = self.GetCodereviewServer()
      if codereview_server:
        self._GitSetBranchConfigValue(
            CODEREVIEW_SERVER_CONFIG_KEY, codereview_server)
    else:
      # Reset all of these just to be clean.
      reset_suffixes = [
          LAST_UPLOAD_HASH_CONFIG_KEY,
          ISSUE_CONFIG_KEY,
          PATCHSET_CONFIG_KEY,
          CODEREVIEW_SERVER_CONFIG_KEY,
          GERRIT_SQUASH_HASH_CONFIG_KEY,
      ]
      for prop in reset_suffixes:
        try:
          self._GitSetBranchConfigValue(prop, None)
        except subprocess2.CalledProcessError:
          pass
      msg = RunGit(['log', '-1', '--format=%B']).strip()
      if msg and git_footers.get_footer_change_id(msg):
        print('WARNING: The change patched into this branch has a Change-Id. '
              'Removing it.')
        RunGit(['commit', '--amend', '-m',
                git_footers.remove_footer(msg, 'Change-Id')])
      self.lookedup_issue = True
      self.issue = None
      self.patchset = None

  def GetAffectedFiles(self, upstream, end_commit=None):
    # type: (str, Optional[str]) -> Sequence[str]
    """Returns the list of affected files for the given commit range."""
    try:
      return [
          f for _, f in scm.GIT.CaptureStatus(
              settings.GetRoot(), upstream, end_commit=end_commit)
      ]
    except subprocess2.CalledProcessError:
      DieWithError(
          ('\nFailed to diff against upstream branch %s\n\n'
           'This branch probably doesn\'t exist anymore. To reset the\n'
           'tracking branch, please run\n'
           '    git branch --set-upstream-to origin/main %s\n'
           'or replace origin/main with the relevant branch') %
          (upstream, self.GetBranch()))

  def UpdateDescription(self, description, force=False):
    assert self.GetIssue(), 'issue is required to update description'

    if gerrit_util.HasPendingChangeEdit(
        self.GetGerritHost(), self._GerritChangeIdentifier()):
      if not force:
        confirm_or_exit(
            'The description cannot be modified while the issue has a pending '
            'unpublished edit. Either publish the edit in the Gerrit web UI '
            'or delete it.\n\n', action='delete the unpublished edit')

      gerrit_util.DeletePendingChangeEdit(
        self.GetGerritHost(), self._GerritChangeIdentifier())
    gerrit_util.SetCommitMessage(
        self.GetGerritHost(), self._GerritChangeIdentifier(),
        description, notify='NONE')

    self.description = description

  def _GetCommonPresubmitArgs(self, verbose, upstream):
    args = [
        '--root', settings.GetRoot(),
        '--upstream', upstream,
    ]

    args.extend(['--verbose'] * verbose)

    remote, remote_branch = self.GetRemoteBranch()
    target_ref = GetTargetRef(remote, remote_branch, None)
    if settings.GetIsGerrit():
      args.extend(['--gerrit_url', self.GetCodereviewServer()])
      args.extend(['--gerrit_project', self.GetGerritProject()])
      args.extend(['--gerrit_branch', target_ref])

    author = self.GetAuthor()
    issue = self.GetIssue()
    patchset = self.GetPatchset()
    if author:
      args.extend(['--author', author])
    if issue:
      args.extend(['--issue', str(issue)])
    if patchset:
      args.extend(['--patchset', str(patchset)])

    return args

  def RunHook(self,
              committing,
              may_prompt,
              verbose,
              parallel,
              upstream,
              description,
              all_files,
              files=None,
              resultdb=False,
              realm=None):
    """Calls sys.exit() if the hook fails; returns a HookResults otherwise."""
    args = self._GetCommonPresubmitArgs(verbose, upstream)
    args.append('--commit' if committing else '--upload')
    if may_prompt:
      args.append('--may_prompt')
    if parallel:
      args.append('--parallel')
    if all_files:
      args.append('--all_files')
    if files:
      args.extend(files.split(';'))
      args.append('--source_controlled_only')
    if files or all_files:
      args.append('--no_diffs')

    if resultdb and not realm:
      # TODO (crbug.com/1113463): store realm somewhere and look it up so
      # it is not required to pass the realm flag
      print('Note: ResultDB reporting will NOT be performed because --realm'
            ' was not specified. To enable ResultDB, please run the command'
            ' again with the --realm argument to specify the LUCI realm.')

    py3_results = self._RunPresubmit(args,
                                     description,
                                     use_python3=True,
                                     resultdb=resultdb,
                                     realm=realm)
    if py3_results.get('skipped_presubmits', 1) == 0:
      print('No more presubmits to run - skipping Python 2 presubmits.')
      return py3_results

    py2_results = self._RunPresubmit(args,
                                     description,
                                     use_python3=False,
                                     resultdb=resultdb,
                                     realm=realm)
    return self._MergePresubmitResults(py2_results, py3_results)

  def _RunPresubmit(self,
                    args,
                    description,
                    use_python3,
                    resultdb=None,
                    realm=None):
    # type: (Sequence[str], str, bool, Optional[bool], Optional[str]
    #    ) -> Mapping[str, Any]
    args = args[:]
    vpython = 'vpython3' if use_python3 else 'vpython'

    with gclient_utils.temporary_file() as description_file:
      with gclient_utils.temporary_file() as json_output:
        gclient_utils.FileWrite(description_file, description)
        args.extend(['--json_output', json_output])
        args.extend(['--description_file', description_file])
        if self.GetUsePython3():
          args.append('--use-python3')
        start = time_time()
        cmd = [vpython, PRESUBMIT_SUPPORT] + args
        if resultdb and realm:
          cmd = ['rdb', 'stream', '-new', '-realm', realm, '--'] + cmd

        p = subprocess2.Popen(cmd)
        exit_code = p.wait()

        metrics.collector.add_repeated('sub_commands', {
          'command': 'presubmit',
          'execution_time': time_time() - start,
          'exit_code': exit_code,
        })

        if exit_code:
          sys.exit(exit_code)

        json_results = gclient_utils.FileRead(json_output)
        return json.loads(json_results)

  def _MergePresubmitResults(self, py2_results, py3_results):
    return {
        'more_cc': sorted(set(py2_results.get('more_cc', []) +
                              py3_results.get('more_cc', []))),
        'errors': (
            py2_results.get('errors', []) + py3_results.get('errors', [])),
        'notifications': (
            py2_results.get('notifications', []) +
            py3_results.get('notifications', [])),
        'warnings': (
            py2_results.get('warnings', []) + py3_results.get('warnings', []))
    }

  def RunPostUploadHook(self, verbose, upstream, description, py3_only):
    args = self._GetCommonPresubmitArgs(verbose, upstream)
    args.append('--post_upload')

    with gclient_utils.temporary_file() as description_file:
      gclient_utils.FileWrite(description_file, description)
      args.extend(['--description_file', description_file])
      run_py2 = not py3_only and os.getenv('LUCI_OMIT_PYTHON2') != 'true'
      if run_py2:
        p_py2 = subprocess2.Popen(['vpython', PRESUBMIT_SUPPORT] + args)
      p_py3 = subprocess2.Popen(['vpython3', PRESUBMIT_SUPPORT] + args +
                                ['--use-python3'])
      if run_py2:
        p_py2.wait()
      p_py3.wait()

  def _GetDescriptionForUpload(self, options, git_diff_args, files):
    # type: (optparse.Values, Sequence[str], Sequence[str]
    #     ) -> ChangeDescription
    """Get description message for upload."""
    if self.GetIssue():
      description = self.FetchDescription()
    elif options.message:
      description = options.message
    else:
      description = _create_description_from_log(git_diff_args)
      if options.title and options.squash:
        description = options.title + '\n\n' + description

    bug = options.bug
    fixed = options.fixed
    if not self.GetIssue():
      # Extract bug number from branch name, but only if issue is being created.
      # It must start with bug or fix, followed by _ or - and number.
      # Optionally, it may contain _ or - after number with arbitrary text.
      # Examples:
      #   bug-123
      #   bug_123
      #   fix-123
      #   fix-123-some-description
      branch = self.GetBranch()
      if branch is not None:
        match = re.match(
            r'^(?P<type>bug|fix(?:e[sd])?)[_-]?(?P<bugnum>\d+)([-_]|$)', branch)
        if not bug and not fixed and match:
          if match.group('type') == 'bug':
            bug = match.group('bugnum')
          else:
            fixed = match.group('bugnum')

    change_description = ChangeDescription(description, bug, fixed)

    # Fill gaps in OWNERS coverage to reviewers if requested.
    if options.add_owners_to:
      assert options.add_owners_to in ('R'), options.add_owners_to
      status = self.owners_client.GetFilesApprovalStatus(
          files, [], options.reviewers)
      missing_files = [
        f for f in files
        if status[f] == self._owners_client.INSUFFICIENT_REVIEWERS
      ]
      owners = self.owners_client.SuggestOwners(
          missing_files, exclude=[self.GetAuthor()])
      assert isinstance(options.reviewers, list), options.reviewers
      options.reviewers.extend(owners)

    # Set the reviewer list now so that presubmit checks can access it.
    if options.reviewers:
      change_description.update_reviewers(options.reviewers)

    return change_description

  def _GetTitleForUpload(self, options, multi_change_upload=False):
    # type: (optparse.Values, Optional[bool]) -> str

    # Getting titles for multipl commits is not supported so we return the
    # default.
    if not options.squash or multi_change_upload or options.title:
      return options.title

    # On first upload, patchset title is always this string, while options.title
    # gets converted to first line of message.
    if not self.GetIssue():
      return 'Initial upload'

    # When uploading subsequent patchsets, options.message is taken as the title
    # if options.title is not provided.
    if options.message:
      return options.message.strip()

    # Use the subject of the last commit as title by default.
    title = RunGit(['show', '-s', '--format=%s', 'HEAD']).strip()
    if options.force or options.skip_title:
      return title
    user_title = gclient_utils.AskForData('Title for patchset [%s]: ' % title)

    # Use the default title if the user confirms the default with a 'y'.
    if user_title.lower() == 'y':
      return title
    return user_title or title

  def _GetRefSpecOptions(self,
                         options,
                         change_desc,
                         multi_change_upload=False,
                         dogfood_path=False):
    # type: (optparse.Values, Sequence[Changelist], Optional[bool],
    #      Optional[bool]) -> Sequence[str]

    # Extra options that can be specified at push time. Doc:
    # https://gerrit-review.googlesource.com/Documentation/user-upload.html
    refspec_opts = []

    # By default, new changes are started in WIP mode, and subsequent patchsets
    # don't send email. At any time, passing --send-mail or --send-email will
    # mark the change ready and send email for that particular patch.
    if options.send_mail:
      refspec_opts.append('ready')
      refspec_opts.append('notify=ALL')
    elif (not self.GetIssue() and options.squash and not dogfood_path):
      refspec_opts.append('wip')
    else:
      refspec_opts.append('notify=NONE')

    # TODO(tandrii): options.message should be posted as a comment if
    # --send-mail or --send-email is set on non-initial upload as Rietveld used
    # to do it.

    # Set options.title in case user was prompted in _GetTitleForUpload and
    # _CMDUploadChange needs to be called again.
    options.title = self._GetTitleForUpload(
        options, multi_change_upload=multi_change_upload)

    if options.title:
      # Punctuation and whitespace in |title| must be percent-encoded.
      refspec_opts.append('m=' +
                          gerrit_util.PercentEncodeForGitRef(options.title))

    if options.private:
      refspec_opts.append('private')

    if options.topic:
      # Documentation on Gerrit topics is here:
      # https://gerrit-review.googlesource.com/Documentation/user-upload.html#topic
      refspec_opts.append('topic=%s' % options.topic)

    if options.enable_auto_submit:
      refspec_opts.append('l=Auto-Submit+1')
    if options.set_bot_commit:
      refspec_opts.append('l=Bot-Commit+1')
    if options.use_commit_queue:
      refspec_opts.append('l=Commit-Queue+2')
    elif options.cq_dry_run:
      refspec_opts.append('l=Commit-Queue+1')
    elif options.cq_quick_run:
      refspec_opts.append('l=Commit-Queue+1')
      refspec_opts.append('l=Quick-Run+1')

    if change_desc.get_reviewers(tbr_only=True):
      score = gerrit_util.GetCodeReviewTbrScore(self.GetGerritHost(),
                                                self.GetGerritProject())
      refspec_opts.append('l=Code-Review+%s' % score)

    # Gerrit sorts hashtags, so order is not important.
    hashtags = {change_desc.sanitize_hash_tag(t) for t in options.hashtags}
    # We check GetIssue because we only add hashtags from the
    # description on the first upload.
    # TODO(b/265929888): When we fully launch the new path:
    # 1) remove fetching hashtags from description alltogether
    # 2) Or use descrtiption hashtags for:
    #    `not (self.GetIssue() and multi_change_upload)`
    # 3) Or enabled change description tags for multi and single changes
    #    by adding them post `git push`.
    if not (self.GetIssue() and dogfood_path):
      hashtags.update(change_desc.get_hash_tags())
    refspec_opts.extend(['hashtag=%s' % t for t in hashtags])

    # Note: Reviewers, and ccs are handled individually for each
    # branch/change.
    return refspec_opts

  def PrepareSquashedCommit(self,
                            options,
                            parent,
                            orig_parent,
                            end_commit=None):
    # type: (optparse.Values, str, str, Optional[str]) -> _NewUpload()
    """Create a squashed commit to upload.


      Args:
        parent: The commit to use as the parent for the new squashed.
        orig_parent: The commit that is an actual ancestor of `end_commit`. It
            is part of the same original tree as end_commit, which does not
            contain squashed commits. This is used to create the change
            description for the new squashed commit with:
            `git log orig_parent..end_commit`.
        end_commit: The commit to use as the end of the new squashed commit.
    """

    if end_commit is None:
      end_commit = RunGit(['rev-parse', self.branchref]).strip()

    reviewers, ccs, change_desc = self._PrepareChange(options, orig_parent,
                                                      end_commit)
    latest_tree = RunGit(['rev-parse', end_commit + ':']).strip()
    with gclient_utils.temporary_file() as desc_tempfile:
      gclient_utils.FileWrite(desc_tempfile, change_desc.description)
      commit_to_push = RunGit(
          ['commit-tree', latest_tree, '-p', parent, '-F',
           desc_tempfile]).strip()

    # Gerrit may or may not update fast enough to return the correct patchset
    # number after we push. Get the pre-upload patchset and increment later.
    prev_patchset = self.GetMostRecentPatchset(update=False) or 0
    return _NewUpload(reviewers, ccs, commit_to_push, end_commit, parent,
                      change_desc, prev_patchset)

  def PrepareCherryPickSquashedCommit(self, options, parent):
    # type: (optparse.Values, str) -> _NewUpload()
    """Create a commit cherry-picked on parent to push."""

    # The `parent` is what we will cherry-pick on top of.
    # The `cherry_pick_base` is the beginning range of what
    # we are cherry-picking.
    cherry_pick_base = self.GetCommonAncestorWithUpstream()
    reviewers, ccs, change_desc = self._PrepareChange(options, cherry_pick_base,
                                                      self.branchref)

    new_upload_hash = RunGit(['rev-parse', self.branchref]).strip()
    latest_tree = RunGit(['rev-parse', self.branchref + ':']).strip()
    with gclient_utils.temporary_file() as desc_tempfile:
      gclient_utils.FileWrite(desc_tempfile, change_desc.description)
      commit_to_cp = RunGit([
          'commit-tree', latest_tree, '-p', cherry_pick_base, '-F',
          desc_tempfile
      ]).strip()

    RunGit(['checkout', '-q', parent])
    ret, _out = RunGitWithCode(['cherry-pick', commit_to_cp])
    if ret:
      RunGit(['cherry-pick', '--abort'])
      RunGit(['checkout', '-q', self.branch])
      DieWithError('Could not cleanly cherry-pick')

    commit_to_push = RunGit(['rev-parse', 'HEAD']).strip()
    RunGit(['checkout', '-q', self.branch])

    # Gerrit may or may not update fast enough to return the correct patchset
    # number after we push. Get the pre-upload patchset and increment later.
    prev_patchset = self.GetMostRecentPatchset(update=False) or 0
    return _NewUpload(reviewers, ccs, commit_to_push, new_upload_hash,
                      cherry_pick_base, change_desc, prev_patchset)

  def _PrepareChange(self, options, parent, end_commit):
    # type: (optparse.Values, str, str) ->
    #     Tuple[Sequence[str], Sequence[str], ChangeDescription]
    """Prepares the change to be uploaded."""
    self.EnsureCanUploadPatchset(options.force)

    files = self.GetAffectedFiles(parent, end_commit=end_commit)
    change_desc = self._GetDescriptionForUpload(options, [parent, end_commit],
                                                files)

    watchlist = watchlists.Watchlists(settings.GetRoot())
    self.ExtendCC(watchlist.GetWatchersForPaths(files))
    if not options.bypass_hooks:
      hook_results = self.RunHook(committing=False,
                                  may_prompt=not options.force,
                                  verbose=options.verbose,
                                  parallel=options.parallel,
                                  upstream=parent,
                                  description=change_desc.description,
                                  all_files=False)
      self.ExtendCC(hook_results['more_cc'])

    # Update the change description and ensure we have a Change Id.
    if self.GetIssue():
      if options.edit_description:
        change_desc.prompt()
      change_detail = self._GetChangeDetail(['CURRENT_REVISION'])
      change_id = change_detail['change_id']
      change_desc.ensure_change_id(change_id)

    else:  # No change issue. First time uploading
      if not options.force and not options.message_file:
        change_desc.prompt()

      # Check if user added a change_id in the descripiton.
      change_ids = git_footers.get_footer_change_id(change_desc.description)
      if len(change_ids) == 1:
        change_id = change_ids[0]
      else:
        change_id = GenerateGerritChangeId(change_desc.description)
        change_desc.ensure_change_id(change_id)

    if options.preserve_tryjobs:
      change_desc.set_preserve_tryjobs()

    SaveDescriptionBackup(change_desc)

    # Add ccs
    ccs = []
    # Add default, watchlist, presubmit ccs if this is the initial upload
    # and CL is not private and auto-ccing has not been disabled.
    if not options.private and not options.no_autocc and not self.GetIssue():
      ccs = self.GetCCList().split(',')
      if len(ccs) > 100:
        lsc = ('https://chromium.googlesource.com/chromium/src/+/HEAD/docs/'
               'process/lsc/lsc_workflow.md')
        print('WARNING: This will auto-CC %s users.' % len(ccs))
        print('LSC may be more appropriate: %s' % lsc)
        print('You can also use the --no-autocc flag to disable auto-CC.')
        confirm_or_exit(action='continue')

    # Add ccs from the --cc flag.
    if options.cc:
      ccs.extend(options.cc)

    ccs = [email.strip() for email in ccs if email.strip()]
    if change_desc.get_cced():
      ccs.extend(change_desc.get_cced())

    return change_desc.get_reviewers(), ccs, change_desc

  def PostUploadUpdates(self, options, new_upload, change_number):
    # type: (optparse.Values, _NewUpload, change_number) -> None
    """Makes necessary post upload changes to the local and remote cl."""
    if not self.GetIssue():
      self.SetIssue(change_number)

    self.SetPatchset(new_upload.prev_patchset + 1)

    self._GitSetBranchConfigValue(GERRIT_SQUASH_HASH_CONFIG_KEY,
                                  new_upload.commit_to_push)
    self._GitSetBranchConfigValue(LAST_UPLOAD_HASH_CONFIG_KEY,
                                  new_upload.new_last_uploaded_commit)

    if settings.GetRunPostUploadHook():
      self.RunPostUploadHook(options.verbose, new_upload.parent,
                             new_upload.change_desc.description,
                             options.no_python2_post_upload_hooks)

    if new_upload.reviewers or new_upload.ccs:
      gerrit_util.AddReviewers(self.GetGerritHost(),
                               self._GerritChangeIdentifier(),
                               reviewers=new_upload.reviewers,
                               ccs=new_upload.ccs,
                               notify=bool(options.send_mail))

  def CMDUpload(self, options, git_diff_args, orig_args):
    """Uploads a change to codereview."""
    custom_cl_base = None
    if git_diff_args:
      custom_cl_base = base_branch = git_diff_args[0]
    else:
      if self.GetBranch() is None:
        DieWithError('Can\'t upload from detached HEAD state. Get on a branch!')

      # Default to diffing against common ancestor of upstream branch
      base_branch = self.GetCommonAncestorWithUpstream()
      git_diff_args = [base_branch, 'HEAD']

    # Fast best-effort checks to abort before running potentially expensive
    # hooks if uploading is likely to fail anyway. Passing these checks does
    # not guarantee that uploading will not fail.
    self.EnsureAuthenticated(force=options.force)
    self.EnsureCanUploadPatchset(force=options.force)

    # Apply watchlists on upload.
    watchlist = watchlists.Watchlists(settings.GetRoot())
    files = self.GetAffectedFiles(base_branch)
    if not options.bypass_watchlists:
      self.ExtendCC(watchlist.GetWatchersForPaths(files))

    change_desc = self._GetDescriptionForUpload(options, git_diff_args, files)
    if not options.bypass_hooks:
      hook_results = self.RunHook(committing=False,
                                  may_prompt=not options.force,
                                  verbose=options.verbose,
                                  parallel=options.parallel,
                                  upstream=base_branch,
                                  description=change_desc.description,
                                  all_files=False)
      self.ExtendCC(hook_results['more_cc'])

    print_stats(git_diff_args)
    ret = self.CMDUploadChange(
        options, git_diff_args, custom_cl_base, change_desc)
    if not ret:
      if self.GetBranch() is not None:
        self._GitSetBranchConfigValue(
            LAST_UPLOAD_HASH_CONFIG_KEY,
            scm.GIT.ResolveCommit(settings.GetRoot(), 'HEAD'))
      # Run post upload hooks, if specified.
      if settings.GetRunPostUploadHook():
        self.RunPostUploadHook(options.verbose, base_branch,
                               change_desc.description,
                               options.no_python2_post_upload_hooks)

      # Upload all dependencies if specified.
      if options.dependencies:
        print()
        print('--dependencies has been specified.')
        print('All dependent local branches will be re-uploaded.')
        print()
        # Remove the dependencies flag from args so that we do not end up in a
        # loop.
        orig_args.remove('--dependencies')
        ret = upload_branch_deps(self, orig_args, options.force)
    return ret

  def SetCQState(self, new_state):
    """Updates the CQ state for the latest patchset.

    Issue must have been already uploaded and known. Optionally allows for
    updating Quick-Run (QR) state.
    """
    assert new_state in _CQState.ALL_STATES
    assert self.GetIssue()
    try:
      vote_map = {
        _CQState.NONE: 0,
        _CQState.QUICK_RUN: 1,
        _CQState.DRY_RUN: 1,
        _CQState.COMMIT: 2,
      }
      if new_state == _CQState.QUICK_RUN:
        labels = {
            'Commit-Queue': vote_map[_CQState.DRY_RUN],
            'Quick-Run': vote_map[_CQState.QUICK_RUN],
        }
      else:
        labels = {'Commit-Queue': vote_map[new_state]}
      notify = False if new_state == _CQState.DRY_RUN else None
      gerrit_util.SetReview(
          self.GetGerritHost(), self._GerritChangeIdentifier(),
          labels=labels, notify=notify)
      return 0
    except KeyboardInterrupt:
      raise
    except:
      print('WARNING: Failed to %s.\n'
            'Either:\n'
            ' * Your project has no CQ,\n'
            ' * You don\'t have permission to change the CQ state,\n'
            ' * There\'s a bug in this code (see stack trace below).\n'
            'Consider specifying which bots to trigger manually or asking your '
            'project owners for permissions or contacting Chrome Infra at:\n'
            'https://www.chromium.org/infra\n\n' %
            ('cancel CQ' if new_state == _CQState.NONE else 'trigger CQ'))
      # Still raise exception so that stack trace is printed.
      raise

  def GetGerritHost(self):
    # Lazy load of configs.
    self.GetCodereviewServer()
    if self._gerrit_host and '.' not in self._gerrit_host:
      # Abbreviated domain like "chromium" instead of chromium.googlesource.com.
      # This happens for internal stuff http://crbug.com/614312.
      parsed = urllib.parse.urlparse(self.GetRemoteUrl())
      if parsed.scheme == 'sso':
        print('WARNING: using non-https URLs for remote is likely broken\n'
              '  Your current remote is: %s' % self.GetRemoteUrl())
        self._gerrit_host = '%s.googlesource.com' % self._gerrit_host
        self._gerrit_server = 'https://%s' % self._gerrit_host
    return self._gerrit_host

  def _GetGitHost(self):
    """Returns git host to be used when uploading change to Gerrit."""
    remote_url = self.GetRemoteUrl()
    if not remote_url:
      return None
    return urllib.parse.urlparse(remote_url).netloc

  def GetCodereviewServer(self):
    if not self._gerrit_server:
      # If we're on a branch then get the server potentially associated
      # with that branch.
      if self.GetIssue() and self.GetBranch():
        self._gerrit_server = self._GitGetBranchConfigValue(
            CODEREVIEW_SERVER_CONFIG_KEY)
        if self._gerrit_server:
          self._gerrit_host = urllib.parse.urlparse(self._gerrit_server).netloc
      if not self._gerrit_server:
        # We assume repo to be hosted on Gerrit, and hence Gerrit server
        # has "-review" suffix for lowest level subdomain.
        parts = self._GetGitHost().split('.')
        parts[0] = parts[0] + '-review'
        self._gerrit_host = '.'.join(parts)
        self._gerrit_server = 'https://%s' % self._gerrit_host
    return self._gerrit_server

  def GetGerritProject(self):
    """Returns Gerrit project name based on remote git URL."""
    remote_url = self.GetRemoteUrl()
    if remote_url is None:
      logging.warning('can\'t detect Gerrit project.')
      return None
    project = urllib.parse.urlparse(remote_url).path.strip('/')
    if project.endswith('.git'):
      project = project[:-len('.git')]
    # *.googlesource.com hosts ensure that Git/Gerrit projects don't start with
    # 'a/' prefix, because 'a/' prefix is used to force authentication in
    # gitiles/git-over-https protocol. E.g.,
    # https://chromium.googlesource.com/a/v8/v8 refers to the same repo/project
    # as
    # https://chromium.googlesource.com/v8/v8
    if project.startswith('a/'):
      project = project[len('a/'):]
    return project

  def _GerritChangeIdentifier(self):
    """Handy method for gerrit_util.ChangeIdentifier for a given CL.

    Not to be confused by value of "Change-Id:" footer.
    If Gerrit project can be determined, this will speed up Gerrit HTTP API RPC.
    """
    project = self.GetGerritProject()
    if project:
      return gerrit_util.ChangeIdentifier(project, self.GetIssue())
    # Fall back on still unique, but less efficient change number.
    return str(self.GetIssue())

  def EnsureAuthenticated(self, force, refresh=None):
    """Best effort check that user is authenticated with Gerrit server."""
    if settings.GetGerritSkipEnsureAuthenticated():
      # For projects with unusual authentication schemes.
      # See http://crbug.com/603378.
      return

    # Check presence of cookies only if using cookies-based auth method.
    cookie_auth = gerrit_util.Authenticator.get()
    if not isinstance(cookie_auth, gerrit_util.CookiesAuthenticator):
      return

    remote_url = self.GetRemoteUrl()
    if remote_url is None:
      logging.warning('invalid remote')
      return
    if urllib.parse.urlparse(remote_url).scheme not in ['https', 'sso']:
      logging.warning(
          'Ignoring branch %(branch)s with non-https/sso remote '
          '%(remote)s', {
              'branch': self.branch,
              'remote': self.GetRemoteUrl()
          })
      return

    # Lazy-loader to identify Gerrit and Git hosts.
    self.GetCodereviewServer()
    git_host = self._GetGitHost()
    assert self._gerrit_server and self._gerrit_host and git_host

    gerrit_auth = cookie_auth.get_auth_header(self._gerrit_host)
    git_auth = cookie_auth.get_auth_header(git_host)
    if gerrit_auth and git_auth:
      if gerrit_auth == git_auth:
        return
      all_gsrc = cookie_auth.get_auth_header('d0esN0tEx1st.googlesource.com')
      print(
          'WARNING: You have different credentials for Gerrit and git hosts:\n'
          '           %s\n'
          '           %s\n'
          '        Consider running the following command:\n'
          '          git cl creds-check\n'
          '        %s\n'
          '        %s' %
          (git_host, self._gerrit_host,
           ('Hint: delete creds for .googlesource.com' if all_gsrc else ''),
           cookie_auth.get_new_password_message(git_host)))
      if not force:
        confirm_or_exit('If you know what you are doing', action='continue')
      return

    missing = (
        ([] if gerrit_auth else [self._gerrit_host]) +
        ([] if git_auth else [git_host]))
    DieWithError('Credentials for the following hosts are required:\n'
                 '  %s\n'
                 'These are read from %s (or legacy %s)\n'
                 '%s' % (
                   '\n  '.join(missing),
                   cookie_auth.get_gitcookies_path(),
                   cookie_auth.get_netrc_path(),
                   cookie_auth.get_new_password_message(git_host)))

  def EnsureCanUploadPatchset(self, force):
    if not self.GetIssue():
      return

    status = self._GetChangeDetail()['status']
    if status == 'ABANDONED':
      DieWithError(
          'Change %s has been abandoned, new uploads are not allowed' %
          (self.GetIssueURL()))
    if status == 'MERGED':
      answer = gclient_utils.AskForData(
          'Change %s has been submitted, new uploads are not allowed. '
          'Would you like to start a new change (Y/n)?' % self.GetIssueURL()
      ).lower()
      if answer not in ('y', ''):
        DieWithError('New uploads are not allowed.')
      self.SetIssue()
      return

    # TODO(vadimsh): For some reason the chunk of code below was skipped if
    # 'is_gce' is True. I'm just refactoring it to be 'skip if not cookies'.
    # Apparently this check is not very important? Otherwise get_auth_email
    # could have been added to other implementations of Authenticator.
    cookies_auth = gerrit_util.Authenticator.get()
    if not isinstance(cookies_auth, gerrit_util.CookiesAuthenticator):
      return

    cookies_user = cookies_auth.get_auth_email(self.GetGerritHost())
    if self.GetIssueOwner() == cookies_user:
      return
    logging.debug('change %s owner is %s, cookies user is %s',
                  self.GetIssue(), self.GetIssueOwner(), cookies_user)
    # Maybe user has linked accounts or something like that,
    # so ask what Gerrit thinks of this user.
    details = gerrit_util.GetAccountDetails(self.GetGerritHost(), 'self')
    if details['email'] == self.GetIssueOwner():
      return
    if not force:
      print('WARNING: Change %s is owned by %s, but you authenticate to Gerrit '
            'as %s.\n'
            'Uploading may fail due to lack of permissions.' %
            (self.GetIssue(), self.GetIssueOwner(), details['email']))
      confirm_or_exit(action='upload')

  def GetStatus(self):
    """Applies a rough heuristic to give a simple summary of an issue's review
    or CQ status, assuming adherence to a common workflow.

    Returns None if no issue for this branch, or one of the following keywords:
      * 'error'   - error from review tool (including deleted issues)
      * 'unsent'  - no reviewers added
      * 'waiting' - waiting for review
      * 'reply'   - waiting for uploader to reply to review
      * 'lgtm'    - Code-Review label has been set
      * 'dry-run' - dry-running in the CQ
      * 'commit'  - in the CQ
      * 'closed'  - successfully submitted or abandoned
    """
    if not self.GetIssue():
      return None

    try:
      data = self._GetChangeDetail([
          'DETAILED_LABELS', 'CURRENT_REVISION', 'SUBMITTABLE'])
    except GerritChangeNotExists:
      return 'error'

    if data['status'] in ('ABANDONED', 'MERGED'):
      return 'closed'

    cq_label = data['labels'].get('Commit-Queue', {})
    max_cq_vote = 0
    for vote in cq_label.get('all', []):
      max_cq_vote = max(max_cq_vote, vote.get('value', 0))
    if max_cq_vote == 2:
      return 'commit'
    if max_cq_vote == 1:
      return 'dry-run'

    if data['labels'].get('Code-Review', {}).get('approved'):
      return 'lgtm'

    if not data.get('reviewers', {}).get('REVIEWER', []):
      return 'unsent'

    owner = data['owner'].get('_account_id')
    messages = sorted(data.get('messages', []), key=lambda m: m.get('date'))
    while messages:
      m = messages.pop()
      if (m.get('tag', '').startswith('autogenerated:cq') or
          m.get('tag', '').startswith('autogenerated:cv')):
        # Ignore replies from LUCI CV/CQ.
        continue
      if m.get('author', {}).get('_account_id') == owner:
        # Most recent message was by owner.
        return 'waiting'

      # Some reply from non-owner.
      return 'reply'

    # Somehow there are no messages even though there are reviewers.
    return 'unsent'

  def GetMostRecentPatchset(self, update=True):
    if not self.GetIssue():
      return None

    data = self._GetChangeDetail(['CURRENT_REVISION'])
    patchset = data['revisions'][data['current_revision']]['_number']
    if update:
      self.SetPatchset(patchset)
    return patchset

  def _IsPatchsetRangeSignificant(self, lower, upper):
    """Returns True if the inclusive range of patchsets contains any reworks or
    rebases."""
    if not self.GetIssue():
      return False

    data = self._GetChangeDetail(['ALL_REVISIONS'])
    ps_kind = {}
    for rev_info in data.get('revisions', {}).values():
      ps_kind[rev_info['_number']] = rev_info.get('kind', '')

    for ps in range(lower, upper + 1):
      assert ps in ps_kind, 'expected patchset %d in change detail' % ps
      if ps_kind[ps] not in ('NO_CHANGE', 'NO_CODE_CHANGE'):
        return True
    return False

  def GetMostRecentDryRunPatchset(self):
    """Get patchsets equivalent to the most recent patchset and return
    the patchset with the latest dry run. If none have been dry run, return
    the latest patchset."""
    if not self.GetIssue():
      return None

    data = self._GetChangeDetail(['ALL_REVISIONS'])
    patchset = data['revisions'][data['current_revision']]['_number']
    dry_run = {int(m['_revision_number'])
               for m in data.get('messages', [])
               if m.get('tag', '').endswith('dry-run')}

    for revision_info in sorted(data.get('revisions', {}).values(),
        key=lambda c: c['_number'], reverse=True):
      if revision_info['_number'] in dry_run:
        patchset = revision_info['_number']
        break
      if revision_info.get('kind', '') not in \
          ('NO_CHANGE', 'NO_CODE_CHANGE', 'TRIVIAL_REBASE'):
        break
    self.SetPatchset(patchset)
    return patchset

  def AddComment(self, message, publish=None):
    gerrit_util.SetReview(
        self.GetGerritHost(), self._GerritChangeIdentifier(),
        msg=message, ready=publish)

  def GetCommentsSummary(self, readable=True):
    # DETAILED_ACCOUNTS is to get emails in accounts.
    # CURRENT_REVISION is included to get the latest patchset so that
    # only the robot comments from the latest patchset can be shown.
    messages = self._GetChangeDetail(
        options=['MESSAGES', 'DETAILED_ACCOUNTS',
                 'CURRENT_REVISION']).get('messages', [])
    file_comments = gerrit_util.GetChangeComments(
        self.GetGerritHost(), self._GerritChangeIdentifier())
    robot_file_comments = gerrit_util.GetChangeRobotComments(
        self.GetGerritHost(), self._GerritChangeIdentifier())

    # Add the robot comments onto the list of comments, but only
    # keep those that are from the latest patchset.
    latest_patch_set = self.GetMostRecentPatchset()
    for path, robot_comments in robot_file_comments.items():
      line_comments = file_comments.setdefault(path, [])
      line_comments.extend(
          [c for c in robot_comments if c['patch_set'] == latest_patch_set])

    # Build dictionary of file comments for easy access and sorting later.
    # {author+date: {path: {patchset: {line: url+message}}}}
    comments = collections.defaultdict(
        lambda: collections.defaultdict(lambda: collections.defaultdict(dict)))

    server = self.GetCodereviewServer()
    if server in _KNOWN_GERRIT_TO_SHORT_URLS:
      # /c/ is automatically added by short URL server.
      url_prefix = '%s/%s' % (_KNOWN_GERRIT_TO_SHORT_URLS[server],
                              self.GetIssue())
    else:
      url_prefix = '%s/c/%s' % (server, self.GetIssue())

    for path, line_comments in file_comments.items():
      for comment in line_comments:
        tag = comment.get('tag', '')
        if tag.startswith('autogenerated') and 'robot_id' not in comment:
          continue
        key = (comment['author']['email'], comment['updated'])
        if comment.get('side', 'REVISION') == 'PARENT':
          patchset = 'Base'
        else:
          patchset = 'PS%d' % comment['patch_set']
        line = comment.get('line', 0)
        url = ('%s/%s/%s#%s%s' %
               (url_prefix, comment['patch_set'], path,
                'b' if comment.get('side') == 'PARENT' else '',
                str(line) if line else ''))
        comments[key][path][patchset][line] = (url, comment['message'])

    summaries = []
    for msg in messages:
      summary = self._BuildCommentSummary(msg, comments, readable)
      if summary:
        summaries.append(summary)
    return summaries

  @staticmethod
  def _BuildCommentSummary(msg, comments, readable):
    if 'email' not in msg['author']:
      # Some bot accounts may not have an email associated.
      return None

    key = (msg['author']['email'], msg['date'])
    # Don't bother showing autogenerated messages that don't have associated
    # file or line comments. this will filter out most autogenerated
    # messages, but will keep robot comments like those from Tricium.
    is_autogenerated = msg.get('tag', '').startswith('autogenerated')
    if is_autogenerated and not comments.get(key):
      return None
    message = msg['message']
    # Gerrit spits out nanoseconds.
    assert len(msg['date'].split('.')[-1]) == 9
    date = datetime.datetime.strptime(msg['date'][:-3],
                                      '%Y-%m-%d %H:%M:%S.%f')
    if key in comments:
      message += '\n'
    for path, patchsets in sorted(comments.get(key, {}).items()):
      if readable:
        message += '\n%s' % path
      for patchset, lines in sorted(patchsets.items()):
        for line, (url, content) in sorted(lines.items()):
          if line:
            line_str = 'Line %d' % line
            path_str = '%s:%d:' % (path, line)
          else:
            line_str = 'File comment'
            path_str = '%s:0:' % path
          if readable:
            message += '\n  %s, %s: %s' % (patchset, line_str, url)
            message += '\n  %s\n' % content
          else:
            message += '\n%s ' % path_str
            message += '\n%s\n' % content

    return _CommentSummary(
      date=date,
      message=message,
      sender=msg['author']['email'],
      autogenerated=is_autogenerated,
      # These could be inferred from the text messages and correlated with
      # Code-Review label maximum, however this is not reliable.
      # Leaving as is until the need arises.
      approval=False,
      disapproval=False,
    )

  def CloseIssue(self):
    gerrit_util.AbandonChange(
        self.GetGerritHost(), self._GerritChangeIdentifier(), msg='')

  def SubmitIssue(self):
    gerrit_util.SubmitChange(
        self.GetGerritHost(), self._GerritChangeIdentifier())

  def _GetChangeDetail(self, options=None):
    """Returns details of associated Gerrit change and caching results."""
    options = options or []
    assert self.GetIssue(), 'issue is required to query Gerrit'

    # Optimization to avoid multiple RPCs:
    if 'CURRENT_REVISION' in options or 'ALL_REVISIONS' in options:
      options.append('CURRENT_COMMIT')

    # Normalize issue and options for consistent keys in cache.
    cache_key = str(self.GetIssue())
    options_set = frozenset(o.upper() for o in options)

    for cached_options_set, data in self._detail_cache.get(cache_key, []):
      # Assumption: data fetched before with extra options is suitable
      # for return for a smaller set of options.
      # For example, if we cached data for
      #     options=[CURRENT_REVISION, DETAILED_FOOTERS]
      #   and request is for options=[CURRENT_REVISION],
      # THEN we can return prior cached data.
      if options_set.issubset(cached_options_set):
        return data

    try:
      data = gerrit_util.GetChangeDetail(
          self.GetGerritHost(), self._GerritChangeIdentifier(), options_set)
    except gerrit_util.GerritError as e:
      if e.http_status == 404:
        raise GerritChangeNotExists(self.GetIssue(), self.GetCodereviewServer())
      raise

    self._detail_cache.setdefault(cache_key, []).append((options_set, data))
    return data

  def _GetChangeCommit(self, revision='current'):
    assert self.GetIssue(), 'issue must be set to query Gerrit'
    try:
      data = gerrit_util.GetChangeCommit(self.GetGerritHost(),
                                         self._GerritChangeIdentifier(),
                                         revision)
    except gerrit_util.GerritError as e:
      if e.http_status == 404:
        raise GerritChangeNotExists(self.GetIssue(), self.GetCodereviewServer())
      raise
    return data

  def _IsCqConfigured(self):
    detail = self._GetChangeDetail(['LABELS'])
    return u'Commit-Queue' in detail.get('labels', {})

  def CMDLand(self, force, bypass_hooks, verbose, parallel, resultdb, realm):
    if git_common.is_dirty_git_tree('land'):
      return 1

    detail = self._GetChangeDetail(['CURRENT_REVISION', 'LABELS'])
    if not force and self._IsCqConfigured():
      confirm_or_exit('\nIt seems this repository has a CQ, '
                        'which can test and land changes for you. '
                        'Are you sure you wish to bypass it?\n',
                        action='bypass CQ')
    differs = True
    last_upload = self._GitGetBranchConfigValue(GERRIT_SQUASH_HASH_CONFIG_KEY)
    # Note: git diff outputs nothing if there is no diff.
    if not last_upload or RunGit(['diff', last_upload]).strip():
      print('WARNING: Some changes from local branch haven\'t been uploaded.')
    else:
      if detail['current_revision'] == last_upload:
        differs = False
      else:
        print('WARNING: Local branch contents differ from latest uploaded '
              'patchset.')
    if differs:
      if not force:
        confirm_or_exit(
            'Do you want to submit latest Gerrit patchset and bypass hooks?\n',
            action='submit')
      print('WARNING: Bypassing hooks and submitting latest uploaded patchset.')
    elif not bypass_hooks:
      upstream = self.GetCommonAncestorWithUpstream()
      if self.GetIssue():
        description = self.FetchDescription()
      else:
        description = _create_description_from_log([upstream])
      self.RunHook(
          committing=True,
          may_prompt=not force,
          verbose=verbose,
          parallel=parallel,
          upstream=upstream,
          description=description,
          all_files=False,
          resultdb=resultdb,
          realm=realm)

    self.SubmitIssue()
    print('Issue %s has been submitted.' % self.GetIssueURL())
    links = self._GetChangeCommit().get('web_links', [])
    for link in links:
      if link.get('name') in ['gitiles', 'browse'] and link.get('url'):
        print('Landed as: %s' % link.get('url'))
        break
    return 0

  def CMDPatchWithParsedIssue(self, parsed_issue_arg, nocommit, force,
                              newbranch):
    assert parsed_issue_arg.valid

    self.issue = parsed_issue_arg.issue

    if parsed_issue_arg.hostname:
      self._gerrit_host = parsed_issue_arg.hostname
      self._gerrit_server = 'https://%s' % self._gerrit_host

    try:
      detail = self._GetChangeDetail(['ALL_REVISIONS'])
    except GerritChangeNotExists as e:
      DieWithError(str(e))

    if not parsed_issue_arg.patchset:
      # Use current revision by default.
      revision_info = detail['revisions'][detail['current_revision']]
      patchset = int(revision_info['_number'])
    else:
      patchset = parsed_issue_arg.patchset
      for revision_info in detail['revisions'].values():
        if int(revision_info['_number']) == parsed_issue_arg.patchset:
          break
      else:
        DieWithError('Couldn\'t find patchset %i in change %i' %
                     (parsed_issue_arg.patchset, self.GetIssue()))

    remote_url = self.GetRemoteUrl()
    if remote_url.endswith('.git'):
      remote_url = remote_url[:-len('.git')]
    remote_url = remote_url.rstrip('/')

    fetch_info = revision_info['fetch']['http']
    fetch_info['url'] = fetch_info['url'].rstrip('/')

    if remote_url != fetch_info['url']:
      DieWithError('Trying to patch a change from %s but this repo appears '
                   'to be %s.' % (fetch_info['url'], remote_url))

    RunGit(['fetch', fetch_info['url'], fetch_info['ref']])

    # Set issue immediately in case the cherry-pick fails, which happens
    # when resolving conflicts.
    if self.GetBranch():
      self.SetIssue(parsed_issue_arg.issue)

    if force:
      RunGit(['reset', '--hard', 'FETCH_HEAD'])
      print('Checked out commit for change %i patchset %i locally' %
            (parsed_issue_arg.issue, patchset))
    elif nocommit:
      RunGit(['cherry-pick', '--no-commit', 'FETCH_HEAD'])
      print('Patch applied to index.')
    else:
      RunGit(['cherry-pick', 'FETCH_HEAD'])
      print('Committed patch for change %i patchset %i locally.' %
            (parsed_issue_arg.issue, patchset))
      print('Note: this created a local commit which does not have '
            'the same hash as the one uploaded for review. This will make '
            'uploading changes based on top of this branch difficult.\n'
            'If you want to do that, use "git cl patch --force" instead.')

    if self.GetBranch():
      self.SetPatchset(patchset)
      fetched_hash = scm.GIT.ResolveCommit(settings.GetRoot(), 'FETCH_HEAD')
      self._GitSetBranchConfigValue(LAST_UPLOAD_HASH_CONFIG_KEY, fetched_hash)
      self._GitSetBranchConfigValue(GERRIT_SQUASH_HASH_CONFIG_KEY, fetched_hash)
    else:
      print('WARNING: You are in detached HEAD state.\n'
            'The patch has been applied to your checkout, but you will not be '
            'able to upload a new patch set to the gerrit issue.\n'
            'Try using the \'-b\' option if you would like to work on a '
            'branch and/or upload a new patch set.')

    return 0

  @staticmethod
  def _GerritCommitMsgHookCheck(offer_removal):
    # type: (bool) -> None
    """Checks for the gerrit's commit-msg hook and removes it if necessary."""
    hook = os.path.join(settings.GetRoot(), '.git', 'hooks', 'commit-msg')
    if not os.path.exists(hook):
      return
    # Crude attempt to distinguish Gerrit Codereview hook from a potentially
    # custom developer-made one.
    data = gclient_utils.FileRead(hook)
    if not('From Gerrit Code Review' in data and 'add_ChangeId()' in data):
      return
    print('WARNING: You have Gerrit commit-msg hook installed.\n'
          'It is not necessary for uploading with git cl in squash mode, '
          'and may interfere with it in subtle ways.\n'
          'We recommend you remove the commit-msg hook.')
    if offer_removal:
      if ask_for_explicit_yes('Do you want to remove it now?'):
        gclient_utils.rm_file_or_tree(hook)
        print('Gerrit commit-msg hook removed.')
      else:
        print('OK, will keep Gerrit commit-msg hook in place.')

  def _CleanUpOldTraces(self):
    """Keep only the last |MAX_TRACES| traces."""
    try:
      traces = sorted([
        os.path.join(TRACES_DIR, f)
        for f in os.listdir(TRACES_DIR)
        if (os.path.isfile(os.path.join(TRACES_DIR, f))
            and not f.startswith('tmp'))
      ])
      traces_to_delete = traces[:-MAX_TRACES]
      for trace in traces_to_delete:
        os.remove(trace)
    except OSError:
      print('WARNING: Failed to remove old git traces from\n'
            '  %s'
            'Consider removing them manually.' % TRACES_DIR)

  def _WriteGitPushTraces(self, trace_name, traces_dir, git_push_metadata):
    """Zip and write the git push traces stored in traces_dir."""
    gclient_utils.safe_makedirs(TRACES_DIR)
    traces_zip = trace_name + '-traces'
    traces_readme = trace_name + '-README'
    # Create a temporary dir to store git config and gitcookies in. It will be
    # compressed and stored next to the traces.
    git_info_dir = tempfile.mkdtemp()
    git_info_zip = trace_name + '-git-info'

    git_push_metadata['now'] = datetime_now().strftime('%Y-%m-%dT%H:%M:%S.%f')

    git_push_metadata['trace_name'] = trace_name
    gclient_utils.FileWrite(
        traces_readme, TRACES_README_FORMAT % git_push_metadata)

    # Keep only the first 6 characters of the git hashes on the packet
    # trace. This greatly decreases size after compression.
    packet_traces = os.path.join(traces_dir, 'trace-packet')
    if os.path.isfile(packet_traces):
      contents = gclient_utils.FileRead(packet_traces)
      gclient_utils.FileWrite(
          packet_traces, GIT_HASH_RE.sub(r'\1', contents))
    shutil.make_archive(traces_zip, 'zip', traces_dir)

    # Collect and compress the git config and gitcookies.
    git_config = RunGit(['config', '-l'])
    gclient_utils.FileWrite(
        os.path.join(git_info_dir, 'git-config'),
        git_config)

    cookie_auth = gerrit_util.Authenticator.get()
    if isinstance(cookie_auth, gerrit_util.CookiesAuthenticator):
      gitcookies_path = cookie_auth.get_gitcookies_path()
      if os.path.isfile(gitcookies_path):
        gitcookies = gclient_utils.FileRead(gitcookies_path)
        gclient_utils.FileWrite(
            os.path.join(git_info_dir, 'gitcookies'),
            GITCOOKIES_REDACT_RE.sub('REDACTED', gitcookies))
    shutil.make_archive(git_info_zip, 'zip', git_info_dir)

    gclient_utils.rmtree(git_info_dir)

  def _RunGitPushWithTraces(self,
                            refspec,
                            refspec_opts,
                            git_push_metadata,
                            git_push_options=None):
    """Run git push and collect the traces resulting from the execution."""
    # Create a temporary directory to store traces in. Traces will be compressed
    # and stored in a 'traces' dir inside depot_tools.
    traces_dir = tempfile.mkdtemp()
    trace_name = os.path.join(
        TRACES_DIR, datetime_now().strftime('%Y%m%dT%H%M%S.%f'))

    env = os.environ.copy()
    env['GIT_REDACT_COOKIES'] = 'o,SSO,GSSO_Uberproxy'
    env['GIT_TR2_EVENT'] = os.path.join(traces_dir, 'tr2-event')
    env['GIT_TRACE2_EVENT'] = os.path.join(traces_dir, 'tr2-event')
    env['GIT_TRACE_CURL'] = os.path.join(traces_dir, 'trace-curl')
    env['GIT_TRACE_CURL_NO_DATA'] = '1'
    env['GIT_TRACE_PACKET'] = os.path.join(traces_dir, 'trace-packet')

    try:
      push_returncode = 0
      remote_url = self.GetRemoteUrl()
      before_push = time_time()
      push_cmd = ['git', 'push', remote_url, refspec]
      if git_push_options:
        for opt in git_push_options:
          push_cmd.extend(['-o', opt])

      push_stdout = gclient_utils.CheckCallAndFilter(
          push_cmd,
          env=env,
          print_stdout=True,
          # Flush after every line: useful for seeing progress when running as
          # recipe.
          filter_fn=lambda _: sys.stdout.flush())
      push_stdout = push_stdout.decode('utf-8', 'replace')
    except subprocess2.CalledProcessError as e:
      push_returncode = e.returncode
      if 'blocked keyword' in str(e.stdout) or 'banned word' in str(e.stdout):
        raise GitPushError(
            'Failed to create a change, very likely due to blocked keyword. '
            'Please examine output above for the reason of the failure.\n'
            'If this is a false positive, you can try to bypass blocked '
            'keyword by using push option '
            '-o banned-words~skip, e.g.:\n'
            'git cl upload -o banned-words~skip\n\n'
            'If git-cl is not working correctly, file a bug under the '
            'Infra>SDK component.')
      if 'git push -o nokeycheck' in str(e.stdout):
        raise GitPushError(
            'Failed to create a change, very likely due to a private key being '
            'detected. Please examine output above for the reason of the '
            'failure.\n'
            'If this is a false positive, you can try to bypass private key '
            'detection by using push option '
            '-o nokeycheck, e.g.:\n'
            'git cl upload -o nokeycheck\n\n'
            'If git-cl is not working correctly, file a bug under the '
            'Infra>SDK component.')

      raise GitPushError(
          'Failed to create a change. Please examine output above for the '
          'reason of the failure.\n'
          'For emergencies, Googlers can escalate to '
          'go/gob-support or go/notify#gob\n'
          'Hint: run command below to diagnose common Git/Gerrit '
          'credential problems:\n'
          '  git cl creds-check\n'
          '\n'
          'If git-cl is not working correctly, file a bug under the Infra>SDK '
          'component including the files below.\n'
          'Review the files before upload, since they might contain sensitive '
          'information.\n'
          'Set the Restrict-View-Google label so that they are not publicly '
          'accessible.\n' + TRACES_MESSAGE % {'trace_name': trace_name})
    finally:
      execution_time = time_time() - before_push
      metrics.collector.add_repeated('sub_commands', {
        'command': 'git push',
        'execution_time': execution_time,
        'exit_code': push_returncode,
        'arguments': metrics_utils.extract_known_subcommand_args(refspec_opts),
      })

      git_push_metadata['execution_time'] = execution_time
      git_push_metadata['exit_code'] = push_returncode
      self._WriteGitPushTraces(trace_name, traces_dir, git_push_metadata)

      self._CleanUpOldTraces()
      gclient_utils.rmtree(traces_dir)

    return push_stdout

  def CMDUploadChange(self, options, git_diff_args, custom_cl_base,
                      change_desc):
    """Upload the current branch to Gerrit, retry if new remote HEAD is
    found. options and change_desc may be mutated."""
    remote, remote_branch = self.GetRemoteBranch()
    branch = GetTargetRef(remote, remote_branch, options.target_branch)

    try:
      return self._CMDUploadChange(options, git_diff_args, custom_cl_base,
                                   change_desc, branch)
    except GitPushError as e:
      # Repository might be in the middle of transition to main branch as
      # default, and uploads to old default might be blocked.
      if remote_branch not in [DEFAULT_OLD_BRANCH, DEFAULT_NEW_BRANCH]:
        DieWithError(str(e), change_desc)

      project_head = gerrit_util.GetProjectHead(self._gerrit_host,
                                                self.GetGerritProject())
      if project_head == branch:
        DieWithError(str(e), change_desc)
      branch = project_head

    print("WARNING: Fetching remote state and retrying upload to default "
          "branch...")
    RunGit(['fetch', '--prune', remote])
    options.edit_description = False
    options.force = True
    try:
      self._CMDUploadChange(options, git_diff_args, custom_cl_base,
                            change_desc, branch)
    except GitPushError as e:
      DieWithError(str(e), change_desc)

  def _CMDUploadChange(self, options, git_diff_args, custom_cl_base,
                       change_desc, branch):
    """Upload the current branch to Gerrit."""
    if options.squash:
      Changelist._GerritCommitMsgHookCheck(offer_removal=not options.force)
      external_parent = None
      if self.GetIssue():
        # User requested to change description
        if options.edit_description:
          change_desc.prompt()
        change_detail = self._GetChangeDetail(['CURRENT_REVISION'])
        change_id = change_detail['change_id']
        change_desc.ensure_change_id(change_id)

        # Check if changes outside of this workspace have been uploaded.
        current_rev = change_detail['current_revision']
        last_uploaded_rev = self._GitGetBranchConfigValue(
            GERRIT_SQUASH_HASH_CONFIG_KEY)
        if last_uploaded_rev and current_rev != last_uploaded_rev:
          external_parent = self._UpdateWithExternalChanges()
      else:  # if not self.GetIssue()
        if not options.force and not options.message_file:
          change_desc.prompt()
        change_ids = git_footers.get_footer_change_id(change_desc.description)
        if len(change_ids) == 1:
          change_id = change_ids[0]
        else:
          change_id = GenerateGerritChangeId(change_desc.description)
          change_desc.ensure_change_id(change_id)

      if options.preserve_tryjobs:
        change_desc.set_preserve_tryjobs()

      remote, upstream_branch = self.FetchUpstreamTuple(self.GetBranch())
      parent = external_parent or self._ComputeParent(
          remote, upstream_branch, custom_cl_base, options.force, change_desc)
      tree = RunGit(['rev-parse', 'HEAD:']).strip()
      with gclient_utils.temporary_file() as desc_tempfile:
        gclient_utils.FileWrite(desc_tempfile, change_desc.description)
        ref_to_push = RunGit(
            ['commit-tree', tree, '-p', parent, '-F', desc_tempfile]).strip()
    else:  # if not options.squash
      if options.no_add_changeid:
        pass
      else:  # adding Change-Ids is okay.
        if not git_footers.get_footer_change_id(change_desc.description):
          DownloadGerritHook(False)
          change_desc.set_description(
              self._AddChangeIdToCommitMessage(change_desc.description,
                                               git_diff_args))
      ref_to_push = 'HEAD'
      # For no-squash mode, we assume the remote called "origin" is the one we
      # want. It is not worthwhile to support different workflows for
      # no-squash mode.
      parent = 'origin/%s' % branch
      # attempt to extract the changeid from the current description
      # fail informatively if not possible.
      change_id_candidates = git_footers.get_footer_change_id(
          change_desc.description)
      if not change_id_candidates:
        DieWithError("Unable to extract change-id from message.")
      change_id = change_id_candidates[0]

    SaveDescriptionBackup(change_desc)
    commits = RunGitSilent(['rev-list', '%s..%s' % (parent,
                                                    ref_to_push)]).splitlines()
    if len(commits) > 1:
      print('WARNING: This will upload %d commits. Run the following command '
            'to see which commits will be uploaded: ' % len(commits))
      print('git log %s..%s' % (parent, ref_to_push))
      print('You can also use `git squash-branch` to squash these into a '
            'single commit.')
      confirm_or_exit(action='upload')

    reviewers = sorted(change_desc.get_reviewers())
    cc = []
    # Add default, watchlist, presubmit ccs if this is the initial upload
    # and CL is not private and auto-ccing has not been disabled.
    if not options.private and not options.no_autocc and not self.GetIssue():
      cc = self.GetCCList().split(',')
    if len(cc) > 100:
      lsc = ('https://chromium.googlesource.com/chromium/src/+/HEAD/docs/'
             'process/lsc/lsc_workflow.md')
      print('WARNING: This will auto-CC %s users.' % len(cc))
      print('LSC may be more appropriate: %s' % lsc)
      print('You can also use the --no-autocc flag to disable auto-CC.')
      confirm_or_exit(action='continue')
    # Add cc's from the --cc flag.
    if options.cc:
      cc.extend(options.cc)
    cc = [email.strip() for email in cc if email.strip()]
    if change_desc.get_cced():
      cc.extend(change_desc.get_cced())
    if self.GetGerritHost() == 'chromium-review.googlesource.com':
      valid_accounts = set(reviewers + cc)
      # TODO(crbug/877717): relax this for all hosts.
    else:
      valid_accounts = gerrit_util.ValidAccounts(
          self.GetGerritHost(), reviewers + cc)
    logging.info('accounts %s are recognized, %s invalid',
                 sorted(valid_accounts),
                 set(reviewers + cc).difference(set(valid_accounts)))

    # Extra options that can be specified at push time. Doc:
    # https://gerrit-review.googlesource.com/Documentation/user-upload.html
    refspec_opts = self._GetRefSpecOptions(options, change_desc)

    for r in sorted(reviewers):
      if r in valid_accounts:
        refspec_opts.append('r=%s' % r)
        reviewers.remove(r)
      else:
        # TODO(tandrii): this should probably be a hard failure.
        print('WARNING: reviewer %s doesn\'t have a Gerrit account, skipping'
              % r)
    for c in sorted(cc):
      # refspec option will be rejected if cc doesn't correspond to an
      # account, even though REST call to add such arbitrary cc may succeed.
      if c in valid_accounts:
        refspec_opts.append('cc=%s' % c)
        cc.remove(c)

    refspec_suffix = ''
    if refspec_opts:
      refspec_suffix = '%' + ','.join(refspec_opts)
      assert ' ' not in refspec_suffix, (
          'spaces not allowed in refspec: "%s"' % refspec_suffix)
    refspec = '%s:refs/for/%s%s' % (ref_to_push, branch, refspec_suffix)

    git_push_metadata = {
        'gerrit_host': self.GetGerritHost(),
        'title': options.title or '<untitled>',
        'change_id': change_id,
        'description': change_desc.description,
    }

    # Gerrit may or may not update fast enough to return the correct patchset
    # number after we push. Get the pre-upload patchset and increment later.
    latest_ps = self.GetMostRecentPatchset(update=False) or 0

    push_stdout = self._RunGitPushWithTraces(refspec, refspec_opts,
                                             git_push_metadata,
                                             options.push_options)

    if options.squash:
      regex = re.compile(r'remote:\s+https?://[\w\-\.\+\/#]*/(\d+)\s.*')
      change_numbers = [m.group(1)
                        for m in map(regex.match, push_stdout.splitlines())
                        if m]
      if len(change_numbers) != 1:
        DieWithError(
          ('Created|Updated %d issues on Gerrit, but only 1 expected.\n'
           'Change-Id: %s') % (len(change_numbers), change_id), change_desc)
      self.SetIssue(change_numbers[0])
      self.SetPatchset(latest_ps + 1)
      self._GitSetBranchConfigValue(GERRIT_SQUASH_HASH_CONFIG_KEY, ref_to_push)

    if self.GetIssue() and (reviewers or cc):
      # GetIssue() is not set in case of non-squash uploads according to tests.
      # TODO(crbug.com/751901): non-squash uploads in git cl should be removed.
      gerrit_util.AddReviewers(self.GetGerritHost(),
                               self._GerritChangeIdentifier(),
                               reviewers,
                               cc,
                               notify=bool(options.send_mail))

    return 0

  def _ComputeParent(self, remote, upstream_branch, custom_cl_base, force,
                     change_desc):
    """Computes parent of the generated commit to be uploaded to Gerrit.

    Returns revision or a ref name.
    """
    if custom_cl_base:
      # Try to avoid creating additional unintended CLs when uploading, unless
      # user wants to take this risk.
      local_ref_of_target_remote = self.GetRemoteBranch()[1]
      code, _ = RunGitWithCode(['merge-base', '--is-ancestor', custom_cl_base,
                                local_ref_of_target_remote])
      if code == 1:
        print('\nWARNING: Manually specified base of this CL `%s` '
              'doesn\'t seem to belong to target remote branch `%s`.\n\n'
              'If you proceed with upload, more than 1 CL may be created by '
              'Gerrit as a result, in turn confusing or crashing git cl.\n\n'
              'If you are certain that specified base `%s` has already been '
              'uploaded to Gerrit as another CL, you may proceed.\n' %
              (custom_cl_base, local_ref_of_target_remote, custom_cl_base))
        if not force:
          confirm_or_exit(
              'Do you take responsibility for cleaning up potential mess '
              'resulting from proceeding with upload?',
              action='upload')
      return custom_cl_base

    if remote != '.':
      return self.GetCommonAncestorWithUpstream()

    # If our upstream branch is local, we base our squashed commit on its
    # squashed version.
    upstream_branch_name = scm.GIT.ShortBranchName(upstream_branch)

    if upstream_branch_name == 'master':
      return self.GetCommonAncestorWithUpstream()
    if upstream_branch_name == 'main':
      return self.GetCommonAncestorWithUpstream()

    # Check the squashed hash of the parent.
    # TODO(tandrii): consider checking parent change in Gerrit and using its
    # hash if tree hash of latest parent revision (patchset) in Gerrit matches
    # the tree hash of the parent branch. The upside is less likely bogus
    # requests to reupload parent change just because it's uploadhash is
    # missing, yet the downside likely exists, too (albeit unknown to me yet).
    parent = scm.GIT.GetBranchConfig(settings.GetRoot(), upstream_branch_name,
                                     GERRIT_SQUASH_HASH_CONFIG_KEY)
    # Verify that the upstream branch has been uploaded too, otherwise
    # Gerrit will create additional CLs when uploading.
    if not parent or (RunGitSilent(['rev-parse', upstream_branch + ':']) !=
                      RunGitSilent(['rev-parse', parent + ':'])):
      DieWithError(
          '\nUpload upstream branch %s first.\n'
          'It is likely that this branch has been rebased since its last '
          'upload, so you just need to upload it again.\n'
          '(If you uploaded it with --no-squash, then branch dependencies '
          'are not supported, and you should reupload with --squash.)'
          % upstream_branch_name,
          change_desc)
    return parent

  def _UpdateWithExternalChanges(self):
    """Updates workspace with external changes.

    Returns the commit hash that should be used as the merge base on upload.
    """
    local_ps = self.GetPatchset()
    if local_ps is None:
      return

    external_ps = self.GetMostRecentPatchset(update=False)
    if external_ps is None or local_ps == external_ps or \
        not self._IsPatchsetRangeSignificant(local_ps + 1, external_ps):
      return

    num_changes = external_ps - local_ps
    if num_changes > 1:
      change_words = 'changes were'
    else:
      change_words = 'change was'
    print('\n%d external %s published to %s:\n' %
          (num_changes, change_words, self.GetIssueURL(short=True)))

    # Print an overview of external changes.
    ps_to_commit = {}
    ps_to_info = {}
    revisions = self._GetChangeDetail(['ALL_REVISIONS'])
    for commit_id, revision_info in revisions.get('revisions', {}).items():
      ps_num = revision_info['_number']
      ps_to_commit[ps_num] = commit_id
      ps_to_info[ps_num] = revision_info

    for ps in range(external_ps, local_ps, -1):
      commit = ps_to_commit[ps][:8]
      desc = ps_to_info[ps].get('description', '')
      print('Patchset %d [%s] %s' % (ps, commit, desc))

    if not ask_for_explicit_yes('\nUploading as-is will override them. '
                                'Get the latest changes and apply?'):
      return

    # Get latest Gerrit merge base. Use the first parent even if multiple exist.
    external_parent = self._GetChangeCommit(revision=external_ps)['parents'][0]
    external_base = external_parent['commit']

    branch = git_common.current_branch()
    local_base = self.GetCommonAncestorWithUpstream()
    if local_base != external_base:
      print('\nLocal merge base %s is different from Gerrit %s.\n' %
            (local_base, external_base))
      if git_common.upstream(branch):
        DieWithError('Upstream branch set. Consider using `git rebase-update` '
                     'to make these the same.')
      print('No upstream branch set. Consider setting it and using '
            '`git rebase-update`.\nContinuing upload with Gerrit merge base.')

    # Fetch Gerrit's CL base if it doesn't exist locally.
    remote, _ = self.GetRemoteBranch()
    if not scm.GIT.IsValidRevision(settings.GetRoot(), external_base):
      RunGitSilent(['fetch', remote, external_base])

    # Get the diff between local_ps and external_ps.
    issue = self.GetIssue()
    changes_ref = 'refs/changes/%02d/%d/' % (issue % 100, issue)
    RunGitSilent(['fetch', remote, changes_ref + str(local_ps)])
    last_uploaded = RunGitSilent(['rev-parse', 'FETCH_HEAD']).strip()
    RunGitSilent(['fetch', remote, changes_ref + str(external_ps)])
    latest_external = RunGitSilent(['rev-parse', 'FETCH_HEAD']).strip()
    diff = RunGitSilent(['diff', '%s..%s' % (last_uploaded, latest_external)])

    # Diff can be empty in the case of trivial rebases.
    if not diff:
      return external_base

    # Apply the diff.
    with gclient_utils.temporary_file() as diff_tempfile:
      gclient_utils.FileWrite(diff_tempfile, diff)
      clean_patch = RunGitWithCode(['apply', '--check', diff_tempfile])[0] == 0
      RunGitSilent(['apply', '-3', '--intent-to-add', diff_tempfile])
      if not clean_patch:
        # Normally patchset is set after upload. But because we exit, that never
        # happens. Updating here makes sure that subsequent uploads don't need
        # to fetch/apply the same diff again.
        self.SetPatchset(external_ps)
        DieWithError('\nPatch did not apply cleanly. Please resolve any '
                     'conflicts and reupload.')

      message = 'Incorporate external changes from '
      if num_changes == 1:
        message += 'patchset %d' % external_ps
      else:
        message += 'patchsets %d to %d' % (local_ps + 1, external_ps)
      RunGitSilent(['commit', '-am', message])
      # TODO(crbug.com/1382528): Use the previous commit's message as a default
      # patchset title instead of this 'Incorporate' message.
    return external_base

  def _AddChangeIdToCommitMessage(self, log_desc, args):
    """Re-commits using the current message, assumes the commit hook is in
    place.
    """
    RunGit(['commit', '--amend', '-m', log_desc])
    new_log_desc = _create_description_from_log(args)
    if git_footers.get_footer_change_id(new_log_desc):
      print('git-cl: Added Change-Id to commit message.')
      return new_log_desc

    DieWithError('ERROR: Gerrit commit-msg hook not installed.')

  def CannotTriggerTryJobReason(self):
    try:
      data = self._GetChangeDetail()
    except GerritChangeNotExists:
      return 'Gerrit doesn\'t know about your change %s' % self.GetIssue()

    if data['status'] in ('ABANDONED', 'MERGED'):
      return 'CL %s is closed' % self.GetIssue()

  def GetGerritChange(self, patchset=None):
    """Returns a buildbucket.v2.GerritChange message for the current issue."""
    host = urllib.parse.urlparse(self.GetCodereviewServer()).hostname
    issue = self.GetIssue()
    patchset = int(patchset or self.GetPatchset())
    data = self._GetChangeDetail(['ALL_REVISIONS'])

    assert host and issue and patchset, 'CL must be uploaded first'

    has_patchset = any(
        int(revision_data['_number']) == patchset
        for revision_data in data['revisions'].values())
    if not has_patchset:
      raise Exception('Patchset %d is not known in Gerrit change %d' %
                      (patchset, self.GetIssue()))

    return {
        'host': host,
        'change': issue,
        'project': data['project'],
        'patchset': patchset,
    }

  def GetIssueOwner(self):
    return self._GetChangeDetail(['DETAILED_ACCOUNTS'])['owner']['email']

  def GetReviewers(self):
    details = self._GetChangeDetail(['DETAILED_ACCOUNTS'])
    return [r['email'] for r in details['reviewers'].get('REVIEWER', [])]


def _get_bug_line_values(default_project_prefix, bugs):
  """Given default_project_prefix and comma separated list of bugs, yields bug
  line values.

  Each bug can be either:
    * a number, which is combined with default_project_prefix
    * string, which is left as is.

  This function may produce more than one line, because bugdroid expects one
  project per line.

  >>> list(_get_bug_line_values('v8:', '123,chromium:789'))
      ['v8:123', 'chromium:789']
  """
  default_bugs = []
  others = []
  for bug in bugs.split(','):
    bug = bug.strip()
    if bug:
      try:
        default_bugs.append(int(bug))
      except ValueError:
        others.append(bug)

  if default_bugs:
    default_bugs = ','.join(map(str, default_bugs))
    if default_project_prefix:
      if not default_project_prefix.endswith(':'):
        default_project_prefix += ':'
      yield '%s%s' % (default_project_prefix, default_bugs)
    else:
      yield default_bugs
  for other in sorted(others):
    # Don't bother finding common prefixes, CLs with >2 bugs are very very rare.
    yield other


class ChangeDescription(object):
  """Contains a parsed form of the change description."""
  R_LINE = r'^[ \t]*(TBR|R)[ \t]*=[ \t]*(.*?)[ \t]*$'
  CC_LINE = r'^[ \t]*(CC)[ \t]*=[ \t]*(.*?)[ \t]*$'
  BUG_LINE = r'^[ \t]*(?:(BUG)[ \t]*=|Bug:)[ \t]*(.*?)[ \t]*$'
  FIXED_LINE = r'^[ \t]*Fixed[ \t]*:[ \t]*(.*?)[ \t]*$'
  CHERRY_PICK_LINE = r'^\(cherry picked from commit [a-fA-F0-9]{40}\)$'
  STRIP_HASH_TAG_PREFIX = r'^(\s*(revert|reland)( "|:)?\s*)*'
  BRACKET_HASH_TAG = r'\s*\[([^\[\]]+)\]'
  COLON_SEPARATED_HASH_TAG = r'^([a-zA-Z0-9_\- ]+):($|[^:])'
  BAD_HASH_TAG_CHUNK = r'[^a-zA-Z0-9]+'

  def __init__(self, description, bug=None, fixed=None):
    self._description_lines = (description or '').strip().splitlines()
    if bug:
      regexp = re.compile(self.BUG_LINE)
      prefix = settings.GetBugPrefix()
      if not any((regexp.match(line) for line in self._description_lines)):
        values = list(_get_bug_line_values(prefix, bug))
        self.append_footer('Bug: %s' % ', '.join(values))
    if fixed:
      regexp = re.compile(self.FIXED_LINE)
      prefix = settings.GetBugPrefix()
      if not any((regexp.match(line) for line in self._description_lines)):
        values = list(_get_bug_line_values(prefix, fixed))
        self.append_footer('Fixed: %s' % ', '.join(values))

  @property               # www.logilab.org/ticket/89786
  def description(self):  # pylint: disable=method-hidden
    return '\n'.join(self._description_lines)

  def set_description(self, desc):
    if isinstance(desc, basestring):
      lines = desc.splitlines()
    else:
      lines = [line.rstrip() for line in desc]
    while lines and not lines[0]:
      lines.pop(0)
    while lines and not lines[-1]:
      lines.pop(-1)
    self._description_lines = lines

  def ensure_change_id(self, change_id):
    description = self.description
    footer_change_ids = git_footers.get_footer_change_id(description)
    # Make sure that the Change-Id in the description matches the given one.
    if footer_change_ids != [change_id]:
      if footer_change_ids:
        # Remove any existing Change-Id footers since they don't match the
        # expected change_id footer.
        description = git_footers.remove_footer(description, 'Change-Id')
        print('WARNING: Change-Id has been set to %s. Use `git cl issue 0` '
              'if you want to set a new one.')
      # Add the expected Change-Id footer.
      description = git_footers.add_footer_change_id(description, change_id)
      self.set_description(description)

  def update_reviewers(self, reviewers):
    """Rewrites the R= line(s) as a single line each.

    Args:
      reviewers (list(str)) - list of additional emails to use for reviewers.
    """
    if not reviewers:
      return

    reviewers = set(reviewers)

    # Get the set of R= lines and remove them from the description.
    regexp = re.compile(self.R_LINE)
    matches = [regexp.match(line) for line in self._description_lines]
    new_desc = [l for i, l in enumerate(self._description_lines)
                if not matches[i]]
    self.set_description(new_desc)

    # Construct new unified R= lines.

    # First, update reviewers with names from the R= lines (if any).
    for match in matches:
      if not match:
        continue
      reviewers.update(cleanup_list([match.group(2).strip()]))

    new_r_line = 'R=' + ', '.join(sorted(reviewers))

    # Put the new lines in the description where the old first R= line was.
    line_loc = next((i for i, match in enumerate(matches) if match), -1)
    if 0 <= line_loc < len(self._description_lines):
      self._description_lines.insert(line_loc, new_r_line)
    else:
      self.append_footer(new_r_line)

  def set_preserve_tryjobs(self):
    """Ensures description footer contains 'Cq-Do-Not-Cancel-Tryjobs: true'."""
    footers = git_footers.parse_footers(self.description)
    for v in footers.get('Cq-Do-Not-Cancel-Tryjobs', []):
      if v.lower() == 'true':
        return
    self.append_footer('Cq-Do-Not-Cancel-Tryjobs: true')

  def prompt(self):
    """Asks the user to update the description."""
    self.set_description([
      '# Enter a description of the change.',
      '# This will be displayed on the codereview site.',
      '# The first line will also be used as the subject of the review.',
      '#--------------------This line is 72 characters long'
      '--------------------',
    ] + self._description_lines)
    bug_regexp = re.compile(self.BUG_LINE)
    fixed_regexp = re.compile(self.FIXED_LINE)
    prefix = settings.GetBugPrefix()
    has_issue = lambda l: bug_regexp.match(l) or fixed_regexp.match(l)

    if not any((has_issue(line) for line in self._description_lines)):
      self.append_footer('Bug: %s' % prefix)

    print('Waiting for editor...')
    content = gclient_utils.RunEditor(self.description, True,
                                      git_editor=settings.GetGitEditor())
    if not content:
      DieWithError('Running editor failed')
    lines = content.splitlines()

    # Strip off comments and default inserted "Bug:" line.
    clean_lines = [line.rstrip() for line in lines if not
                   (line.startswith('#') or
                    line.rstrip() == "Bug:" or
                    line.rstrip() == "Bug: " + prefix)]
    if not clean_lines:
      DieWithError('No CL description, aborting')
    self.set_description(clean_lines)

  def append_footer(self, line):
    """Adds a footer line to the description.

    Differentiates legacy "KEY=xxx" footers (used to be called tags) and
    Gerrit's footers in the form of "Footer-Key: footer any value" and ensures
    that Gerrit footers are always at the end.
    """
    parsed_footer_line = git_footers.parse_footer(line)
    if parsed_footer_line:
      # Line is a gerrit footer in the form: Footer-Key: any value.
      # Thus, must be appended observing Gerrit footer rules.
      self.set_description(
          git_footers.add_footer(self.description,
                                 key=parsed_footer_line[0],
                                 value=parsed_footer_line[1]))
      return

    if not self._description_lines:
      self._description_lines.append(line)
      return

    top_lines, gerrit_footers, _ = git_footers.split_footers(self.description)
    if gerrit_footers:
      # git_footers.split_footers ensures that there is an empty line before
      # actual (gerrit) footers, if any. We have to keep it that way.
      assert top_lines and top_lines[-1] == ''
      top_lines, separator = top_lines[:-1], top_lines[-1:]
    else:
      separator = []  # No need for separator if there are no gerrit_footers.

    prev_line = top_lines[-1] if top_lines else ''
    if (not presubmit_support.Change.TAG_LINE_RE.match(prev_line) or
        not presubmit_support.Change.TAG_LINE_RE.match(line)):
      top_lines.append('')
    top_lines.append(line)
    self._description_lines = top_lines + separator + gerrit_footers

  def get_reviewers(self, tbr_only=False):
    """Retrieves the list of reviewers."""
    matches = [re.match(self.R_LINE, line) for line in self._description_lines]
    reviewers = [match.group(2).strip()
                 for match in matches
                 if match and (not tbr_only or match.group(1).upper() == 'TBR')]
    return cleanup_list(reviewers)

  def get_cced(self):
    """Retrieves the list of reviewers."""
    matches = [re.match(self.CC_LINE, line) for line in self._description_lines]
    cced = [match.group(2).strip() for match in matches if match]
    return cleanup_list(cced)

  def get_hash_tags(self):
    """Extracts and sanitizes a list of Gerrit hashtags."""
    subject = (self._description_lines or ('',))[0]
    subject = re.sub(
        self.STRIP_HASH_TAG_PREFIX, '', subject, flags=re.IGNORECASE)

    tags = []
    start = 0
    bracket_exp = re.compile(self.BRACKET_HASH_TAG)
    while True:
      m = bracket_exp.match(subject, start)
      if not m:
        break
      tags.append(self.sanitize_hash_tag(m.group(1)))
      start = m.end()

    if not tags:
      # Try "Tag: " prefix.
      m = re.match(self.COLON_SEPARATED_HASH_TAG, subject)
      if m:
        tags.append(self.sanitize_hash_tag(m.group(1)))
    return tags

  @classmethod
  def sanitize_hash_tag(cls, tag):
    """Returns a sanitized Gerrit hash tag.

    A sanitized hashtag can be used as a git push refspec parameter value.
    """
    return re.sub(cls.BAD_HASH_TAG_CHUNK, '-', tag).strip('-').lower()


def FindCodereviewSettingsFile(filename='codereview.settings'):
  """Finds the given file starting in the cwd and going up.

  Only looks up to the top of the repository unless an
  'inherit-review-settings-ok' file exists in the root of the repository.
  """
  inherit_ok_file = 'inherit-review-settings-ok'
  cwd = os.getcwd()
  root = settings.GetRoot()
  if os.path.isfile(os.path.join(root, inherit_ok_file)):
    root = None
  while True:
    if os.path.isfile(os.path.join(cwd, filename)):
      return open(os.path.join(cwd, filename))
    if cwd == root:
      break
    parent_dir = os.path.dirname(cwd)
    if parent_dir == cwd:
      # We hit the system root directory.
      break
    cwd = parent_dir
  return None


def LoadCodereviewSettingsFromFile(fileobj):
  """Parses a codereview.settings file and updates hooks."""
  keyvals = gclient_utils.ParseCodereviewSettingsContent(fileobj.read())

  def SetProperty(name, setting, unset_error_ok=False):
    fullname = 'rietveld.' + name
    if setting in keyvals:
      RunGit(['config', fullname, keyvals[setting]])
    else:
      RunGit(['config', '--unset-all', fullname], error_ok=unset_error_ok)

  if not keyvals.get('GERRIT_HOST', False):
    SetProperty('server', 'CODE_REVIEW_SERVER')
  # Only server setting is required. Other settings can be absent.
  # In that case, we ignore errors raised during option deletion attempt.
  SetProperty('cc', 'CC_LIST', unset_error_ok=True)
  SetProperty('tree-status-url', 'STATUS', unset_error_ok=True)
  SetProperty('viewvc-url', 'VIEW_VC', unset_error_ok=True)
  SetProperty('bug-prefix', 'BUG_PREFIX', unset_error_ok=True)
  SetProperty('cpplint-regex', 'LINT_REGEX', unset_error_ok=True)
  SetProperty('cpplint-ignore-regex', 'LINT_IGNORE_REGEX', unset_error_ok=True)
  SetProperty('run-post-upload-hook', 'RUN_POST_UPLOAD_HOOK',
              unset_error_ok=True)
  SetProperty(
      'format-full-by-default', 'FORMAT_FULL_BY_DEFAULT', unset_error_ok=True)
  SetProperty('use-python3', 'USE_PYTHON3', unset_error_ok=True)

  if 'GERRIT_HOST' in keyvals:
    RunGit(['config', 'gerrit.host', keyvals['GERRIT_HOST']])

  if 'GERRIT_SQUASH_UPLOADS' in keyvals:
    RunGit(['config', 'gerrit.squash-uploads',
            keyvals['GERRIT_SQUASH_UPLOADS']])

  if 'GERRIT_SKIP_ENSURE_AUTHENTICATED' in keyvals:
    RunGit(['config', 'gerrit.skip-ensure-authenticated',
            keyvals['GERRIT_SKIP_ENSURE_AUTHENTICATED']])

  if 'PUSH_URL_CONFIG' in keyvals and 'ORIGIN_URL_CONFIG' in keyvals:
    # should be of the form
    # PUSH_URL_CONFIG: url.ssh://gitrw.chromium.org.pushinsteadof
    # ORIGIN_URL_CONFIG: http://src.chromium.org/git
    RunGit(['config', keyvals['PUSH_URL_CONFIG'],
            keyvals['ORIGIN_URL_CONFIG']])


def urlretrieve(source, destination):
  """Downloads a network object to a local file, like urllib.urlretrieve.

  This is necessary because urllib is broken for SSL connections via a proxy.
  """
  with open(destination, 'wb') as f:
    f.write(urllib.request.urlopen(source).read())


def hasSheBang(fname):
  """Checks fname is a #! script."""
  with open(fname) as f:
    return f.read(2).startswith('#!')


def DownloadGerritHook(force):
  """Downloads and installs a Gerrit commit-msg hook.

  Args:
    force: True to update hooks. False to install hooks if not present.
  """
  src = 'https://gerrit-review.googlesource.com/tools/hooks/commit-msg'
  dst = os.path.join(settings.GetRoot(), '.git', 'hooks', 'commit-msg')
  if not os.access(dst, os.X_OK):
    if os.path.exists(dst):
      if not force:
        return
    try:
      urlretrieve(src, dst)
      if not hasSheBang(dst):
        DieWithError('Not a script: %s\n'
                     'You need to download from\n%s\n'
                     'into .git/hooks/commit-msg and '
                     'chmod +x .git/hooks/commit-msg' % (dst, src))
      os.chmod(dst, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
    except Exception:
      if os.path.exists(dst):
        os.remove(dst)
      DieWithError('\nFailed to download hooks.\n'
                   'You need to download from\n%s\n'
                   'into .git/hooks/commit-msg and '
                   'chmod +x .git/hooks/commit-msg' % src)


class _GitCookiesChecker(object):
  """Provides facilities for validating and suggesting fixes to .gitcookies."""

  def __init__(self):
    # Cached list of [host, identity, source], where source is either
    # .gitcookies or .netrc.
    self._all_hosts = None

  def ensure_configured_gitcookies(self):
    """Runs checks and suggests fixes to make git use .gitcookies from default
    path."""
    default = gerrit_util.CookiesAuthenticator.get_gitcookies_path()
    configured_path = RunGitSilent(
        ['config', '--global', 'http.cookiefile']).strip()
    configured_path = os.path.expanduser(configured_path)
    if configured_path:
      self._ensure_default_gitcookies_path(configured_path, default)
    else:
      self._configure_gitcookies_path(default)

  @staticmethod
  def _ensure_default_gitcookies_path(configured_path, default_path):
    assert configured_path
    if configured_path == default_path:
      print('git is already configured to use your .gitcookies from %s' %
            configured_path)
      return

    print('WARNING: You have configured custom path to .gitcookies: %s\n'
          'Gerrit and other depot_tools expect .gitcookies at %s\n' %
          (configured_path, default_path))

    if not os.path.exists(configured_path):
      print('However, your configured .gitcookies file is missing.')
      confirm_or_exit('Reconfigure git to use default .gitcookies?',
                      action='reconfigure')
      RunGit(['config', '--global', 'http.cookiefile', default_path])
      return

    if os.path.exists(default_path):
      print('WARNING: default .gitcookies file already exists %s' %
            default_path)
      DieWithError('Please delete %s manually and re-run git cl creds-check' %
                   default_path)

    confirm_or_exit('Move existing .gitcookies to default location?',
                    action='move')
    shutil.move(configured_path, default_path)
    RunGit(['config', '--global', 'http.cookiefile', default_path])
    print('Moved and reconfigured git to use .gitcookies from %s' %
          default_path)

  @staticmethod
  def _configure_gitcookies_path(default_path):
    netrc_path = gerrit_util.CookiesAuthenticator.get_netrc_path()
    if os.path.exists(netrc_path):
      print('You seem to be using outdated .netrc for git credentials: %s' %
            netrc_path)
    print('This tool will guide you through setting up recommended '
          '.gitcookies store for git credentials.\n'
          '\n'
          'IMPORTANT: If something goes wrong and you decide to go back, do:\n'
          '  git config --global --unset http.cookiefile\n'
          '  mv %s %s.backup\n\n' % (default_path, default_path))
    confirm_or_exit(action='setup .gitcookies')
    RunGit(['config', '--global', 'http.cookiefile', default_path])
    print('Configured git to use .gitcookies from %s' % default_path)

  def get_hosts_with_creds(self, include_netrc=False):
    if self._all_hosts is None:
      a = gerrit_util.CookiesAuthenticator()
      self._all_hosts = [(h, u, s) for h, u, s in itertools.chain((
          (h, u, '.netrc') for h, (u, _, _) in a.netrc.hosts.items()), (
              (h, u, '.gitcookies') for h, (u, _) in a.gitcookies.items()))
                         if h.endswith(_GOOGLESOURCE)]

    if include_netrc:
      return self._all_hosts
    return [(h, u, s) for h, u, s in self._all_hosts if s != '.netrc']

  def print_current_creds(self, include_netrc=False):
    hosts = sorted(self.get_hosts_with_creds(include_netrc=include_netrc))
    if not hosts:
      print('No Git/Gerrit credentials found')
      return
    lengths = [max(map(len, (row[i] for row in hosts))) for i in range(3)]
    header = [('Host', 'User', 'Which file'),
              ['=' * l for l in lengths]]
    for row in (header + hosts):
      print('\t'.join((('%%+%ds' % l) % s)
                       for l, s in zip(lengths, row)))

  @staticmethod
  def _parse_identity(identity):
    """Parses identity "git-<username>.domain" into <username> and domain."""
    # Special case: usernames that contain ".", which are generally not
    # distinguishable from sub-domains. But we do know typical domains:
    if identity.endswith('.chromium.org'):
      domain = 'chromium.org'
      username = identity[:-len('.chromium.org')]
    else:
      username, domain = identity.split('.', 1)
    if username.startswith('git-'):
      username = username[len('git-'):]
    return username, domain

  def has_generic_host(self):
    """Returns whether generic .googlesource.com has been configured.

    Chrome Infra recommends to use explicit ${host}.googlesource.com instead.
    """
    for host, _, _ in self.get_hosts_with_creds(include_netrc=False):
      if host == '.' + _GOOGLESOURCE:
        return True
    return False

  def _get_git_gerrit_identity_pairs(self):
    """Returns map from canonic host to pair of identities (Git, Gerrit).

    One of identities might be None, meaning not configured.
    """
    host_to_identity_pairs = {}
    for host, identity, _ in self.get_hosts_with_creds():
      canonical = _canonical_git_googlesource_host(host)
      pair = host_to_identity_pairs.setdefault(canonical, [None, None])
      idx = 0 if canonical == host else 1
      pair[idx] = identity
    return host_to_identity_pairs

  def get_partially_configured_hosts(self):
    return set(
        (host if i1 else _canonical_gerrit_googlesource_host(host))
        for host, (i1, i2) in self._get_git_gerrit_identity_pairs().items()
        if None in (i1, i2) and host != '.' + _GOOGLESOURCE)

  def get_conflicting_hosts(self):
    return set(
        host
        for host, (i1, i2) in self._get_git_gerrit_identity_pairs().items()
        if None not in (i1, i2) and i1 != i2)

  def get_duplicated_hosts(self):
    counters = collections.Counter(h for h, _, _ in self.get_hosts_with_creds())
    return set(host for host, count in counters.items() if count > 1)


  @staticmethod
  def _format_hosts(hosts, extra_column_func=None):
    hosts = sorted(hosts)
    assert hosts
    if extra_column_func is None:
      extras = [''] * len(hosts)
    else:
      extras = [extra_column_func(host) for host in hosts]
    tmpl = '%%-%ds    %%-%ds' % (max(map(len, hosts)), max(map(len, extras)))
    lines = []
    for he in zip(hosts, extras):
      lines.append(tmpl % he)
    return lines

  def _find_problems(self):
    if self.has_generic_host():
      yield ('.googlesource.com wildcard record detected',
             ['Chrome Infrastructure team recommends to list full host names '
              'explicitly.'],
             None)

    dups = self.get_duplicated_hosts()
    if dups:
      yield ('The following hosts were defined twice',
             self._format_hosts(dups),
             None)

    partial = self.get_partially_configured_hosts()
    if partial:
      yield ('Credentials should come in pairs for Git and Gerrit hosts. '
             'These hosts are missing',
             self._format_hosts(
                 partial, lambda host: 'but %s defined' % _get_counterpart_host(
                     host)), partial)

    conflicting = self.get_conflicting_hosts()
    if conflicting:
      yield ('The following Git hosts have differing credentials from their '
             'Gerrit counterparts',
             self._format_hosts(conflicting, lambda host: '%s vs %s' %
                 tuple(self._get_git_gerrit_identity_pairs()[host])),
             conflicting)

  def find_and_report_problems(self):
    """Returns True if there was at least one problem, else False."""
    found = False
    bad_hosts = set()
    for title, sublines, hosts in self._find_problems():
      if not found:
        found = True
        print('\n\n.gitcookies problem report:\n')
      bad_hosts.update(hosts or [])
      print('  %s%s' % (title, (':' if sublines else '')))
      if sublines:
        print()
        print('    %s' % '\n    '.join(sublines))
      print()

    if bad_hosts:
      assert found
      print('  You can manually remove corresponding lines in your %s file and '
            'visit the following URLs with correct account to generate '
            'correct credential lines:\n' %
            gerrit_util.CookiesAuthenticator.get_gitcookies_path())
      print('    %s' % '\n    '.join(
          sorted(
              set(gerrit_util.CookiesAuthenticator().get_new_password_url(
                  _canonical_git_googlesource_host(host))
                  for host in bad_hosts))))
    return found


@metrics.collector.collect_metrics('git cl creds-check')
def CMDcreds_check(parser, args):
  """Checks credentials and suggests changes."""
  _, _ = parser.parse_args(args)

  # Code below checks .gitcookies. Abort if using something else.
  authn = gerrit_util.Authenticator.get()
  if not isinstance(authn, gerrit_util.CookiesAuthenticator):
    message = (
        'This command is not designed for bot environment. It checks '
        '~/.gitcookies file not generally used on bots.')
    # TODO(crbug.com/1059384): Automatically detect when running on cloudtop.
    if isinstance(authn, gerrit_util.GceAuthenticator):
      message += (
          '\n'
          'If you need to run this on GCE or a cloudtop instance, '
          'export SKIP_GCE_AUTH_FOR_GIT=1 in your env.')
    DieWithError(message)

  checker = _GitCookiesChecker()
  checker.ensure_configured_gitcookies()

  print('Your .netrc and .gitcookies have credentials for these hosts:')
  checker.print_current_creds(include_netrc=True)

  if not checker.find_and_report_problems():
    print('\nNo problems detected in your .gitcookies file.')
    return 0
  return 1


@metrics.collector.collect_metrics('git cl baseurl')
def CMDbaseurl(parser, args):
  """Gets or sets base-url for this branch."""
  _, args = parser.parse_args(args)
  branchref = scm.GIT.GetBranchRef(settings.GetRoot())
  branch = scm.GIT.ShortBranchName(branchref)
  if not args:
    print('Current base-url:')
    return RunGit(['config', 'branch.%s.base-url' % branch],
                  error_ok=False).strip()

  print('Setting base-url to %s' % args[0])
  return RunGit(['config', 'branch.%s.base-url' % branch, args[0]],
                error_ok=False).strip()


def color_for_status(status):
  """Maps a Changelist status to color, for CMDstatus and other tools."""
  BOLD = '\033[1m'
  return {
    'unsent': BOLD + Fore.YELLOW,
    'waiting': BOLD + Fore.RED,
    'reply': BOLD + Fore.YELLOW,
    'not lgtm': BOLD + Fore.RED,
    'lgtm': BOLD + Fore.GREEN,
    'commit': BOLD + Fore.MAGENTA,
    'closed': BOLD + Fore.CYAN,
    'error': BOLD + Fore.WHITE,
  }.get(status, Fore.WHITE)


def get_cl_statuses(changes, fine_grained, max_processes=None):
  """Returns a blocking iterable of (cl, status) for given branches.

  If fine_grained is true, this will fetch CL statuses from the server.
  Otherwise, simply indicate if there's a matching url for the given branches.

  If max_processes is specified, it is used as the maximum number of processes
  to spawn to fetch CL status from the server. Otherwise 1 process per branch is
  spawned.

  See GetStatus() for a list of possible statuses.
  """
  if not changes:
    return

  if not fine_grained:
    # Fast path which doesn't involve querying codereview servers.
    # Do not use get_approving_reviewers(), since it requires an HTTP request.
    for cl in changes:
      yield (cl, 'waiting' if cl.GetIssueURL() else 'error')
    return

  # First, sort out authentication issues.
  logging.debug('ensuring credentials exist')
  for cl in changes:
    cl.EnsureAuthenticated(force=False, refresh=True)

  def fetch(cl):
    try:
      return (cl, cl.GetStatus())
    except:
      # See http://crbug.com/629863.
      logging.exception('failed to fetch status for cl %s:', cl.GetIssue())
      raise

  threads_count = len(changes)
  if max_processes:
    threads_count = max(1, min(threads_count, max_processes))
  logging.debug('querying %d CLs using %d threads', len(changes), threads_count)

  pool = multiprocessing.pool.ThreadPool(threads_count)
  fetched_cls = set()
  try:
    it = pool.imap_unordered(fetch, changes).__iter__()
    while True:
      try:
        cl, status = it.next(timeout=5)
      except (multiprocessing.TimeoutError, StopIteration):
        break
      fetched_cls.add(cl)
      yield cl, status
  finally:
    pool.close()

  # Add any branches that failed to fetch.
  for cl in set(changes) - fetched_cls:
    yield (cl, 'error')


def upload_branch_deps(cl, args, force=False):
  """Uploads CLs of local branches that are dependents of the current branch.

  If the local branch dependency tree looks like:

    test1 -> test2.1 -> test3.1
                     -> test3.2
          -> test2.2 -> test3.3

  and you run "git cl upload --dependencies" from test1 then "git cl upload" is
  run on the dependent branches in this order:
  test2.1, test3.1, test3.2, test2.2, test3.3

  Note: This function does not rebase your local dependent branches. Use it
        when you make a change to the parent branch that will not conflict
        with its dependent branches, and you would like their dependencies
        updated in Rietveld.
  """
  if git_common.is_dirty_git_tree('upload-branch-deps'):
    return 1

  root_branch = cl.GetBranch()
  if root_branch is None:
    DieWithError('Can\'t find dependent branches from detached HEAD state. '
                 'Get on a branch!')
  if not cl.GetIssue():
    DieWithError('Current branch does not have an uploaded CL. We cannot set '
                 'patchset dependencies without an uploaded CL.')

  branches = RunGit(['for-each-ref',
                     '--format=%(refname:short) %(upstream:short)',
                     'refs/heads'])
  if not branches:
    print('No local branches found.')
    return 0

  # Create a dictionary of all local branches to the branches that are
  # dependent on it.
  tracked_to_dependents = collections.defaultdict(list)
  for b in branches.splitlines():
    tokens = b.split()
    if len(tokens) == 2:
      branch_name, tracked = tokens
      tracked_to_dependents[tracked].append(branch_name)

  print()
  print('The dependent local branches of %s are:' % root_branch)
  dependents = []

  def traverse_dependents_preorder(branch, padding=''):
    dependents_to_process = tracked_to_dependents.get(branch, [])
    padding += '  '
    for dependent in dependents_to_process:
      print('%s%s' % (padding, dependent))
      dependents.append(dependent)
      traverse_dependents_preorder(dependent, padding)

  traverse_dependents_preorder(root_branch)
  print()

  if not dependents:
    print('There are no dependent local branches for %s' % root_branch)
    return 0

  # Record all dependents that failed to upload.
  failures = {}
  # Go through all dependents, checkout the branch and upload.
  try:
    for dependent_branch in dependents:
      print()
      print('--------------------------------------')
      print('Running "git cl upload" from %s:' % dependent_branch)
      RunGit(['checkout', '-q', dependent_branch])
      print()
      try:
        if CMDupload(OptionParser(), args) != 0:
          print('Upload failed for %s!' % dependent_branch)
          failures[dependent_branch] = 1
      except:  # pylint: disable=bare-except
        failures[dependent_branch] = 1
      print()
  finally:
    # Swap back to the original root branch.
    RunGit(['checkout', '-q', root_branch])

  print()
  print('Upload complete for dependent branches!')
  for dependent_branch in dependents:
    upload_status = 'failed' if failures.get(dependent_branch) else 'succeeded'
    print('  %s : %s' % (dependent_branch, upload_status))
  print()

  return 0


def GetArchiveTagForBranch(issue_num, branch_name, existing_tags, pattern):
  """Given a proposed tag name, returns a tag name that is guaranteed to be
  unique. If 'foo' is proposed but already exists, then 'foo-2' is used,
  or 'foo-3', and so on."""

  proposed_tag = pattern.format(**{'issue': issue_num, 'branch': branch_name})
  for suffix_num in itertools.count(1):
    if suffix_num == 1:
      to_check = proposed_tag
    else:
      to_check = '%s-%d' % (proposed_tag, suffix_num)

    if to_check not in existing_tags:
      return to_check


@metrics.collector.collect_metrics('git cl archive')
def CMDarchive(parser, args):
  """Archives and deletes branches associated with closed changelists."""
  parser.add_option(
      '-j', '--maxjobs', action='store', type=int,
      help='The maximum number of jobs to use when retrieving review status.')
  parser.add_option(
      '-f', '--force', action='store_true',
      help='Bypasses the confirmation prompt.')
  parser.add_option(
      '-d', '--dry-run', action='store_true',
      help='Skip the branch tagging and removal steps.')
  parser.add_option(
      '-t', '--notags', action='store_true',
      help='Do not tag archived branches. '
           'Note: local commit history may be lost.')
  parser.add_option(
      '-p',
      '--pattern',
      default='git-cl-archived-{issue}-{branch}',
      help='Format string for archive tags. '
      'E.g. \'archived-{issue}-{branch}\'.')

  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported args: %s' % ' '.join(args))

  branches = RunGit(['for-each-ref', '--format=%(refname)', 'refs/heads'])
  if not branches:
    return 0

  tags = RunGit(['for-each-ref', '--format=%(refname)',
                 'refs/tags']).splitlines() or []
  tags = [t.split('/')[-1] for t in tags]

  print('Finding all branches associated with closed issues...')
  changes = [Changelist(branchref=b)
             for b in branches.splitlines()]
  alignment = max(5, max(len(c.GetBranch()) for c in changes))
  statuses = get_cl_statuses(changes,
                             fine_grained=True,
                             max_processes=options.maxjobs)
  proposal = [(cl.GetBranch(),
               GetArchiveTagForBranch(cl.GetIssue(), cl.GetBranch(), tags,
                                      options.pattern))
              for cl, status in statuses
              if status in ('closed', 'rietveld-not-supported')]
  proposal.sort()

  if not proposal:
    print('No branches with closed codereview issues found.')
    return 0

  current_branch = scm.GIT.GetBranch(settings.GetRoot())

  print('\nBranches with closed issues that will be archived:\n')
  if options.notags:
    for next_item in proposal:
      print('  ' + next_item[0])
  else:
    print('%*s | %s' % (alignment, 'Branch name', 'Archival tag name'))
    for next_item in proposal:
      print('%*s   %s' % (alignment, next_item[0], next_item[1]))

  # Quit now on precondition failure or if instructed by the user, either
  # via an interactive prompt or by command line flags.
  if options.dry_run:
    print('\nNo changes were made (dry run).\n')
    return 0

  if any(branch == current_branch for branch, _ in proposal):
    print('You are currently on a branch \'%s\' which is associated with a '
          'closed codereview issue, so archive cannot proceed. Please '
          'checkout another branch and run this command again.' %
          current_branch)
    return 1

  if not options.force:
    answer = gclient_utils.AskForData('\nProceed with deletion (Y/n)? ').lower()
    if answer not in ('y', ''):
      print('Aborted.')
      return 1

  for branch, tagname in proposal:
    if not options.notags:
      RunGit(['tag', tagname, branch])

    if RunGitWithCode(['branch', '-D', branch])[0] != 0:
      # Clean up the tag if we failed to delete the branch.
      RunGit(['tag', '-d', tagname])

  print('\nJob\'s done!')

  return 0


@metrics.collector.collect_metrics('git cl status')
def CMDstatus(parser, args):
  """Show status of changelists.

  Colors are used to tell the state of the CL unless --fast is used:
    - Blue     waiting for review
    - Yellow   waiting for you to reply to review, or not yet sent
    - Green    LGTM'ed
    - Red      'not LGTM'ed
    - Magenta  in the CQ
    - Cyan     was committed, branch can be deleted
    - White    error, or unknown status

  Also see 'git cl comments'.
  """
  parser.add_option(
      '--no-branch-color',
      action='store_true',
      help='Disable colorized branch names')
  parser.add_option('--field',
                    help='print only specific field (desc|id|patch|status|url)')
  parser.add_option('-f', '--fast', action='store_true',
                    help='Do not retrieve review status')
  parser.add_option(
      '-j', '--maxjobs', action='store', type=int,
      help='The maximum number of jobs to use when retrieving review status')
  parser.add_option(
      '-i', '--issue', type=int,
      help='Operate on this issue instead of the current branch\'s implicit '
      'issue. Requires --field to be set.')
  parser.add_option('-d',
                    '--date-order',
                    action='store_true',
                    help='Order branches by committer date.')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unsupported args: %s' % args)

  if options.issue is not None and not options.field:
    parser.error('--field must be given when --issue is set.')

  if options.field:
    cl = Changelist(issue=options.issue)
    if options.field.startswith('desc'):
      if cl.GetIssue():
        print(cl.FetchDescription())
    elif options.field == 'id':
      issueid = cl.GetIssue()
      if issueid:
        print(issueid)
    elif options.field == 'patch':
      patchset = cl.GetMostRecentPatchset()
      if patchset:
        print(patchset)
    elif options.field == 'status':
      print(cl.GetStatus())
    elif options.field == 'url':
      url = cl.GetIssueURL()
      if url:
        print(url)
    return 0

  branches = RunGit([
      'for-each-ref', '--format=%(refname) %(committerdate:unix)', 'refs/heads'
  ])
  if not branches:
    print('No local branch found.')
    return 0

  changes = [
      Changelist(branchref=b, commit_date=ct)
      for b, ct in map(lambda line: line.split(' '), branches.splitlines())
  ]
  print('Branches associated with reviews:')
  output = get_cl_statuses(changes,
                           fine_grained=not options.fast,
                           max_processes=options.maxjobs)

  current_branch = scm.GIT.GetBranch(settings.GetRoot())

  def FormatBranchName(branch, colorize=False):
    """Simulates 'git branch' behavior. Colorizes and prefixes branch name with
    an asterisk when it is the current branch."""

    asterisk = ""
    color = Fore.RESET
    if branch == current_branch:
      asterisk = "* "
      color = Fore.GREEN
    branch_name = scm.GIT.ShortBranchName(branch)

    if colorize:
      return asterisk + color + branch_name + Fore.RESET
    return asterisk + branch_name

  branch_statuses = {}

  alignment = max(5, max(len(FormatBranchName(c.GetBranch())) for c in changes))

  if options.date_order or settings.IsStatusCommitOrderByDate():
    sorted_changes = sorted(changes,
                            key=lambda c: c.GetCommitDate(),
                            reverse=True)
  else:
    sorted_changes = sorted(changes, key=lambda c: c.GetBranch())
  for cl in sorted_changes:
    branch = cl.GetBranch()
    while branch not in branch_statuses:
      c, status = next(output)
      branch_statuses[c.GetBranch()] = status
    status = branch_statuses.pop(branch)
    url = cl.GetIssueURL(short=True)
    if url and (not status or status == 'error'):
      # The issue probably doesn't exist anymore.
      url += ' (broken)'

    color = color_for_status(status)
    # Turn off bold as well as colors.
    END = '\033[0m'
    reset = Fore.RESET + END
    if not setup_color.IS_TTY:
      color = ''
      reset = ''
    status_str = '(%s)' % status if status else ''

    branch_display = FormatBranchName(branch)
    padding = ' ' * (alignment - len(branch_display))
    if not options.no_branch_color:
      branch_display = FormatBranchName(branch, colorize=True)

    print('  %s : %s%s %s%s' % (padding + branch_display, color, url,
                                status_str, reset))

  print()
  print('Current branch: %s' % current_branch)
  for cl in changes:
    if cl.GetBranch() == current_branch:
      break
  if not cl.GetIssue():
    print('No issue assigned.')
    return 0
  print('Issue number: %s (%s)' % (cl.GetIssue(), cl.GetIssueURL()))
  if not options.fast:
    print('Issue description:')
    print(cl.FetchDescription(pretty=True))
  return 0


def colorize_CMDstatus_doc():
  """To be called once in main() to add colors to git cl status help."""
  colors = [i for i in dir(Fore) if i[0].isupper()]

  def colorize_line(line):
    for color in colors:
      if color in line.upper():
        # Extract whitespace first and the leading '-'.
        indent = len(line) - len(line.lstrip(' ')) + 1
        return line[:indent] + getattr(Fore, color) + line[indent:] + Fore.RESET
    return line

  lines = CMDstatus.__doc__.splitlines()
  CMDstatus.__doc__ = '\n'.join(colorize_line(l) for l in lines)


def write_json(path, contents):
  if path == '-':
    json.dump(contents, sys.stdout)
  else:
    with open(path, 'w') as f:
      json.dump(contents, f)


@subcommand.usage('[issue_number]')
@metrics.collector.collect_metrics('git cl issue')
def CMDissue(parser, args):
  """Sets or displays the current code review issue number.

  Pass issue number 0 to clear the current issue.
  """
  parser.add_option('-r', '--reverse', action='store_true',
                    help='Lookup the branch(es) for the specified issues. If '
                         'no issues are specified, all branches with mapped '
                         'issues will be listed.')
  parser.add_option('--json',
                    help='Path to JSON output file, or "-" for stdout.')
  options, args = parser.parse_args(args)

  if options.reverse:
    branches = RunGit(['for-each-ref', 'refs/heads',
                       '--format=%(refname)']).splitlines()
    # Reverse issue lookup.
    issue_branch_map = {}

    git_config = {}
    for config in RunGit(['config', '--get-regexp',
                          r'branch\..*issue']).splitlines():
      name, _space, val = config.partition(' ')
      git_config[name] = val

    for branch in branches:
      issue = git_config.get(
          'branch.%s.%s' % (scm.GIT.ShortBranchName(branch), ISSUE_CONFIG_KEY))
      if issue:
        issue_branch_map.setdefault(int(issue), []).append(branch)
    if not args:
      args = sorted(issue_branch_map.keys())
    result = {}
    for issue in args:
      try:
        issue_num = int(issue)
      except ValueError:
        print('ERROR cannot parse issue number: %s' % issue, file=sys.stderr)
        continue
      result[issue_num] = issue_branch_map.get(issue_num)
      print('Branch for issue number %s: %s' %
            (issue, ', '.join(issue_branch_map.get(issue_num) or ('None', ))))
    if options.json:
      write_json(options.json, result)
    return 0

  if len(args) > 0:
    issue = ParseIssueNumberArgument(args[0])
    if not issue.valid:
      DieWithError('Pass a url or number to set the issue, 0 to unset it, '
                   'or no argument to list it.\n'
                   'Maybe you want to run git cl status?')
    cl = Changelist()
    cl.SetIssue(issue.issue)
  else:
    cl = Changelist()
  print('Issue number: %s (%s)' % (cl.GetIssue(), cl.GetIssueURL()))
  if options.json:
    write_json(options.json, {
      'gerrit_host': cl.GetGerritHost(),
      'gerrit_project': cl.GetGerritProject(),
      'issue_url': cl.GetIssueURL(),
      'issue': cl.GetIssue(),
    })
  return 0


@metrics.collector.collect_metrics('git cl comments')
def CMDcomments(parser, args):
  """Shows or posts review comments for any changelist."""
  parser.add_option('-a', '--add-comment', dest='comment',
                    help='comment to add to an issue')
  parser.add_option('-p', '--publish', action='store_true',
                    help='marks CL as ready and sends comment to reviewers')
  parser.add_option('-i', '--issue', dest='issue',
                    help='review issue id (defaults to current issue).')
  parser.add_option('-m', '--machine-readable', dest='readable',
                    action='store_false', default=True,
                    help='output comments in a format compatible with '
                         'editor parsing')
  parser.add_option('-j', '--json-file',
                    help='File to write JSON summary to, or "-" for stdout')
  options, args = parser.parse_args(args)

  issue = None
  if options.issue:
    try:
      issue = int(options.issue)
    except ValueError:
      DieWithError('A review issue ID is expected to be a number.')

  cl = Changelist(issue=issue)

  if options.comment:
    cl.AddComment(options.comment, options.publish)
    return 0

  summary = sorted(cl.GetCommentsSummary(readable=options.readable),
                   key=lambda c: c.date)
  for comment in summary:
    if comment.disapproval:
      color = Fore.RED
    elif comment.approval:
      color = Fore.GREEN
    elif comment.sender == cl.GetIssueOwner():
      color = Fore.MAGENTA
    elif comment.autogenerated:
      color = Fore.CYAN
    else:
      color = Fore.BLUE
    print('\n%s%s   %s%s\n%s' % (
      color,
      comment.date.strftime('%Y-%m-%d %H:%M:%S UTC'),
      comment.sender,
      Fore.RESET,
      '\n'.join('  ' + l for l in comment.message.strip().splitlines())))

  if options.json_file:
    def pre_serialize(c):
      dct = c._asdict().copy()
      dct['date'] = dct['date'].strftime('%Y-%m-%d %H:%M:%S.%f')
      return dct
    write_json(options.json_file, [pre_serialize(x) for x in summary])
  return 0


@subcommand.usage('[codereview url or issue id]')
@metrics.collector.collect_metrics('git cl description')
def CMDdescription(parser, args):
  """Brings up the editor for the current CL's description."""
  parser.add_option('-d', '--display', action='store_true',
                    help='Display the description instead of opening an editor')
  parser.add_option('-n', '--new-description',
                    help='New description to set for this issue (- for stdin, '
                         '+ to load from local commit HEAD)')
  parser.add_option('-f', '--force', action='store_true',
                    help='Delete any unpublished Gerrit edits for this issue '
                         'without prompting')

  options, args = parser.parse_args(args)

  target_issue_arg = None
  if len(args) > 0:
    target_issue_arg = ParseIssueNumberArgument(args[0])
    if not target_issue_arg.valid:
      parser.error('Invalid issue ID or URL.')

  kwargs = {}
  if target_issue_arg:
    kwargs['issue'] = target_issue_arg.issue
    kwargs['codereview_host'] = target_issue_arg.hostname

  cl = Changelist(**kwargs)
  if not cl.GetIssue():
    DieWithError('This branch has no associated changelist.')

  if args and not args[0].isdigit():
    logging.info('canonical issue/change URL: %s\n', cl.GetIssueURL())

  description = ChangeDescription(cl.FetchDescription())

  if options.display:
    print(description.description)
    return 0

  if options.new_description:
    text = options.new_description
    if text == '-':
      text = '\n'.join(l.rstrip() for l in sys.stdin)
    elif text == '+':
      base_branch = cl.GetCommonAncestorWithUpstream()
      text = _create_description_from_log([base_branch])

    description.set_description(text)
  else:
    description.prompt()
  if cl.FetchDescription().strip() != description.description:
    cl.UpdateDescription(description.description, force=options.force)
  return 0


@metrics.collector.collect_metrics('git cl lint')
def CMDlint(parser, args):
  """Runs cpplint on the current changelist."""
  parser.add_option('--filter', action='append', metavar='-x,+y',
                    help='Comma-separated list of cpplint\'s category-filters')
  options, args = parser.parse_args(args)

  # Access to a protected member _XX of a client class
  # pylint: disable=protected-access
  try:
    import cpplint
    import cpplint_chromium
  except ImportError:
    print('Your depot_tools is missing cpplint.py and/or cpplint_chromium.py.')
    return 1

  # Change the current working directory before calling lint so that it
  # shows the correct base.
  previous_cwd = os.getcwd()
  os.chdir(settings.GetRoot())
  try:
    cl = Changelist()
    files = cl.GetAffectedFiles(cl.GetCommonAncestorWithUpstream())
    if not files:
      print('Cannot lint an empty CL')
      return 1

    # Process cpplint arguments, if any.
    filters = presubmit_canned_checks.GetCppLintFilters(options.filter)
    command = ['--filter=' + ','.join(filters)] + args + files
    filenames = cpplint.ParseArguments(command)

    include_regex = re.compile(settings.GetLintRegex())
    ignore_regex = re.compile(settings.GetLintIgnoreRegex())
    extra_check_functions = [cpplint_chromium.CheckPointerDeclarationWhitespace]
    for filename in filenames:
      if not include_regex.match(filename):
        print('Skipping file %s' % filename)
        continue

      if ignore_regex.match(filename):
        print('Ignoring file %s' % filename)
        continue

      cpplint.ProcessFile(filename, cpplint._cpplint_state.verbose_level,
                          extra_check_functions)
  finally:
    os.chdir(previous_cwd)
  print('Total errors found: %d\n' % cpplint._cpplint_state.error_count)
  if cpplint._cpplint_state.error_count != 0:
    return 1
  return 0


@metrics.collector.collect_metrics('git cl presubmit')
def CMDpresubmit(parser, args):
  """Runs presubmit tests on the current changelist."""
  parser.add_option('-u', '--upload', action='store_true',
                    help='Run upload hook instead of the push hook')
  parser.add_option('-f', '--force', action='store_true',
                    help='Run checks even if tree is dirty')
  parser.add_option('--all', action='store_true',
                    help='Run checks against all files, not just modified ones')
  parser.add_option('--files',
                    nargs=1,
                    help='Semicolon-separated list of files to be marked as '
                    'modified when executing presubmit or post-upload hooks. '
                    'fnmatch wildcards can also be used.')
  parser.add_option('--parallel', action='store_true',
                    help='Run all tests specified by input_api.RunTests in all '
                         'PRESUBMIT files in parallel.')
  parser.add_option('--resultdb', action='store_true',
                    help='Run presubmit checks in the ResultSink environment '
                         'and send results to the ResultDB database.')
  parser.add_option('--realm', help='LUCI realm if reporting to ResultDB')
  options, args = parser.parse_args(args)

  if not options.force and git_common.is_dirty_git_tree('presubmit'):
    print('use --force to check even if tree is dirty.')
    return 1

  cl = Changelist()
  if args:
    base_branch = args[0]
  else:
    # Default to diffing against the common ancestor of the upstream branch.
    base_branch = cl.GetCommonAncestorWithUpstream()

  start = time.time()
  try:
    if not 'PRESUBMIT_SKIP_NETWORK' in os.environ and cl.GetIssue():
      description = cl.FetchDescription()
    else:
      description = _create_description_from_log([base_branch])
  except Exception as e:
    print('Failed to fetch CL description - %s' % str(e))
    description = _create_description_from_log([base_branch])
  elapsed = time.time() - start
  if elapsed > 5:
    print('%.1f s to get CL description.' % elapsed)

  if not base_branch:
    if not options.force:
      print('use --force to check even when not on a branch.')
      return 1
    base_branch = 'HEAD'

  cl.RunHook(committing=not options.upload,
             may_prompt=False,
             verbose=options.verbose,
             parallel=options.parallel,
             upstream=base_branch,
             description=description,
             all_files=options.all,
             files=options.files,
             resultdb=options.resultdb,
             realm=options.realm)
  return 0


def GenerateGerritChangeId(message):
  """Returns the Change ID footer value (Ixxxxxx...xxx).

  Works the same way as
  https://gerrit-review.googlesource.com/tools/hooks/commit-msg
  but can be called on demand on all platforms.

  The basic idea is to generate git hash of a state of the tree, original
  commit message, author/committer info and timestamps.
  """
  lines = []
  tree_hash = RunGitSilent(['write-tree'])
  lines.append('tree %s' % tree_hash.strip())
  code, parent = RunGitWithCode(['rev-parse', 'HEAD~0'], suppress_stderr=False)
  if code == 0:
    lines.append('parent %s' % parent.strip())
  author = RunGitSilent(['var', 'GIT_AUTHOR_IDENT'])
  lines.append('author %s' % author.strip())
  committer = RunGitSilent(['var', 'GIT_COMMITTER_IDENT'])
  lines.append('committer %s' % committer.strip())
  lines.append('')
  # Note: Gerrit's commit-hook actually cleans message of some lines and
  # whitespace. This code is not doing this, but it clearly won't decrease
  # entropy.
  lines.append(message)
  change_hash = RunCommand(['git', 'hash-object', '-t', 'commit', '--stdin'],
                           stdin=('\n'.join(lines)).encode())
  return 'I%s' % change_hash.strip()


def GetTargetRef(remote, remote_branch, target_branch):
  """Computes the remote branch ref to use for the CL.

  Args:
    remote (str): The git remote for the CL.
    remote_branch (str): The git remote branch for the CL.
    target_branch (str): The target branch specified by the user.
  """
  if not (remote and remote_branch):
    return None

  if target_branch:
    # Canonicalize branch references to the equivalent local full symbolic
    # refs, which are then translated into the remote full symbolic refs
    # below.
    if '/' not in target_branch:
      remote_branch = 'refs/remotes/%s/%s' % (remote, target_branch)
    else:
      prefix_replacements = (
        ('^((refs/)?remotes/)?branch-heads/', 'refs/remotes/branch-heads/'),
        ('^((refs/)?remotes/)?%s/' % remote,  'refs/remotes/%s/' % remote),
        ('^(refs/)?heads/',                   'refs/remotes/%s/' % remote),
      )
      match = None
      for regex, replacement in prefix_replacements:
        match = re.search(regex, target_branch)
        if match:
          remote_branch = target_branch.replace(match.group(0), replacement)
          break
      if not match:
        # This is a branch path but not one we recognize; use as-is.
        remote_branch = target_branch
  # pylint: disable=consider-using-get
  elif remote_branch in REFS_THAT_ALIAS_TO_OTHER_REFS:
    # pylint: enable=consider-using-get
    # Handle the refs that need to land in different refs.
    remote_branch = REFS_THAT_ALIAS_TO_OTHER_REFS[remote_branch]

  # Create the true path to the remote branch.
  # Does the following translation:
  # * refs/remotes/origin/refs/diff/test -> refs/diff/test
  # * refs/remotes/origin/main -> refs/heads/main
  # * refs/remotes/branch-heads/test -> refs/branch-heads/test
  if remote_branch.startswith('refs/remotes/%s/refs/' % remote):
    remote_branch = remote_branch.replace('refs/remotes/%s/' % remote, '')
  elif remote_branch.startswith('refs/remotes/%s/' % remote):
    remote_branch = remote_branch.replace('refs/remotes/%s/' % remote,
                                          'refs/heads/')
  elif remote_branch.startswith('refs/remotes/branch-heads'):
    remote_branch = remote_branch.replace('refs/remotes/', 'refs/')

  return remote_branch


def cleanup_list(l):
  """Fixes a list so that comma separated items are put as individual items.

  So that "--reviewers joe@c,john@c --reviewers joa@c" results in
  options.reviewers == sorted(['joe@c', 'john@c', 'joa@c']).
  """
  items = sum((i.split(',') for i in l), [])
  stripped_items = (i.strip() for i in items)
  return sorted(filter(None, stripped_items))


@subcommand.usage('[flags]')
@metrics.collector.collect_metrics('git cl upload')
def CMDupload(parser, args):
  """Uploads the current changelist to codereview.

  Can skip dependency patchset uploads for a branch by running:
    git config branch.branch_name.skip-deps-uploads True
  To unset, run:
    git config --unset branch.branch_name.skip-deps-uploads
  Can also set the above globally by using the --global flag.

  If the name of the checked out branch starts with "bug-" or "fix-" followed
  by a bug number, this bug number is automatically populated in the CL
  description.

  If subject contains text in square brackets or has "<text>: " prefix, such
  text(s) is treated as Gerrit hashtags. For example, CLs with subjects:
    [git-cl] add support for hashtags
    Foo bar: implement foo
  will be hashtagged with "git-cl" and "foo-bar" respectively.
  """
  parser.add_option('--bypass-hooks', action='store_true', dest='bypass_hooks',
                    help='bypass upload presubmit hook')
  parser.add_option('--bypass-watchlists', action='store_true',
                    dest='bypass_watchlists',
                    help='bypass watchlists auto CC-ing reviewers')
  parser.add_option('-f', '--force', action='store_true', dest='force',
                    help="force yes to questions (don't prompt)")
  parser.add_option('--message', '-m', dest='message',
                    help='message for patchset')
  parser.add_option('-b', '--bug',
                    help='pre-populate the bug number(s) for this issue. '
                         'If several, separate with commas')
  parser.add_option('--message-file', dest='message_file',
                    help='file which contains message for patchset')
  parser.add_option('--title', '-t', dest='title',
                    help='title for patchset')
  parser.add_option('-T', '--skip-title', action='store_true',
                    dest='skip_title',
                    help='Use the most recent commit message as the title of '
                         'the patchset')
  parser.add_option('-r', '--reviewers',
                    action='append', default=[],
                    help='reviewer email addresses')
  parser.add_option('--cc',
                    action='append', default=[],
                    help='cc email addresses')
  parser.add_option('--hashtag', dest='hashtags',
                    action='append', default=[],
                    help=('Gerrit hashtag for new CL; '
                          'can be applied multiple times'))
  parser.add_option('-s',
                    '--send-mail',
                    '--send-email',
                    dest='send_mail',
                    action='store_true',
                    help='send email to reviewer(s) and cc(s) immediately')
  parser.add_option('--target_branch',
                    '--target-branch',
                    metavar='TARGET',
                    help='Apply CL to remote ref TARGET.  ' +
                         'Default: remote branch head, or main')
  parser.add_option('--squash', action='store_true',
                    help='Squash multiple commits into one')
  parser.add_option('--no-squash', action='store_false', dest='squash',
                    help='Don\'t squash multiple commits into one')
  parser.add_option('--topic', default=None,
                    help='Topic to specify when uploading')
  parser.add_option('--r-owners', dest='add_owners_to', action='store_const',
                    const='R', help='add a set of OWNERS to R')
  parser.add_option('-c',
                    '--use-commit-queue',
                    action='store_true',
                    default=False,
                    help='tell the CQ to commit this patchset; '
                    'implies --send-mail')
  parser.add_option('-d', '--cq-dry-run',
                    action='store_true', default=False,
                    help='Send the patchset to do a CQ dry run right after '
                         'upload.')
  parser.add_option(
      '-q',
      '--cq-quick-run',
      action='store_true',
      default=False,
      help='Send the patchset to do a CQ quick run right after '
           'upload (https://source.chromium.org/chromium/chromium/src/+/main:do'
           'cs/cq_quick_run.md) (chromium only).')
  parser.add_option('--set-bot-commit', action='store_true',
                    help=optparse.SUPPRESS_HELP)
  parser.add_option('--preserve-tryjobs', action='store_true',
                    help='instruct the CQ to let tryjobs running even after '
                         'new patchsets are uploaded instead of canceling '
                         'prior patchset\' tryjobs')
  parser.add_option('--dependencies', action='store_true',
                    help='Uploads CLs of all the local branches that depend on '
                         'the current branch')
  parser.add_option('-a', '--enable-auto-submit', action='store_true',
                    help='Sends your change to the CQ after an approval. Only '
                         'works on repos that have the Auto-Submit label '
                         'enabled')
  parser.add_option('--parallel', action='store_true',
                    help='Run all tests specified by input_api.RunTests in all '
                         'PRESUBMIT files in parallel.')
  parser.add_option('--no-autocc', action='store_true',
                    help='Disables automatic addition of CC emails')
  parser.add_option('--private', action='store_true',
                    help='Set the review private. This implies --no-autocc.')
  parser.add_option('-R', '--retry-failed', action='store_true',
                    help='Retry failed tryjobs from old patchset immediately '
                         'after uploading new patchset. Cannot be used with '
                         '--use-commit-queue or --cq-dry-run.')
  parser.add_option('--fixed', '-x',
                    help='List of bugs that will be commented on and marked '
                         'fixed (pre-populates "Fixed:" tag). Same format as '
                         '-b option / "Bug:" tag. If fixing several issues, '
                         'separate with commas.')
  parser.add_option('--edit-description', action='store_true', default=False,
                    help='Modify description before upload. Cannot be used '
                         'with --force. It is a noop when --no-squash is set '
                         'or a new commit is created.')
  parser.add_option('--git-completion-helper', action="store_true",
                    help=optparse.SUPPRESS_HELP)
  parser.add_option('-o',
                    '--push-options',
                    action='append',
                    default=[],
                    help='Transmit the given string to the server when '
                    'performing git push (pass-through). See git-push '
                    'documentation for more details.')
  parser.add_option('--no-add-changeid',
                    action='store_true',
                    dest='no_add_changeid',
                    help='Do not add change-ids to messages.')
  parser.add_option('--no-python2-post-upload-hooks',
                    action='store_true',
                    help='Only run post-upload hooks in Python 3.')
  parser.add_option('--cherry-pick-stacked',
                    '--cp',
                    dest='cherry_pick_stacked',
                    action='store_true',
                    help='If parent branch has un-uploaded updates, '
                    'automatically skip parent branches and just upload '
                    'the current branch cherry-pick on its parent\'s last '
                    'uploaded commit. Allows users to skip the potential '
                    'interactive confirmation step.')
  # TODO(b/265929888): Add --wip option of --cl-status option.

  orig_args = args
  (options, args) = parser.parse_args(args)

  if options.git_completion_helper:
    print(' '.join(opt.get_opt_string() for opt in parser.option_list
                   if opt.help != optparse.SUPPRESS_HELP))
    return

  if git_common.is_dirty_git_tree('upload'):
    return 1

  options.reviewers = cleanup_list(options.reviewers)
  options.cc = cleanup_list(options.cc)

  if options.edit_description and options.force:
    parser.error('Only one of --force and --edit-description allowed')

  if options.message_file:
    if options.message:
      parser.error('Only one of --message and --message-file allowed.')
    options.message = gclient_utils.FileRead(options.message_file)

  if ([options.cq_dry_run,
       options.cq_quick_run,
       options.use_commit_queue,
       options.retry_failed].count(True) > 1):
    parser.error('Only one of --use-commit-queue, --cq-dry-run, --cq-quick-run '
                 'or --retry-failed is allowed.')

  if options.skip_title and options.title:
    parser.error('Only one of --title and --skip-title allowed.')

  if options.use_commit_queue:
    options.send_mail = True

  if options.squash is None:
    # Load default for user, repo, squash=true, in this order.
    options.squash = settings.GetSquashGerritUploads()

  cl = Changelist(branchref=options.target_branch)

  # Warm change details cache now to avoid RPCs later, reducing latency for
  # developers.
  if cl.GetIssue():
    cl._GetChangeDetail(
        ['DETAILED_ACCOUNTS', 'CURRENT_REVISION', 'CURRENT_COMMIT', 'LABELS'])

  if options.retry_failed and not cl.GetIssue():
    print('No previous patchsets, so --retry-failed has no effect.')
    options.retry_failed = False

  remote = cl.GetRemoteUrl()
  dogfood_stacked_changes = (os.environ.get(DOGFOOD_STACKED_CHANGES_VAR)
                             not in ['1', '0']
                             and any(repo in remote
                                     for repo in DOGFOOD_STACKED_CHANGES_REPOS))

  if dogfood_stacked_changes:
    print('This repo has been enrolled in the stacked changes dogfood. '
          'To opt-out use `export DOGFOOD_STACKED_CHANGES=0`. '
          'File bugs at https://bit.ly/3Y6opoI')

  if options.squash and (dogfood_stacked_changes
                         or os.environ.get(DOGFOOD_STACKED_CHANGES_VAR) == '1'):
    print('Depot Tools no longer sets new uploads to "WIP". Please update the\n'
          '"Set new changes to "work in progress" by default" checkbox at\n'
          'https://<host>-review.googlesource.com/settings/')
    if options.dependencies:
      parser.error('--dependencies is not available for this workflow.')

    if options.cherry_pick_stacked:
      try:
        orig_args.remove('--cherry-pick-stacked')
      except ValueError:
        orig_args.remove('--cp')
    UploadAllSquashed(options, orig_args)
    return 0

  if options.cherry_pick_stacked:
    parser.error('--cherry-pick-stacked is not available for this workflow.')

  # cl.GetMostRecentPatchset uses cached information, and can return the last
  # patchset before upload. Calling it here makes it clear that it's the
  # last patchset before upload. Note that GetMostRecentPatchset will fail
  # if no CL has been uploaded yet.
  if options.retry_failed:
    patchset = cl.GetMostRecentPatchset()

  ret = cl.CMDUpload(options, args, orig_args)

  if options.retry_failed:
    if ret != 0:
      print('Upload failed, so --retry-failed has no effect.')
      return ret
    builds, _ = _fetch_latest_builds(cl,
                                     DEFAULT_BUILDBUCKET_HOST,
                                     latest_patchset=patchset)
    jobs = _filter_failed_for_retry(builds)
    if len(jobs) == 0:
      print('No failed tryjobs, so --retry-failed has no effect.')
      return ret
    _trigger_tryjobs(cl, jobs, options, patchset + 1)

  return ret


def UploadAllSquashed(options, orig_args):
  # type: (optparse.Values, Sequence[str]) -> Tuple[Sequence[Changelist], bool]
  """Uploads the current and upstream branches (if necessary)."""
  cls, cherry_pick_current = _UploadAllPrecheck(options, orig_args)

  # Create commits.
  uploads_by_cl = []  #type: Sequence[Tuple[Changelist, _NewUpload]]
  if cherry_pick_current:
    parent = cls[1]._GitGetBranchConfigValue(GERRIT_SQUASH_HASH_CONFIG_KEY)
    new_upload = cls[0].PrepareCherryPickSquashedCommit(options, parent)
    uploads_by_cl.append((cls[0], new_upload))
  else:
    ordered_cls = list(reversed(cls))

    cl = ordered_cls[0]
    # We can only support external changes when we're only uploading one
    # branch.
    parent = cl._UpdateWithExternalChanges() if len(ordered_cls) == 1 else None
    orig_parent = None
    if parent is None:
      origin = '.'
      branch = cl.GetBranch()

      while origin == '.':
        # Search for cl's closest ancestor with a gerrit hash.
        origin, upstream_branch_ref = Changelist.FetchUpstreamTuple(branch)
        if origin == '.':
          upstream_branch = scm.GIT.ShortBranchName(upstream_branch_ref)

          # Support the `git merge` and `git pull` workflow.
          if upstream_branch in ['master', 'main']:
            parent = cl.GetCommonAncestorWithUpstream()
          else:
            orig_parent = scm.GIT.GetBranchConfig(settings.GetRoot(),
                                                  upstream_branch,
                                                  LAST_UPLOAD_HASH_CONFIG_KEY)
            parent = scm.GIT.GetBranchConfig(settings.GetRoot(),
                                             upstream_branch,
                                             GERRIT_SQUASH_HASH_CONFIG_KEY)
          if parent:
            break
          branch = upstream_branch
      else:
        # Either the root of the tree is the cl's direct parent and the while
        # loop above only found empty branches between cl and the root of the
        # tree.
        parent = cl.GetCommonAncestorWithUpstream()

    if orig_parent is None:
      orig_parent = parent
    for i, cl in enumerate(ordered_cls):
      # If we're in the middle of the stack, set end_commit to downstream's
      # direct ancestor.
      if i + 1 < len(ordered_cls):
        child_base_commit = ordered_cls[i + 1].GetCommonAncestorWithUpstream()
      else:
        child_base_commit = None
      new_upload = cl.PrepareSquashedCommit(options,
                                            parent,
                                            orig_parent,
                                            end_commit=child_base_commit)
      uploads_by_cl.append((cl, new_upload))
      parent = new_upload.commit_to_push
      orig_parent = child_base_commit

  # Create refspec options
  cl, new_upload = uploads_by_cl[-1]
  refspec_opts = cl._GetRefSpecOptions(
      options,
      new_upload.change_desc,
      multi_change_upload=len(uploads_by_cl) > 1,
      dogfood_path=True)
  refspec_suffix = ''
  if refspec_opts:
    refspec_suffix = '%' + ','.join(refspec_opts)
    assert ' ' not in refspec_suffix, ('spaces not allowed in refspec: "%s"' %
                                       refspec_suffix)

  remote, remote_branch = cl.GetRemoteBranch()
  branch = GetTargetRef(remote, remote_branch, options.target_branch)
  refspec = '%s:refs/for/%s%s' % (new_upload.commit_to_push, branch,
                                  refspec_suffix)

  # Git push
  git_push_metadata = {
      'gerrit_host':
      cl.GetGerritHost(),
      'title':
      options.title or '<untitled>',
      'change_id':
      git_footers.get_footer_change_id(new_upload.change_desc.description),
      'description':
      new_upload.change_desc.description,
  }
  push_stdout = cl._RunGitPushWithTraces(refspec, refspec_opts,
                                         git_push_metadata,
                                         options.push_options)

  # Post push updates
  regex = re.compile(r'remote:\s+https?://[\w\-\.\+\/#]*/(\d+)\s.*')
  change_numbers = [
      m.group(1) for m in map(regex.match, push_stdout.splitlines()) if m
  ]

  for i, (cl, new_upload) in enumerate(uploads_by_cl):
    cl.PostUploadUpdates(options, new_upload, change_numbers[i])

  return 0


def _UploadAllPrecheck(options, orig_args):
  # type: (optparse.Values, Sequence[str]) -> Tuple[Sequence[Changelist], bool]
  """Checks the state of the tree and gives the user uploading options

  Returns: A tuple of the ordered list of changes that have new commits
      since their last upload and a boolean of whether the user wants to
      cherry-pick and upload the current branch instead of uploading all cls.
  """
  cl = Changelist()
  if cl.GetBranch() is None:
    DieWithError('Can\'t upload from detached HEAD state. Get on a branch!')

  branch_ref = None
  cls = []
  must_upload_upstream = False
  first_pass = True

  Changelist._GerritCommitMsgHookCheck(offer_removal=not options.force)

  while True:
    if len(cls) > _MAX_STACKED_BRANCHES_UPLOAD:
      DieWithError(
          'More than %s branches in the stack have not been uploaded.\n'
          'Are your branches in a misconfigured state?\n'
          'If not, please upload some upstream changes first.' %
          (_MAX_STACKED_BRANCHES_UPLOAD))

    cl = Changelist(branchref=branch_ref)

    # Only add CL if it has anything to commit.
    base_commit = cl.GetCommonAncestorWithUpstream()
    end_commit = RunGit(['rev-parse', cl.GetBranchRef()]).strip()

    diff = RunGitSilent(['diff', '%s..%s' % (base_commit, end_commit)])
    if diff:
      cls.append(cl)
      if (not first_pass and
          cl._GitGetBranchConfigValue(GERRIT_SQUASH_HASH_CONFIG_KEY) is None):
        # We are mid-stack and the user must upload their upstream branches.
        must_upload_upstream = True
    elif first_pass:  # The current branch has nothing to commit. Exit.
      DieWithError('Branch %s has nothing to commit' % cl.GetBranch())
    # Else: A mid-stack branch has nothing to commit. We do not add it to cls.
    first_pass = False

    # Cases below determine if we should continue to traverse up the tree.
    origin, upstream_branch_ref = Changelist.FetchUpstreamTuple(cl.GetBranch())
    branch_ref = upstream_branch_ref  # set branch for next run.

    upstream_branch = scm.GIT.ShortBranchName(upstream_branch_ref)
    upstream_last_upload = scm.GIT.GetBranchConfig(settings.GetRoot(),
                                                   upstream_branch,
                                                   LAST_UPLOAD_HASH_CONFIG_KEY)

    # Case 1: We've reached the beginning of the tree.
    if origin != '.':
      break

    # Case 2: If any upstream branches have never been uploaded,
    # the user MUST upload them unless they are empty. Continue to
    # next loop to add upstream if it is not empty.
    if not upstream_last_upload:
      continue

    # Case 3: If upstream's last_upload == cl.base_commit we do
    # not need to upload any more upstreams from this point on.
    # (Even if there may be diverged branches higher up the tree)
    if base_commit == upstream_last_upload:
      break

    # Case 4: If upstream's last_upload < cl.base_commit we are
    # uploading cl and upstream_cl.
    # Continue up the tree to check other branch relations.
    if scm.GIT.IsAncestor(upstream_last_upload, base_commit):
      continue

    # Case 5: If cl.base_commit < upstream's last_upload the user
    # must rebase before uploading.
    if scm.GIT.IsAncestor(base_commit, upstream_last_upload):
      DieWithError(
          'At least one branch in the stack has diverged from its upstream '
          'branch and does not contain its upstream\'s last upload.\n'
          'Please rebase the stack with `git rebase-update` before uploading.')

    # The tree went through a rebase. LAST_UPLOAD_HASH_CONFIG_KEY no longer has
    # any relation to commits in the tree. Continue up the tree until we hit
    # the root.

  # We assume all cls in the stack have the same auth requirements and only
  # check this once.
  cls[0].EnsureAuthenticated(force=options.force)

  cherry_pick = False
  if len(cls) > 1:
    opt_message = ''
    branches = ', '.join([cl.branch for cl in cls])
    if len(orig_args):
      opt_message = ('options %s will be used for all uploads.\n' % orig_args)
    if must_upload_upstream:
      msg = ('At least one parent branch in `%s` has never been uploaded '
             'and must be uploaded before/with `%s`.\n' %
             (branches, cls[1].branch))
      if options.cherry_pick_stacked:
        DieWithError(msg)
      if not options.force:
        confirm_or_exit('\n' + opt_message + msg)
    else:
      if options.cherry_pick_stacked:
        print('cherry-picking `%s` on %s\'s last upload' %
              (cls[0].branch, cls[1].branch))
        cherry_pick = True
      elif not options.force:
        answer = gclient_utils.AskForData(
            '\n' + opt_message +
            'Press enter to update branches %s.\nOr type `n` to upload only '
            '`%s` cherry-picked on %s\'s last upload:' %
            (branches, cls[0].branch, cls[1].branch))
        if answer.lower() == 'n':
          cherry_pick = True
  return cls, cherry_pick


@subcommand.usage('--description=<description file>')
@metrics.collector.collect_metrics('git cl split')
def CMDsplit(parser, args):
  """Splits a branch into smaller branches and uploads CLs.

  Creates a branch and uploads a CL for each group of files modified in the
  current branch that share a common OWNERS file. In the CL description and
  comment, the string '$directory', is replaced with the directory containing
  the shared OWNERS file.
  """
  parser.add_option('-d', '--description', dest='description_file',
                    help='A text file containing a CL description in which '
                         '$directory will be replaced by each CL\'s directory.')
  parser.add_option('-c', '--comment', dest='comment_file',
                    help='A text file containing a CL comment.')
  parser.add_option('-n', '--dry-run', dest='dry_run', action='store_true',
                    default=False,
                    help='List the files and reviewers for each CL that would '
                         'be created, but don\'t create branches or CLs.')
  parser.add_option('--cq-dry-run', action='store_true',
                    help='If set, will do a cq dry run for each uploaded CL. '
                         'Please be careful when doing this; more than ~10 CLs '
                         'has the potential to overload our build '
                         'infrastructure. Try to upload these not during high '
                         'load times (usually 11-3 Mountain View time). Email '
                         'infra-dev@chromium.org with any questions.')
  parser.add_option('-a', '--enable-auto-submit', action='store_true',
                    default=True,
                    help='Sends your change to the CQ after an approval. Only '
                         'works on repos that have the Auto-Submit label '
                         'enabled')
  parser.add_option('--max-depth',
                    type='int',
                    default=0,
                    help='The max depth to look for OWNERS files. Useful for '
                    'controlling the granularity of the split CLs, e.g. '
                    '--max-depth=1 will only split by top-level '
                    'directory. Specifying a value less than 1 means no '
                    'limit on max depth.')
  options, _ = parser.parse_args(args)

  if not options.description_file:
    parser.error('No --description flag specified.')

  def WrappedCMDupload(args):
    return CMDupload(OptionParser(), args)

  return split_cl.SplitCl(options.description_file, options.comment_file,
                          Changelist, WrappedCMDupload, options.dry_run,
                          options.cq_dry_run, options.enable_auto_submit,
                          options.max_depth, settings.GetRoot())


@subcommand.usage('DEPRECATED')
@metrics.collector.collect_metrics('git cl commit')
def CMDdcommit(parser, args):
  """DEPRECATED: Used to commit the current changelist via git-svn."""
  message = ('git-cl no longer supports committing to SVN repositories via '
             'git-svn. You probably want to use `git cl land` instead.')
  print(message)
  return 1


@subcommand.usage('[upstream branch to apply against]')
@metrics.collector.collect_metrics('git cl land')
def CMDland(parser, args):
  """Commits the current changelist via git.

  In case of Gerrit, uses Gerrit REST api to "submit" the issue, which pushes
  upstream and closes the issue automatically and atomically.
  """
  parser.add_option('--bypass-hooks', action='store_true', dest='bypass_hooks',
                    help='bypass upload presubmit hook')
  parser.add_option('-f', '--force', action='store_true', dest='force',
                    help="force yes to questions (don't prompt)")
  parser.add_option('--parallel', action='store_true',
                    help='Run all tests specified by input_api.RunTests in all '
                         'PRESUBMIT files in parallel.')
  parser.add_option('--resultdb', action='store_true',
                     help='Run presubmit checks in the ResultSink environment '
                          'and send results to the ResultDB database.')
  parser.add_option('--realm', help='LUCI realm if reporting to ResultDB')
  (options, args) = parser.parse_args(args)

  cl = Changelist()

  if not cl.GetIssue():
    DieWithError('You must upload the change first to Gerrit.\n'
                 '  If you would rather have `git cl land` upload '
                 'automatically for you, see http://crbug.com/642759')
  return cl.CMDLand(options.force, options.bypass_hooks, options.verbose,
                    options.parallel, options.resultdb, options.realm)


@subcommand.usage('<patch url or issue id or issue url>')
@metrics.collector.collect_metrics('git cl patch')
def CMDpatch(parser, args):
  """Patches in a code review."""
  parser.add_option('-b', dest='newbranch',
                    help='create a new branch off trunk for the patch')
  parser.add_option('-f', '--force', action='store_true',
                    help='overwrite state on the current or chosen branch')
  parser.add_option('-n', '--no-commit', action='store_true', dest='nocommit',
                    help='don\'t commit after patch applies.')

  group = optparse.OptionGroup(
      parser,
      'Options for continuing work on the current issue uploaded from a '
      'different clone (e.g. different machine). Must be used independently '
      'from the other options. No issue number should be specified, and the '
      'branch must have an issue number associated with it')
  group.add_option('--reapply', action='store_true', dest='reapply',
                   help='Reset the branch and reapply the issue.\n'
                        'CAUTION: This will undo any local changes in this '
                        'branch')

  group.add_option('--pull', action='store_true', dest='pull',
                    help='Performs a pull before reapplying.')
  parser.add_option_group(group)

  (options, args) = parser.parse_args(args)

  if options.reapply:
    if options.newbranch:
      parser.error('--reapply works on the current branch only.')
    if len(args) > 0:
      parser.error('--reapply implies no additional arguments.')

    cl = Changelist()
    if not cl.GetIssue():
      parser.error('Current branch must have an associated issue.')

    upstream = cl.GetUpstreamBranch()
    if upstream is None:
      parser.error('No upstream branch specified. Cannot reset branch.')

    RunGit(['reset', '--hard', upstream])
    if options.pull:
      RunGit(['pull'])

    target_issue_arg = ParseIssueNumberArgument(cl.GetIssue())
    return cl.CMDPatchWithParsedIssue(target_issue_arg, options.nocommit,
                                      options.force, False)

  if len(args) != 1 or not args[0]:
    parser.error('Must specify issue number or URL.')

  target_issue_arg = ParseIssueNumberArgument(args[0])
  if not target_issue_arg.valid:
    parser.error('Invalid issue ID or URL.')

  # We don't want uncommitted changes mixed up with the patch.
  if git_common.is_dirty_git_tree('patch'):
    return 1

  if options.newbranch:
    if options.force:
      RunGit(['branch', '-D', options.newbranch],
             stderr=subprocess2.PIPE, error_ok=True)
    git_new_branch.create_new_branch(options.newbranch)

  cl = Changelist(
      codereview_host=target_issue_arg.hostname, issue=target_issue_arg.issue)

  if not args[0].isdigit():
    print('canonical issue/change URL: %s\n' % cl.GetIssueURL())

  return cl.CMDPatchWithParsedIssue(target_issue_arg, options.nocommit,
                                    options.force, options.newbranch)


def GetTreeStatus(url=None):
  """Fetches the tree status and returns either 'open', 'closed',
  'unknown' or 'unset'."""
  url = url or settings.GetTreeStatusUrl(error_ok=True)
  if url:
    status = str(urllib.request.urlopen(url).read().lower())
    if status.find('closed') != -1 or status == '0':
      return 'closed'

    if status.find('open') != -1 or status == '1':
      return 'open'

    return 'unknown'
  return 'unset'


def GetTreeStatusReason():
  """Fetches the tree status from a json url and returns the message
  with the reason for the tree to be opened or closed."""
  url = settings.GetTreeStatusUrl()
  json_url = urllib.parse.urljoin(url, '/current?format=json')
  connection = urllib.request.urlopen(json_url)
  status = json.loads(connection.read())
  connection.close()
  return status['message']


@metrics.collector.collect_metrics('git cl tree')
def CMDtree(parser, args):
  """Shows the status of the tree."""
  _, args = parser.parse_args(args)
  status = GetTreeStatus()
  if 'unset' == status:
    print('You must configure your tree status URL by running "git cl config".')
    return 2

  print('The tree is %s' % status)
  print()
  print(GetTreeStatusReason())
  if status != 'open':
    return 1
  return 0


@metrics.collector.collect_metrics('git cl try')
def CMDtry(parser, args):
  """Triggers tryjobs using either Buildbucket or CQ dry run."""
  group = optparse.OptionGroup(parser, 'Tryjob options')
  group.add_option(
      '-b', '--bot', action='append',
      help=('IMPORTANT: specify ONE builder per --bot flag. Use it multiple '
            'times to specify multiple builders. ex: '
            '"-b win_rel -b win_layout". See '
            'the try server waterfall for the builders name and the tests '
            'available.'))
  group.add_option(
      '-B', '--bucket', default='',
      help=('Buildbucket bucket to send the try requests. Format: '
            '"luci.$LUCI_PROJECT.$LUCI_BUCKET". eg: "luci.chromium.try"'))
  group.add_option(
      '-r', '--revision',
      help='Revision to use for the tryjob; default: the revision will '
           'be determined by the try recipe that builder runs, which usually '
           'defaults to HEAD of origin/master or origin/main')
  group.add_option(
      '-c', '--clobber', action='store_true', default=False,
      help='Force a clobber before building; that is don\'t do an '
           'incremental build')
  group.add_option(
      '-q',
      '--quick-run',
      action='store_true',
      default=False,
      help='trigger in quick run mode '
           '(https://source.chromium.org/chromium/chromium/src/+/main:docs/cq_q'
           'uick_run.md) (chromium only).')
  group.add_option(
      '--category', default='git_cl_try', help='Specify custom build category.')
  group.add_option(
      '--project',
      help='Override which project to use. Projects are defined '
           'in recipe to determine to which repository or directory to '
           'apply the patch')
  group.add_option(
      '-p', '--property', dest='properties', action='append', default=[],
      help='Specify generic properties in the form -p key1=value1 -p '
           'key2=value2 etc. The value will be treated as '
           'json if decodable, or as string otherwise. '
           'NOTE: using this may make your tryjob not usable for CQ, '
           'which will then schedule another tryjob with default properties')
  group.add_option(
      '--buildbucket-host', default='cr-buildbucket.appspot.com',
      help='Host of buildbucket. The default host is %default.')
  parser.add_option_group(group)
  parser.add_option(
      '-R', '--retry-failed', action='store_true', default=False,
      help='Retry failed jobs from the latest set of tryjobs. '
           'Not allowed with --bucket and --bot options.')
  parser.add_option(
      '-i', '--issue', type=int,
      help='Operate on this issue instead of the current branch\'s implicit '
      'issue.')
  options, args = parser.parse_args(args)

  # Make sure that all properties are prop=value pairs.
  bad_params = [x for x in options.properties if '=' not in x]
  if bad_params:
    parser.error('Got properties with missing "=": %s' % bad_params)

  if args:
    parser.error('Unknown arguments: %s' % args)

  cl = Changelist(issue=options.issue)
  if not cl.GetIssue():
    parser.error('Need to upload first.')

  # HACK: warm up Gerrit change detail cache to save on RPCs.
  cl._GetChangeDetail(['DETAILED_ACCOUNTS', 'ALL_REVISIONS'])

  error_message = cl.CannotTriggerTryJobReason()
  if error_message:
    parser.error('Can\'t trigger tryjobs: %s' % error_message)

  if options.bot:
    if options.retry_failed:
      parser.error('--bot is not compatible with --retry-failed.')
    if not options.bucket:
      parser.error('A bucket (e.g. "chromium/try") is required.')

    triggered = [b for b in options.bot if 'triggered' in b]
    if triggered:
      parser.error(
          'Cannot schedule builds on triggered bots: %s.\n'
          'This type of bot requires an initial job from a parent (usually a '
          'builder). Schedule a job on the parent instead.\n' % triggered)

    if options.bucket.startswith('.master'):
      parser.error('Buildbot masters are not supported.')

    project, bucket = _parse_bucket(options.bucket)
    if project is None or bucket is None:
      parser.error('Invalid bucket: %s.' % options.bucket)
    jobs = sorted((project, bucket, bot) for bot in options.bot)
  elif options.retry_failed:
    print('Searching for failed tryjobs...')
    builds, patchset = _fetch_latest_builds(cl, DEFAULT_BUILDBUCKET_HOST)
    if options.verbose:
      print('Got %d builds in patchset #%d' % (len(builds), patchset))
    jobs = _filter_failed_for_retry(builds)
    if not jobs:
      print('There are no failed jobs in the latest set of jobs '
            '(patchset #%d), doing nothing.' % patchset)
      return 0
    num_builders = len(jobs)
    if num_builders > 10:
      confirm_or_exit('There are %d builders with failed builds.'
                      % num_builders, action='continue')
  elif options.quick_run:
    print('Scheduling CQ quick run on: %s' % cl.GetIssueURL())
    return cl.SetCQState(_CQState.QUICK_RUN)
  else:
    if options.verbose:
      print('git cl try with no bots now defaults to CQ dry run.')
    print('Scheduling CQ dry run on: %s' % cl.GetIssueURL())
    return cl.SetCQState(_CQState.DRY_RUN)

  patchset = cl.GetMostRecentPatchset()
  try:
    _trigger_tryjobs(cl, jobs, options, patchset)
  except BuildbucketResponseException as ex:
    print('ERROR: %s' % ex)
    return 1
  return 0


@metrics.collector.collect_metrics('git cl try-results')
def CMDtry_results(parser, args):
  """Prints info about results for tryjobs associated with the current CL."""
  group = optparse.OptionGroup(parser, 'Tryjob results options')
  group.add_option(
      '-p', '--patchset', type=int, help='patchset number if not current.')
  group.add_option(
      '--print-master', action='store_true', help='print master name as well.')
  group.add_option(
      '--color', action='store_true', default=setup_color.IS_TTY,
      help='force color output, useful when piping output.')
  group.add_option(
      '--buildbucket-host', default='cr-buildbucket.appspot.com',
      help='Host of buildbucket. The default host is %default.')
  group.add_option(
      '--json', help=('Path of JSON output file to write tryjob results to,'
                      'or "-" for stdout.'))
  parser.add_option_group(group)
  parser.add_option(
      '-i', '--issue', type=int,
      help='Operate on this issue instead of the current branch\'s implicit '
      'issue.')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  cl = Changelist(issue=options.issue)
  if not cl.GetIssue():
    parser.error('Need to upload first.')

  patchset = options.patchset
  if not patchset:
    patchset = cl.GetMostRecentDryRunPatchset()
    if not patchset:
      parser.error('Code review host doesn\'t know about issue %s. '
                   'No access to issue or wrong issue number?\n'
                   'Either upload first, or pass --patchset explicitly.' %
                   cl.GetIssue())

  try:
    jobs = _fetch_tryjobs(cl, DEFAULT_BUILDBUCKET_HOST, patchset)
  except BuildbucketResponseException as ex:
    print('Buildbucket error: %s' % ex)
    return 1
  if options.json:
    write_json(options.json, jobs)
  else:
    _print_tryjobs(options, jobs)
  return 0


@subcommand.usage('[new upstream branch]')
@metrics.collector.collect_metrics('git cl upstream')
def CMDupstream(parser, args):
  """Prints or sets the name of the upstream branch, if any."""
  _, args = parser.parse_args(args)
  if len(args) > 1:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  cl = Changelist()
  if args:
    # One arg means set upstream branch.
    branch = cl.GetBranch()
    RunGit(['branch', '--set-upstream-to', args[0], branch])
    cl = Changelist()
    print('Upstream branch set to %s' % (cl.GetUpstreamBranch(),))

    # Clear configured merge-base, if there is one.
    git_common.remove_merge_base(branch)
  else:
    print(cl.GetUpstreamBranch())
  return 0


@metrics.collector.collect_metrics('git cl web')
def CMDweb(parser, args):
  """Opens the current CL in the web browser."""
  parser.add_option('-p',
                    '--print-only',
                    action='store_true',
                    dest='print_only',
                    help='Only print the Gerrit URL, don\'t open it in the '
                    'browser.')
  (options, args) = parser.parse_args(args)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  issue_url = Changelist().GetIssueURL()
  if not issue_url:
    print('ERROR No issue to open', file=sys.stderr)
    return 1

  if options.print_only:
    print(issue_url)
    return 0

  # Redirect I/O before invoking browser to hide its output. For example, this
  # allows us to hide the "Created new window in existing browser session."
  # message from Chrome. Based on https://stackoverflow.com/a/2323563.
  saved_stdout = os.dup(1)
  saved_stderr = os.dup(2)
  os.close(1)
  os.close(2)
  os.open(os.devnull, os.O_RDWR)
  try:
    webbrowser.open(issue_url)
  finally:
    os.dup2(saved_stdout, 1)
    os.dup2(saved_stderr, 2)
  return 0


@metrics.collector.collect_metrics('git cl set-commit')
def CMDset_commit(parser, args):
  """Sets the commit bit to trigger the CQ."""
  parser.add_option('-d', '--dry-run', action='store_true',
                    help='trigger in dry run mode')
  parser.add_option(
      '-q',
      '--quick-run',
      action='store_true',
      help='trigger in quick run mode '
          '(https://source.chromium.org/chromium/chromium/src/+/main:docs/cq_qu'
          'ick_run.md) (chromium only).')
  parser.add_option('-c', '--clear', action='store_true',
                    help='stop CQ run, if any')
  parser.add_option(
      '-i', '--issue', type=int,
      help='Operate on this issue instead of the current branch\'s implicit '
      'issue.')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))
  if [options.dry_run, options.quick_run, options.clear].count(True) > 1:
    parser.error('Only one of --dry-run, --quick-run, and --clear are allowed.')

  cl = Changelist(issue=options.issue)
  if not cl.GetIssue():
    parser.error('Must upload the issue first.')

  if options.clear:
    state = _CQState.NONE
  elif options.quick_run:
    state = _CQState.QUICK_RUN
  elif options.dry_run:
    state = _CQState.DRY_RUN
  else:
    state = _CQState.COMMIT
  cl.SetCQState(state)
  return 0


@metrics.collector.collect_metrics('git cl set-close')
def CMDset_close(parser, args):
  """Closes the issue."""
  parser.add_option(
      '-i', '--issue', type=int,
      help='Operate on this issue instead of the current branch\'s implicit '
      'issue.')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))
  cl = Changelist(issue=options.issue)
  # Ensure there actually is an issue to close.
  if not cl.GetIssue():
    DieWithError('ERROR: No issue to close.')
  cl.CloseIssue()
  return 0


@metrics.collector.collect_metrics('git cl diff')
def CMDdiff(parser, args):
  """Shows differences between local tree and last upload."""
  parser.add_option(
      '--stat',
      action='store_true',
      dest='stat',
      help='Generate a diffstat')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Unrecognized args: %s' % ' '.join(args))

  cl = Changelist()
  issue = cl.GetIssue()
  branch = cl.GetBranch()
  if not issue:
    DieWithError('No issue found for current branch (%s)' % branch)

  base = cl._GitGetBranchConfigValue(LAST_UPLOAD_HASH_CONFIG_KEY)
  if not base:
    base = cl._GitGetBranchConfigValue(GERRIT_SQUASH_HASH_CONFIG_KEY)
  if not base:
    detail = cl._GetChangeDetail(['CURRENT_REVISION', 'CURRENT_COMMIT'])
    revision_info = detail['revisions'][detail['current_revision']]
    fetch_info = revision_info['fetch']['http']
    RunGit(['fetch', fetch_info['url'], fetch_info['ref']])
    base = 'FETCH_HEAD'

  cmd = ['git', 'diff']
  if options.stat:
    cmd.append('--stat')
  cmd.append(base)
  subprocess2.check_call(cmd)

  return 0


@metrics.collector.collect_metrics('git cl owners')
def CMDowners(parser, args):
  """Finds potential owners for reviewing."""
  parser.add_option(
      '--ignore-current',
      action='store_true',
      help='Ignore the CL\'s current reviewers and start from scratch.')
  parser.add_option(
      '--ignore-self',
      action='store_true',
      help='Do not consider CL\'s author as an owners.')
  parser.add_option(
      '--no-color',
      action='store_true',
      help='Use this option to disable color output')
  parser.add_option(
      '--batch',
      action='store_true',
      help='Do not run interactively, just suggest some')
  # TODO: Consider moving this to another command, since other
  #       git-cl owners commands deal with owners for a given CL.
  parser.add_option(
      '--show-all',
      action='store_true',
      help='Show all owners for a particular file')
  options, args = parser.parse_args(args)

  cl = Changelist()
  author = cl.GetAuthor()

  if options.show_all:
    if len(args) == 0:
      print('No files specified for --show-all. Nothing to do.')
      return 0
    owners_by_path = cl.owners_client.BatchListOwners(args)
    for path in args:
      print('Owners for %s:' % path)
      print('\n'.join(
          ' - %s' % owner
          for owner in owners_by_path.get(path, ['No owners found'])))
    return 0

  if args:
    if len(args) > 1:
      parser.error('Unknown args.')
    base_branch = args[0]
  else:
    # Default to diffing against the common ancestor of the upstream branch.
    base_branch = cl.GetCommonAncestorWithUpstream()

  affected_files = cl.GetAffectedFiles(base_branch)

  if options.batch:
    owners = cl.owners_client.SuggestOwners(affected_files, exclude=[author])
    print('\n'.join(owners))
    return 0

  return owners_finder.OwnersFinder(
      affected_files,
      author,
      [] if options.ignore_current else cl.GetReviewers(),
      cl.owners_client,
      disable_color=options.no_color,
      ignore_author=options.ignore_self).run()


def BuildGitDiffCmd(diff_type, upstream_commit, args, allow_prefix=False):
  """Generates a diff command."""
  # Generate diff for the current branch's changes.
  diff_cmd = ['-c', 'core.quotePath=false', 'diff', '--no-ext-diff']

  if allow_prefix:
    # explicitly setting --src-prefix and --dst-prefix is necessary in the
    # case that diff.noprefix is set in the user's git config.
    diff_cmd += ['--src-prefix=a/', '--dst-prefix=b/']
  else:
    diff_cmd += ['--no-prefix']

  diff_cmd += [diff_type, upstream_commit, '--']

  if args:
    for arg in args:
      if os.path.isdir(arg) or os.path.isfile(arg):
        diff_cmd.append(arg)
      else:
        DieWithError('Argument "%s" is not a file or a directory' % arg)

  return diff_cmd


def _RunClangFormatDiff(opts, clang_diff_files, top_dir, upstream_commit):
  """Runs clang-format-diff and sets a return value if necessary."""

  if not clang_diff_files:
    return 0

  # Set to 2 to signal to CheckPatchFormatted() that this patch isn't
  # formatted. This is used to block during the presubmit.
  return_value = 0

  # Locate the clang-format binary in the checkout
  try:
    clang_format_tool = clang_format.FindClangFormatToolInChromiumTree()
  except clang_format.NotFoundError as e:
    DieWithError(e)

  if opts.full or settings.GetFormatFullByDefault():
    cmd = [clang_format_tool]
    if not opts.dry_run and not opts.diff:
      cmd.append('-i')
    if opts.dry_run:
      for diff_file in clang_diff_files:
        with open(diff_file, 'r') as myfile:
          code = myfile.read().replace('\r\n', '\n')
          stdout = RunCommand(cmd + [diff_file], cwd=top_dir)
          stdout = stdout.replace('\r\n', '\n')
          if opts.diff:
            sys.stdout.write(stdout)
          if code != stdout:
            return_value = 2
    else:
      stdout = RunCommand(cmd + clang_diff_files, cwd=top_dir)
      if opts.diff:
        sys.stdout.write(stdout)
  else:
    try:
      script = clang_format.FindClangFormatScriptInChromiumTree(
          'clang-format-diff.py')
    except clang_format.NotFoundError as e:
      DieWithError(e)

    cmd = ['vpython3', script, '-p0']
    if not opts.dry_run and not opts.diff:
      cmd.append('-i')

    diff_cmd = BuildGitDiffCmd('-U0', upstream_commit, clang_diff_files)
    diff_output = RunGit(diff_cmd).encode('utf-8')

    env = os.environ.copy()
    env['PATH'] = (
        str(os.path.dirname(clang_format_tool)) + os.pathsep + env['PATH'])
    stdout = RunCommand(
        cmd, stdin=diff_output, cwd=top_dir, env=env,
        shell=sys.platform.startswith('win32'))
    if opts.diff:
      sys.stdout.write(stdout)
    if opts.dry_run and len(stdout) > 0:
      return_value = 2

  return return_value


def _RunRustFmt(opts, rust_diff_files, top_dir, upstream_commit):
  """Runs rustfmt.  Just like _RunClangFormatDiff returns 2 to indicate that
  presubmit checks have failed (and returns 0 otherwise)."""

  if not rust_diff_files:
    return 0

  # Locate the rustfmt binary.
  try:
    rustfmt_tool = rustfmt.FindRustfmtToolInChromiumTree()
  except rustfmt.NotFoundError as e:
    DieWithError(e)

  # TODO(crbug.com/1231317): Support formatting only the changed lines
  # if `opts.full or settings.GetFormatFullByDefault()` is False.  See also:
  # https://github.com/emilio/rustfmt-format-diff
  cmd = [rustfmt_tool]
  if opts.dry_run:
    cmd.append('--check')
  cmd += rust_diff_files
  rustfmt_exitcode = subprocess2.call(cmd)

  if opts.presubmit and rustfmt_exitcode != 0:
    return 2

  return 0


def _RunSwiftFormat(opts, swift_diff_files, top_dir, upstream_commit):
  """Runs swift-format.  Just like _RunClangFormatDiff returns 2 to indicate
  that presubmit checks have failed (and returns 0 otherwise)."""

  if not swift_diff_files:
    return 0

  # Locate the swift-format binary.
  try:
    swift_format_tool = swift_format.FindSwiftFormatToolInChromiumTree()
  except swift_format.NotFoundError as e:
    DieWithError(e)

  cmd = [swift_format_tool]
  if opts.dry_run:
    cmd += ['lint', '-s']
  else:
    cmd += ['format', '-i']
  cmd += swift_diff_files
  swift_format_exitcode = subprocess2.call(cmd)

  if opts.presubmit and swift_format_exitcode != 0:
    return 2

  return 0


def MatchingFileType(file_name, extensions):
  """Returns True if the file name ends with one of the given extensions."""
  return bool([ext for ext in extensions if file_name.lower().endswith(ext)])


@subcommand.usage('[files or directories to diff]')
@metrics.collector.collect_metrics('git cl format')
def CMDformat(parser, args):
  """Runs auto-formatting tools (clang-format etc.) on the diff."""
  CLANG_EXTS = ['.cc', '.cpp', '.h', '.m', '.mm', '.proto', '.java']
  GN_EXTS = ['.gn', '.gni', '.typemap']
  RUST_EXTS = ['.rs']
  SWIFT_EXTS = ['.swift']
  parser.add_option('--full', action='store_true',
                    help='Reformat the full content of all touched files')
  parser.add_option('--upstream', help='Branch to check against')
  parser.add_option('--dry-run', action='store_true',
                    help='Don\'t modify any file on disk.')
  parser.add_option(
      '--no-clang-format',
      dest='clang_format',
      action='store_false',
      default=True,
      help='Disables formatting of various file types using clang-format.')
  parser.add_option(
      '--python',
      action='store_true',
      default=None,
      help='Enables python formatting on all python files.')
  parser.add_option(
      '--no-python',
      action='store_true',
      default=False,
      help='Disables python formatting on all python files. '
      'If neither --python or --no-python are set, python files that have a '
      '.style.yapf file in an ancestor directory will be formatted. '
      'It is an error to set both.')
  parser.add_option(
      '--js',
      action='store_true',
      help='Format javascript code with clang-format. '
      'Has no effect if --no-clang-format is set.')
  parser.add_option('--diff', action='store_true',
                    help='Print diff to stdout rather than modifying files.')
  parser.add_option('--presubmit', action='store_true',
                    help='Used when running the script from a presubmit.')

  parser.add_option('--rust-fmt',
                    dest='use_rust_fmt',
                    action='store_true',
                    default=rustfmt.IsRustfmtSupported(),
                    help='Enables formatting of Rust file types using rustfmt.')
  parser.add_option(
      '--no-rust-fmt',
      dest='use_rust_fmt',
      action='store_false',
      help='Disables formatting of Rust file types using rustfmt.')

  parser.add_option(
      '--swift-format',
      dest='use_swift_format',
      action='store_true',
      default=swift_format.IsSwiftFormatSupported(),
      help='Enables formatting of Swift file types using swift-format '
      '(macOS host only).')
  parser.add_option(
      '--no-swift-format',
      dest='use_swift_format',
      action='store_false',
      help='Disables formatting of Swift file types using swift-format.')

  opts, args = parser.parse_args(args)

  if opts.python is not None and opts.no_python:
    raise parser.error('Cannot set both --python and --no-python')
  if opts.no_python:
    opts.python = False

  # Normalize any remaining args against the current path, so paths relative to
  # the current directory are still resolved as expected.
  args = [os.path.join(os.getcwd(), arg) for arg in args]

  # git diff generates paths against the root of the repository.  Change
  # to that directory so clang-format can find files even within subdirs.
  rel_base_path = settings.GetRelativeRoot()
  if rel_base_path:
    os.chdir(rel_base_path)

  # Grab the merge-base commit, i.e. the upstream commit of the current
  # branch when it was created or the last time it was rebased. This is
  # to cover the case where the user may have called "git fetch origin",
  # moving the origin branch to a newer commit, but hasn't rebased yet.
  upstream_commit = None
  upstream_branch = opts.upstream
  if not upstream_branch:
    cl = Changelist()
    upstream_branch = cl.GetUpstreamBranch()
  if upstream_branch:
    upstream_commit = RunGit(['merge-base', 'HEAD', upstream_branch])
    upstream_commit = upstream_commit.strip()

  if not upstream_commit:
    DieWithError('Could not find base commit for this branch. '
                 'Are you in detached state?')

  changed_files_cmd = BuildGitDiffCmd('--name-only', upstream_commit, args)
  diff_output = RunGit(changed_files_cmd)
  diff_files = diff_output.splitlines()
  # Filter out files deleted by this CL
  diff_files = [x for x in diff_files if os.path.isfile(x)]

  if opts.js:
    CLANG_EXTS.extend(['.js', '.ts'])

  clang_diff_files = []
  if opts.clang_format:
    clang_diff_files = [
        x for x in diff_files if MatchingFileType(x, CLANG_EXTS)
    ]
  python_diff_files = [x for x in diff_files if MatchingFileType(x, ['.py'])]
  rust_diff_files = [x for x in diff_files if MatchingFileType(x, RUST_EXTS)]
  swift_diff_files = [x for x in diff_files if MatchingFileType(x, SWIFT_EXTS)]
  gn_diff_files = [x for x in diff_files if MatchingFileType(x, GN_EXTS)]

  top_dir = settings.GetRoot()

  return_value = _RunClangFormatDiff(opts, clang_diff_files, top_dir,
                                     upstream_commit)

  if opts.use_rust_fmt:
    rust_fmt_return_value = _RunRustFmt(opts, rust_diff_files, top_dir,
                                        upstream_commit)
    if rust_fmt_return_value == 2:
      return_value = 2

  if opts.use_swift_format:
    if sys.platform != 'darwin':
      DieWithError('swift-format is only supported on macOS.')
    swift_format_return_value = _RunSwiftFormat(opts, swift_diff_files, top_dir,
                                                upstream_commit)
    if swift_format_return_value == 2:
      return_value = 2

  # Similar code to above, but using yapf on .py files rather than clang-format
  # on C/C++ files
  py_explicitly_disabled = opts.python is not None and not opts.python
  if python_diff_files and not py_explicitly_disabled:
    depot_tools_path = os.path.dirname(os.path.abspath(__file__))
    yapf_tool = os.path.join(depot_tools_path, 'yapf')

    # Used for caching.
    yapf_configs = {}
    for f in python_diff_files:
      # Find the yapf style config for the current file, defaults to depot
      # tools default.
      _FindYapfConfigFile(f, yapf_configs, top_dir)

    # Turn on python formatting by default if a yapf config is specified.
    # This breaks in the case of this repo though since the specified
    # style file is also the global default.
    if opts.python is None:
      filtered_py_files = []
      for f in python_diff_files:
        if _FindYapfConfigFile(f, yapf_configs, top_dir) is not None:
          filtered_py_files.append(f)
    else:
      filtered_py_files = python_diff_files

    # Note: yapf still seems to fix indentation of the entire file
    # even if line ranges are specified.
    # See https://github.com/google/yapf/issues/499
    if not opts.full and filtered_py_files:
      py_line_diffs = _ComputeDiffLineRanges(filtered_py_files, upstream_commit)

    yapfignore_patterns = _GetYapfIgnorePatterns(top_dir)
    filtered_py_files = _FilterYapfIgnoredFiles(filtered_py_files,
                                                yapfignore_patterns)

    for f in filtered_py_files:
      yapf_style = _FindYapfConfigFile(f, yapf_configs, top_dir)
      # Default to pep8 if not .style.yapf is found.
      if not yapf_style:
        yapf_style = 'pep8'

      with open(f, 'r') as py_f:
        if 'python2' in py_f.readline():
          vpython_script = 'vpython'
        else:
          vpython_script = 'vpython3'

      cmd = [vpython_script, yapf_tool, '--style', yapf_style, f]

      has_formattable_lines = False
      if not opts.full:
        # Only run yapf over changed line ranges.
        for diff_start, diff_len in py_line_diffs[f]:
          diff_end = diff_start + diff_len - 1
          # Yapf errors out if diff_end < diff_start but this
          # is a valid line range diff for a removal.
          if diff_end >= diff_start:
            has_formattable_lines = True
            cmd += ['-l', '{}-{}'.format(diff_start, diff_end)]
        # If all line diffs were removals we have nothing to format.
        if not has_formattable_lines:
          continue

      if opts.diff or opts.dry_run:
        cmd += ['--diff']
        # Will return non-zero exit code if non-empty diff.
        stdout = RunCommand(cmd,
                            error_ok=True,
                            stderr=subprocess2.PIPE,
                            cwd=top_dir,
                            shell=sys.platform.startswith('win32'))
        if opts.diff:
          sys.stdout.write(stdout)
        elif len(stdout) > 0:
          return_value = 2
      else:
        cmd += ['-i']
        RunCommand(cmd, cwd=top_dir, shell=sys.platform.startswith('win32'))

  # Format GN build files. Always run on full build files for canonical form.
  if gn_diff_files:
    cmd = ['gn', 'format']
    if opts.dry_run or opts.diff:
      cmd.append('--dry-run')
    for gn_diff_file in gn_diff_files:
      gn_ret = subprocess2.call(cmd + [gn_diff_file],
                                shell=sys.platform.startswith('win'),
                                cwd=top_dir)
      if opts.dry_run and gn_ret == 2:
        return_value = 2  # Not formatted.
      elif opts.diff and gn_ret == 2:
        # TODO this should compute and print the actual diff.
        print('This change has GN build file diff for ' + gn_diff_file)
      elif gn_ret != 0:
        # For non-dry run cases (and non-2 return values for dry-run), a
        # nonzero error code indicates a failure, probably because the file
        # doesn't parse.
        DieWithError('gn format failed on ' + gn_diff_file +
                     '\nTry running `gn format` on this file manually.')

  # Skip the metrics formatting from the global presubmit hook. These files have
  # a separate presubmit hook that issues an error if the files need formatting,
  # whereas the top-level presubmit script merely issues a warning. Formatting
  # these files is somewhat slow, so it's important not to duplicate the work.
  if not opts.presubmit:
    for diff_xml in GetDiffXMLs(diff_files):
      xml_dir = GetMetricsDir(diff_xml)
      if not xml_dir:
        continue

      tool_dir = os.path.join(top_dir, xml_dir)
      pretty_print_tool = os.path.join(tool_dir, 'pretty_print.py')
      cmd = ['vpython3', pretty_print_tool, '--non-interactive']

      # If the XML file is histograms.xml or enums.xml, add the xml path to the
      # command as histograms/pretty_print.py now needs a relative path argument
      # after splitting the histograms into multiple directories.
      # For example, in tools/metrics/ukm, pretty-print could be run using:
      #   $ python pretty_print.py
      # But in tools/metrics/histogrmas, pretty-print should be run with an
      # additional relative path argument, like:
      #   $ python pretty_print.py metadata/UMA/histograms.xml
      #   $ python pretty_print.py enums.xml

      if (diff_xml.endswith('histograms.xml') or diff_xml.endswith('enums.xml')
          or diff_xml.endswith('histogram_suffixes_list.xml')):
        cmd.append(diff_xml)

      if opts.dry_run or opts.diff:
        cmd.append('--diff')

      # TODO(isherman): Once this file runs only on Python 3.3+, drop the
      # `shell` param and instead replace `'vpython'` with
      # `shutil.which('frob')` above: https://stackoverflow.com/a/32799942
      stdout = RunCommand(cmd,
                          cwd=top_dir,
                          shell=sys.platform.startswith('win32'))
      if opts.diff:
        sys.stdout.write(stdout)
      if opts.dry_run and stdout:
        return_value = 2  # Not formatted.

  return return_value


def GetDiffXMLs(diff_files):
  return [
      os.path.normpath(x) for x in diff_files if MatchingFileType(x, ['.xml'])
  ]


def GetMetricsDir(diff_xml):
  metrics_xml_dirs = [
    os.path.join('tools', 'metrics', 'actions'),
    os.path.join('tools', 'metrics', 'histograms'),
    os.path.join('tools', 'metrics', 'rappor'),
    os.path.join('tools', 'metrics', 'structured'),
    os.path.join('tools', 'metrics', 'ukm'),
  ]
  for xml_dir in metrics_xml_dirs:
    if diff_xml.startswith(xml_dir):
      return xml_dir
  return None


@subcommand.usage('<codereview url or issue id>')
@metrics.collector.collect_metrics('git cl checkout')
def CMDcheckout(parser, args):
  """Checks out a branch associated with a given Gerrit issue."""
  _, args = parser.parse_args(args)

  if len(args) != 1:
    parser.print_help()
    return 1

  issue_arg = ParseIssueNumberArgument(args[0])
  if not issue_arg.valid:
    parser.error('Invalid issue ID or URL.')

  target_issue = str(issue_arg.issue)

  output = RunGit(['config', '--local', '--get-regexp',
                   r'branch\..*\.' + ISSUE_CONFIG_KEY],
                   error_ok=True)

  branches = []
  for key, issue in [x.split() for x in output.splitlines()]:
    if issue == target_issue:
      branches.append(re.sub(r'branch\.(.*)\.' + ISSUE_CONFIG_KEY, r'\1', key))

  if len(branches) == 0:
    print('No branch found for issue %s.' % target_issue)
    return 1
  if len(branches) == 1:
    RunGit(['checkout', branches[0]])
  else:
    print('Multiple branches match issue %s:' % target_issue)
    for i in range(len(branches)):
      print('%d: %s' % (i, branches[i]))
    which = gclient_utils.AskForData('Choose by index: ')
    try:
      RunGit(['checkout', branches[int(which)]])
    except (IndexError, ValueError):
      print('Invalid selection, not checking out any branch.')
      return 1

  return 0


def CMDlol(parser, args):
  # This command is intentionally undocumented.
  print(zlib.decompress(base64.b64decode(
      'eNptkLEOwyAMRHe+wupCIqW57v0Vq84WqWtXyrcXnCBsmgMJ+/SSAxMZgRB6NzE'
      'E2ObgCKJooYdu4uAQVffUEoE1sRQLxAcqzd7uK2gmStrll1ucV3uZyaY5sXyDd9'
      'JAnN+lAXsOMJ90GANAi43mq5/VeeacylKVgi8o6F1SC63FxnagHfJUTfUYdCR/W'
      'Ofe+0dHL7PicpytKP750Fh1q2qnLVof4w8OZWNY')).decode('utf-8'))
  return 0


def CMDversion(parser, args):
  import utils
  print(utils.depot_tools_version())


class OptionParser(optparse.OptionParser):
  """Creates the option parse and add --verbose support."""

  def __init__(self, *args, **kwargs):
    optparse.OptionParser.__init__(
        self, *args, prog='git cl', version=__version__, **kwargs)
    self.add_option(
        '-v', '--verbose', action='count', default=0,
        help='Use 2 times for more debugging info')

  def parse_args(self, args=None, _values=None):
    try:
      return self._parse_args(args)
    finally:
      # Regardless of success or failure of args parsing, we want to report
      # metrics, but only after logging has been initialized (if parsing
      # succeeded).
      global settings
      settings = Settings()

      if metrics.collector.config.should_collect_metrics:
        try:
          # GetViewVCUrl ultimately calls logging method.
          project_url = settings.GetViewVCUrl().strip('/+')
          if project_url in metrics_utils.KNOWN_PROJECT_URLS:
            metrics.collector.add('project_urls', [project_url])
        except subprocess2.CalledProcessError:
          # Occurs when command is not executed in a git repository
          # We should not fail here. If the command needs to be executed
          # in a repo, it will be raised later.
          pass

  def _parse_args(self, args=None):
    # Create an optparse.Values object that will store only the actual passed
    # options, without the defaults.
    actual_options = optparse.Values()
    _, args = optparse.OptionParser.parse_args(self, args, actual_options)
    # Create an optparse.Values object with the default options.
    options = optparse.Values(self.get_default_values().__dict__)
    # Update it with the options passed by the user.
    options._update_careful(actual_options.__dict__)
    # Store the options passed by the user in an _actual_options attribute.
    # We store only the keys, and not the values, since the values can contain
    # arbitrary information, which might be PII.
    metrics.collector.add('arguments', list(actual_options.__dict__.keys()))

    levels = [logging.WARNING, logging.INFO, logging.DEBUG]
    logging.basicConfig(
        level=levels[min(options.verbose, len(levels) - 1)],
        format='[%(levelname).1s%(asctime)s %(process)d %(thread)d '
               '%(filename)s] %(message)s')

    return options, args


def main(argv):
  if sys.hexversion < 0x02060000:
    print('\nYour Python version %s is unsupported, please upgrade.\n' %
          (sys.version.split(' ', 1)[0],), file=sys.stderr)
    return 2

  colorize_CMDstatus_doc()
  dispatcher = subcommand.CommandDispatcher(__name__)
  try:
    return dispatcher.execute(OptionParser(), argv)
  except auth.LoginRequiredError as e:
    DieWithError(str(e))
  except urllib.error.HTTPError as e:
    if e.code != 500:
      raise
    DieWithError(
        ('App Engine is misbehaving and returned HTTP %d, again. Keep faith '
         'and retry or visit go/isgaeup.\n%s') % (e.code, str(e)))
  return 0


if __name__ == '__main__':
  # These affect sys.stdout, so do it outside of main() to simplify mocks in
  # the unit tests.
  fix_encoding.fix_encoding()
  setup_color.init()
  with metrics.collector.print_notice_and_exit():
    sys.exit(main(sys.argv[1:]))
