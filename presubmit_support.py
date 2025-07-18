#!/usr/bin/env python3
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enables directory-specific presubmit checks to run at upload and/or commit.
"""

from __future__ import print_function

__version__ = '2.0.0'

# TODO(joi) Add caching where appropriate/needed. The API is designed to allow
# caching (between all different invocations of presubmit scripts for a given
# change). We should add it as our presubmit scripts start feeling slow.

import argparse
import ast  # Exposed through the API.
import contextlib
import cpplint
import fnmatch  # Exposed through the API.
import glob
import inspect
import itertools
import json  # Exposed through the API.
import logging
import multiprocessing
import os  # Somewhat exposed through the API.
import random
import re  # Exposed through the API.
import signal
import six
import sys  # Parts exposed through API.
import tempfile  # Exposed through the API.
import threading
import time
import traceback
import unittest  # Exposed through the API.
from warnings import warn

# Local imports.
import fix_encoding
import gclient_paths  # Exposed through the API
import gclient_utils
import git_footers
import gerrit_util
import owners_client
import owners_finder
import presubmit_canned_checks
import rdb_wrapper
import scm
import subprocess2 as subprocess  # Exposed through the API.

if sys.version_info.major == 2:
  # TODO(1009814): Expose urllib2 only through urllib_request and urllib_error
  import urllib2  # Exposed through the API.
  import urlparse
  import urllib2 as urllib_request
  import urllib2 as urllib_error
else:
  import urllib.parse as urlparse
  import urllib.request as urllib_request
  import urllib.error as urllib_error

# Ask for feedback only once in program lifetime.
_ASKED_FOR_FEEDBACK = False

# Set if super-verbose mode is requested, for tracking where presubmit messages
# are coming from.
_SHOW_CALLSTACKS = False


def time_time():
  # Use this so that it can be mocked in tests without interfering with python
  # system machinery.
  return time.time()


class PresubmitFailure(Exception):
  pass


class CommandData(object):
  def __init__(self, name, cmd, kwargs, message, python3=False):
    self.name = name
    self.cmd = cmd
    self.stdin = kwargs.get('stdin', None)
    self.kwargs = kwargs.copy()
    self.kwargs['stdout'] = subprocess.PIPE
    self.kwargs['stderr'] = subprocess.STDOUT
    self.kwargs['stdin'] = subprocess.PIPE
    self.message = message
    self.info = None
    self.python3 = python3


# Adapted from
# https://github.com/google/gtest-parallel/blob/master/gtest_parallel.py#L37
#
# An object that catches SIGINT sent to the Python process and notices
# if processes passed to wait() die by SIGINT (we need to look for
# both of those cases, because pressing Ctrl+C can result in either
# the main process or one of the subprocesses getting the signal).
#
# Before a SIGINT is seen, wait(p) will simply call p.wait() and
# return the result. Once a SIGINT has been seen (in the main process
# or a subprocess, including the one the current call is waiting for),
# wait(p) will call p.terminate().
class SigintHandler(object):
  sigint_returncodes = {-signal.SIGINT,  # Unix
                        -1073741510,     # Windows
                        }
  def __init__(self):
    self.__lock = threading.Lock()
    self.__processes = set()
    self.__got_sigint = False
    self.__previous_signal = signal.signal(signal.SIGINT, self.interrupt)

  def __on_sigint(self):
    self.__got_sigint = True
    while self.__processes:
      try:
        self.__processes.pop().terminate()
      except OSError:
        pass

  def interrupt(self, signal_num, frame):
    with self.__lock:
      self.__on_sigint()
    self.__previous_signal(signal_num, frame)

  def got_sigint(self):
    with self.__lock:
      return self.__got_sigint

  def wait(self, p, stdin):
    with self.__lock:
      if self.__got_sigint:
        p.terminate()
      self.__processes.add(p)
    stdout, stderr = p.communicate(stdin)
    code = p.returncode
    with self.__lock:
      self.__processes.discard(p)
      if code in self.sigint_returncodes:
        self.__on_sigint()
    return stdout, stderr

sigint_handler = SigintHandler()


class Timer(object):
  def __init__(self, timeout, fn):
    self.completed = False
    self._fn = fn
    self._timer = threading.Timer(timeout, self._onTimer) if timeout else None

  def __enter__(self):
    if self._timer:
      self._timer.start()
    return self

  def __exit__(self, _type, _value, _traceback):
    if self._timer:
      self._timer.cancel()

  def _onTimer(self):
    self._fn()
    self.completed = True


class ThreadPool(object):
  def __init__(self, pool_size=None, timeout=None):
    self.timeout = timeout
    self._pool_size = pool_size or multiprocessing.cpu_count()
    if sys.platform == 'win32':
      # TODO(crbug.com/1190269) - we can't use more than 56 child processes on
      # Windows or Python3 may hang.
      self._pool_size = min(self._pool_size, 56)
    self._messages = []
    self._messages_lock = threading.Lock()
    self._tests = []
    self._tests_lock = threading.Lock()
    self._nonparallel_tests = []

  def _GetCommand(self, test):
    vpython = 'vpython'
    if test.python3:
      vpython += '3'
    if sys.platform == 'win32':
      vpython += '.bat'

    cmd = test.cmd
    if cmd[0] == 'python':
      cmd = list(cmd)
      cmd[0] = vpython
    elif cmd[0].endswith('.py'):
      cmd = [vpython] + cmd

    # On Windows, scripts on the current directory take precedence over PATH, so
    # that when testing depot_tools on Windows, calling `vpython.bat` will
    # execute the copy of vpython of the depot_tools under test instead of the
    # one in the bot.
    # As a workaround, we run the tests from the parent directory instead.
    if (cmd[0] == vpython and
        'cwd' in test.kwargs and
        os.path.basename(test.kwargs['cwd']) == 'depot_tools'):
      test.kwargs['cwd'] = os.path.dirname(test.kwargs['cwd'])
      cmd[1] = os.path.join('depot_tools', cmd[1])

    return cmd

  def _RunWithTimeout(self, cmd, stdin, kwargs):
    p = subprocess.Popen(cmd, **kwargs)
    with Timer(self.timeout, p.terminate) as timer:
      stdout, _ = sigint_handler.wait(p, stdin)
      stdout = stdout.decode('utf-8', 'ignore')
      if timer.completed:
        stdout = 'Process timed out after %ss\n%s' % (self.timeout, stdout)
      return p.returncode, stdout

  def CallCommand(self, test):
    """Runs an external program.

    This function converts invocation of .py files and invocations of 'python'
    to vpython invocations.
    """
    cmd = self._GetCommand(test)
    try:
      start = time_time()
      returncode, stdout = self._RunWithTimeout(cmd, test.stdin, test.kwargs)
      duration = time_time() - start
    except Exception:
      duration = time_time() - start
      return test.message(
          '%s\n%s exec failure (%4.2fs)\n%s' % (
              test.name, ' '.join(cmd), duration, traceback.format_exc()))

    if returncode != 0:
      return test.message(
          '%s\n%s (%4.2fs) failed\n%s' % (
              test.name, ' '.join(cmd), duration, stdout))

    if test.info:
      return test.info('%s\n%s (%4.2fs)' % (test.name, ' '.join(cmd), duration))

  def AddTests(self, tests, parallel=True):
    if parallel:
      self._tests.extend(tests)
    else:
      self._nonparallel_tests.extend(tests)

  def RunAsync(self):
    self._messages = []

    def _WorkerFn():
      while True:
        test = None
        with self._tests_lock:
          if not self._tests:
            break
          test = self._tests.pop()
        result = self.CallCommand(test)
        if result:
          with self._messages_lock:
            self._messages.append(result)

    def _StartDaemon():
      t = threading.Thread(target=_WorkerFn)
      t.daemon = True
      t.start()
      return t

    while self._nonparallel_tests:
      test = self._nonparallel_tests.pop()
      result = self.CallCommand(test)
      if result:
        self._messages.append(result)

    if self._tests:
      threads = [_StartDaemon() for _ in range(self._pool_size)]
      for worker in threads:
        worker.join()

    return self._messages


def normpath(path):
  '''Version of os.path.normpath that also changes backward slashes to
  forward slashes when not running on Windows.
  '''
  # This is safe to always do because the Windows version of os.path.normpath
  # will replace forward slashes with backward slashes.
  path = path.replace(os.sep, '/')
  return os.path.normpath(path)


def _RightHandSideLinesImpl(affected_files):
  """Implements RightHandSideLines for InputApi and GclChange."""
  for af in affected_files:
    lines = af.ChangedContents()
    for line in lines:
      yield (af, line[0], line[1])


def prompt_should_continue(prompt_string):
  sys.stdout.write(prompt_string)
  sys.stdout.flush()
  response = sys.stdin.readline().strip().lower()
  return response in ('y', 'yes')


def _ShouldRunPresubmit(script_text, use_python3):
  """Try to figure out whether these presubmit checks should be run under
    python2 or python3. We need to do this without actually trying to
    compile the text, since the text might compile in one but not the
    other.

    Args:
      script_text: The text of the presubmit script.
      use_python3: if true, will use python3 instead of python2 by default
                if USE_PYTHON3 is not specified.

    Return:
      A boolean if presubmit should be executed
  """
  if os.getenv('LUCI_OMIT_PYTHON2') == 'true':
    # If LUCI omits python2, run all presubmits with python3, regardless of
    # USE_PYTHON3 variable.
    return True

  m = re.search('^USE_PYTHON3 = (True|False)$', script_text, flags=re.MULTILINE)
  if m:
    use_python3 = m.group(1) == 'True'

  return ((sys.version_info.major == 2) and not use_python3) or \
      ((sys.version_info.major == 3) and use_python3)

# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _PresubmitResult(object):
  """Base class for result objects."""
  fatal = False
  should_prompt = False

  def __init__(self, message, items=None, long_text=''):
    """
    message: A short one-line message to indicate errors.
    items: A list of short strings to indicate where errors occurred.
    long_text: multi-line text output, e.g. from another tool
    """
    self._message = _PresubmitResult._ensure_str(message)
    self._items = items or []
    self._long_text = _PresubmitResult._ensure_str(long_text.rstrip())
    if _SHOW_CALLSTACKS:
      self._long_text += 'Presubmit result call stack is:\n'
      self._long_text += ''.join(traceback.format_stack(None, 8))

  @staticmethod
  def _ensure_str(val):
    """
    val: A "stringish" value. Can be any of str, unicode or bytes.
    returns: A str after applying encoding/decoding as needed.
    Assumes/uses UTF-8 for relevant inputs/outputs.

    We'd prefer to use six.ensure_str but our copy of six is old :(
    """
    if isinstance(val, str):
      return val

    if six.PY2 and isinstance(val, unicode):
      return val.encode()

    if six.PY3 and isinstance(val, bytes):
      return val.decode()
    raise ValueError("Unknown string type %s" % type(val))

  def handle(self):
    sys.stdout.write(self._message)
    sys.stdout.write('\n')
    for item in self._items:
      sys.stdout.write('  ')
      # Write separately in case it's unicode.
      sys.stdout.write(str(item))
      sys.stdout.write('\n')
    if self._long_text:
      sys.stdout.write('\n***************\n')
      # Write separately in case it's unicode.
      sys.stdout.write(self._long_text)
      sys.stdout.write('\n***************\n')

  def json_format(self):
    return {
        'message': self._message,
        'items': [str(item) for item in self._items],
        'long_text': self._long_text,
        'fatal': self.fatal
    }


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _PresubmitError(_PresubmitResult):
  """A hard presubmit error."""
  fatal = True


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _PresubmitPromptWarning(_PresubmitResult):
  """An warning that prompts the user if they want to continue."""
  should_prompt = True


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _PresubmitNotifyResult(_PresubmitResult):
  """Just print something to the screen -- but it's not even a warning."""


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _MailTextResult(_PresubmitResult):
  """A warning that should be included in the review request email."""
  def __init__(self, *args, **kwargs):
    super(_MailTextResult, self).__init__()
    raise NotImplementedError()

class GerritAccessor(object):
  """Limited Gerrit functionality for canned presubmit checks to work.

  To avoid excessive Gerrit calls, caches the results.
  """

  def __init__(self, url=None, project=None, branch=None):
    self.host = urlparse.urlparse(url).netloc if url else None
    self.project = project
    self.branch = branch
    self.cache = {}
    self.code_owners_enabled = None

  def _FetchChangeDetail(self, issue):
    # Separate function to be easily mocked in tests.
    try:
      return gerrit_util.GetChangeDetail(
          self.host, str(issue),
          ['ALL_REVISIONS', 'DETAILED_LABELS', 'ALL_COMMITS'])
    except gerrit_util.GerritError as e:
      if e.http_status == 404:
        raise Exception('Either Gerrit issue %s doesn\'t exist, or '
                        'no credentials to fetch issue details' % issue)
      raise

  def GetChangeInfo(self, issue):
    """Returns labels and all revisions (patchsets) for this issue.

    The result is a dictionary according to Gerrit REST Api.
    https://gerrit-review.googlesource.com/Documentation/rest-api.html

    However, API isn't very clear what's inside, so see tests for example.
    """
    assert issue
    cache_key = int(issue)
    if cache_key not in self.cache:
      self.cache[cache_key] = self._FetchChangeDetail(issue)
    return self.cache[cache_key]

  def GetChangeDescription(self, issue, patchset=None):
    """If patchset is none, fetches current patchset."""
    info = self.GetChangeInfo(issue)
    # info is a reference to cache. We'll modify it here adding description to
    # it to the right patchset, if it is not yet there.

    # Find revision info for the patchset we want.
    if patchset is not None:
      for rev, rev_info in info['revisions'].items():
        if str(rev_info['_number']) == str(patchset):
          break
      else:
        raise Exception('patchset %s doesn\'t exist in issue %s' % (
            patchset, issue))
    else:
      rev = info['current_revision']
      rev_info = info['revisions'][rev]

    return rev_info['commit']['message']

  def GetDestRef(self, issue):
    ref = self.GetChangeInfo(issue)['branch']
    if not ref.startswith('refs/'):
      # NOTE: it is possible to create 'refs/x' branch,
      # aka 'refs/heads/refs/x'. However, this is ill-advised.
      ref = 'refs/heads/%s' % ref
    return ref

  def _GetApproversForLabel(self, issue, label):
    change_info = self.GetChangeInfo(issue)
    label_info = change_info.get('labels', {}).get(label, {})
    values = label_info.get('values', {}).keys()
    if not values:
      return []
    max_value = max(int(v) for v in values)
    return [v for v in label_info.get('all', [])
            if v.get('value', 0) == max_value]

  def IsBotCommitApproved(self, issue):
    return bool(self._GetApproversForLabel(issue, 'Bot-Commit'))

  def IsOwnersOverrideApproved(self, issue):
    return bool(self._GetApproversForLabel(issue, 'Owners-Override'))

  def GetChangeOwner(self, issue):
    return self.GetChangeInfo(issue)['owner']['email']

  def GetChangeReviewers(self, issue, approving_only=True):
    changeinfo = self.GetChangeInfo(issue)
    if approving_only:
      reviewers = self._GetApproversForLabel(issue, 'Code-Review')
    else:
      reviewers = changeinfo.get('reviewers', {}).get('REVIEWER', [])
    return [r.get('email') for r in reviewers]

  def UpdateDescription(self, description, issue):
    gerrit_util.SetCommitMessage(self.host, issue, description, notify='NONE')

  def IsCodeOwnersEnabledOnRepo(self):
    if self.code_owners_enabled is None:
      self.code_owners_enabled = gerrit_util.IsCodeOwnersEnabledOnRepo(
          self.host, self.project)
    return self.code_owners_enabled


class OutputApi(object):
  """An instance of OutputApi gets passed to presubmit scripts so that they
  can output various types of results.
  """
  PresubmitResult = _PresubmitResult
  PresubmitError = _PresubmitError
  PresubmitPromptWarning = _PresubmitPromptWarning
  PresubmitNotifyResult = _PresubmitNotifyResult
  MailTextResult = _MailTextResult

  def __init__(self, is_committing):
    self.is_committing = is_committing
    self.more_cc = []

  def AppendCC(self, cc):
    """Appends a user to cc for this change."""
    if cc not in self.more_cc:
      self.more_cc.append(cc)

  def PresubmitPromptOrNotify(self, *args, **kwargs):
    """Warn the user when uploading, but only notify if committing."""
    if self.is_committing:
      return self.PresubmitNotifyResult(*args, **kwargs)
    return self.PresubmitPromptWarning(*args, **kwargs)


class InputApi(object):
  """An instance of this object is passed to presubmit scripts so they can
  know stuff about the change they're looking at.
  """
  # Method could be a function
  # pylint: disable=no-self-use

  # File extensions that are considered source files from a style guide
  # perspective. Don't modify this list from a presubmit script!
  #
  # Files without an extension aren't included in the list. If you want to
  # filter them as source files, add r'(^|.*?[\\\/])[^.]+$' to the allow list.
  # Note that ALL CAPS files are skipped in DEFAULT_FILES_TO_SKIP below.
  DEFAULT_FILES_TO_CHECK = (
      # C++ and friends
      r'.+\.c$', r'.+\.cc$', r'.+\.cpp$', r'.+\.h$', r'.+\.m$', r'.+\.mm$',
      r'.+\.inl$', r'.+\.asm$', r'.+\.hxx$', r'.+\.hpp$', r'.+\.s$', r'.+\.S$',
      # Scripts
      r'.+\.js$', r'.+\.ts$', r'.+\.py$', r'.+\.sh$', r'.+\.rb$', r'.+\.pl$',
      r'.+\.pm$',
      # Other
      r'.+\.java$', r'.+\.mk$', r'.+\.am$', r'.+\.css$', r'.+\.mojom$',
      r'.+\.fidl$', r'.+\.rs$',
  )

  # Path regexp that should be excluded from being considered containing source
  # files. Don't modify this list from a presubmit script!
  DEFAULT_FILES_TO_SKIP = (
      r'testing_support[\\\/]google_appengine[\\\/].*',
      r'.*\bexperimental[\\\/].*',
      # Exclude third_party/.* but NOT third_party/{WebKit,blink}
      # (crbug.com/539768 and crbug.com/836555).
      r'.*\bthird_party[\\\/](?!(WebKit|blink)[\\\/]).*',
      # Output directories (just in case)
      r'.*\bDebug[\\\/].*',
      r'.*\bRelease[\\\/].*',
      r'.*\bxcodebuild[\\\/].*',
      r'.*\bout[\\\/].*',
      # All caps files like README and LICENCE.
      r'.*\b[A-Z0-9_]{2,}$',
      # SCM (can happen in dual SCM configuration). (Slightly over aggressive)
      r'(|.*[\\\/])\.git[\\\/].*',
      r'(|.*[\\\/])\.svn[\\\/].*',
      # There is no point in processing a patch file.
      r'.+\.diff$',
      r'.+\.patch$',
  )

  def __init__(self, change, presubmit_path, is_committing,
      verbose, gerrit_obj, dry_run=None, thread_pool=None, parallel=False,
      no_diffs=False):
    """Builds an InputApi object.

    Args:
      change: A presubmit.Change object.
      presubmit_path: The path to the presubmit script being processed.
      is_committing: True if the change is about to be committed.
      gerrit_obj: provides basic Gerrit codereview functionality.
      dry_run: if true, some Checks will be skipped.
      parallel: if true, all tests reported via input_api.RunTests for all
                PRESUBMIT files will be run in parallel.
      no_diffs: if true, implies that --files or --all was specified so some
                checks can be skipped, and some errors will be messages.
    """
    # Version number of the presubmit_support script.
    self.version = [int(x) for x in __version__.split('.')]
    self.change = change
    self.is_committing = is_committing
    self.gerrit = gerrit_obj
    self.dry_run = dry_run
    self.no_diffs = no_diffs

    self.parallel = parallel
    self.thread_pool = thread_pool or ThreadPool()

    # We expose various modules and functions as attributes of the input_api
    # so that presubmit scripts don't have to import them.
    self.ast = ast
    self.basename = os.path.basename
    self.cpplint = cpplint
    self.fnmatch = fnmatch
    self.gclient_paths = gclient_paths
    # TODO(yyanagisawa): stop exposing this when python3 become default.
    # Since python3's tempfile has TemporaryDirectory, we do not need this.
    self.temporary_directory = gclient_utils.temporary_directory
    self.glob = glob.glob
    self.json = json
    self.logging = logging.getLogger('PRESUBMIT')
    self.os_listdir = os.listdir
    self.os_path = os.path
    self.os_stat = os.stat
    self.os_walk = os.walk
    self.re = re
    self.subprocess = subprocess
    self.sys = sys
    self.tempfile = tempfile
    self.time = time
    self.unittest = unittest
    if sys.version_info.major == 2:
      self.urllib2 = urllib2
    self.urllib_request = urllib_request
    self.urllib_error = urllib_error

    self.is_windows = sys.platform == 'win32'

    # Set python_executable to 'vpython' in order to allow scripts in other
    # repos (e.g. src.git) to automatically pick up that repo's .vpython file,
    # instead of inheriting the one in depot_tools.
    self.python_executable = 'vpython'
    # Offer a python 3 executable for use during the migration off of python 2.
    self.python3_executable = 'vpython3'
    self.environ = os.environ

    # InputApi.platform is the platform you're currently running on.
    self.platform = sys.platform

    self.cpu_count = multiprocessing.cpu_count()
    if self.is_windows:
      # TODO(crbug.com/1190269) - we can't use more than 56 child processes on
      # Windows or Python3 may hang.
      self.cpu_count = min(self.cpu_count, 56)

    # The local path of the currently-being-processed presubmit script.
    self._current_presubmit_path = os.path.dirname(presubmit_path)

    # We carry the canned checks so presubmit scripts can easily use them.
    self.canned_checks = presubmit_canned_checks

    # Temporary files we must manually remove at the end of a run.
    self._named_temporary_files = []

    self.owners_client = None
    if self.gerrit and not 'PRESUBMIT_SKIP_NETWORK' in self.environ:
      try:
        self.owners_client = owners_client.GetCodeOwnersClient(
            host=self.gerrit.host,
            project=self.gerrit.project,
            branch=self.gerrit.branch)
      except Exception as e:
        print('Failed to set owners_client - %s' % str(e))
    self.owners_finder = owners_finder.OwnersFinder
    self.verbose = verbose
    self.Command = CommandData

    # Replace <hash_map> and <hash_set> as headers that need to be included
    # with 'base/containers/hash_tables.h' instead.
    # Access to a protected member _XX of a client class
    # pylint: disable=protected-access
    self.cpplint._re_pattern_templates = [
      (a, b, 'base/containers/hash_tables.h')
        if header in ('<hash_map>', '<hash_set>') else (a, b, header)
      for (a, b, header) in cpplint._re_pattern_templates
    ]

  def SetTimeout(self, timeout):
    self.thread_pool.timeout = timeout

  def PresubmitLocalPath(self):
    """Returns the local path of the presubmit script currently being run.

    This is useful if you don't want to hard-code absolute paths in the
    presubmit script.  For example, It can be used to find another file
    relative to the PRESUBMIT.py script, so the whole tree can be branched and
    the presubmit script still works, without editing its content.
    """
    return self._current_presubmit_path

  def AffectedFiles(self, include_deletes=True, file_filter=None):
    """Same as input_api.change.AffectedFiles() except only lists files
    (and optionally directories) in the same directory as the current presubmit
    script, or subdirectories thereof. Note that files are listed using the OS
    path separator, so backslashes are used as separators on Windows.
    """
    dir_with_slash = normpath(self.PresubmitLocalPath())
    # normpath strips trailing path separators, so the trailing separator has to
    # be added after the normpath call.
    if len(dir_with_slash) > 0:
      dir_with_slash += os.path.sep

    return list(filter(
        lambda x: normpath(x.AbsoluteLocalPath()).startswith(dir_with_slash),
        self.change.AffectedFiles(include_deletes, file_filter)))

  def LocalPaths(self):
    """Returns local paths of input_api.AffectedFiles()."""
    paths = [af.LocalPath() for af in self.AffectedFiles()]
    logging.debug('LocalPaths: %s', paths)
    return paths

  def AbsoluteLocalPaths(self):
    """Returns absolute local paths of input_api.AffectedFiles()."""
    return [af.AbsoluteLocalPath() for af in self.AffectedFiles()]

  def AffectedTestableFiles(self, include_deletes=None, **kwargs):
    """Same as input_api.change.AffectedTestableFiles() except only lists files
    in the same directory as the current presubmit script, or subdirectories
    thereof.
    """
    if include_deletes is not None:
      warn('AffectedTestableFiles(include_deletes=%s)'
               ' is deprecated and ignored' % str(include_deletes),
           category=DeprecationWarning,
           stacklevel=2)
    # pylint: disable=consider-using-generator
    return [
        x for x in self.AffectedFiles(include_deletes=False, **kwargs)
        if x.IsTestableFile()
    ]

  def AffectedTextFiles(self, include_deletes=None):
    """An alias to AffectedTestableFiles for backwards compatibility."""
    return self.AffectedTestableFiles(include_deletes=include_deletes)

  def FilterSourceFile(self,
                       affected_file,
                       files_to_check=None,
                       files_to_skip=None,
                       allow_list=None,
                       block_list=None):
    """Filters out files that aren't considered 'source file'.

    If files_to_check or files_to_skip is None, InputApi.DEFAULT_FILES_TO_CHECK
    and InputApi.DEFAULT_FILES_TO_SKIP is used respectively.

    affected_file.LocalPath() needs to re.match an entry in the files_to_check
    list and not re.match any entries in the files_to_skip list.
    '/' path separators should be used in the regular expressions and will work
    on Windows as well as other platforms.

    Note: Copy-paste this function to suit your needs or use a lambda function.
    """
    if files_to_check is None:
      files_to_check = self.DEFAULT_FILES_TO_CHECK
    if files_to_skip is None:
      files_to_skip = self.DEFAULT_FILES_TO_SKIP

    def Find(affected_file, items):
      local_path = affected_file.LocalPath()
      for item in items:
        if self.re.match(item, local_path):
          return True
        # Handle the cases where the files regex only handles /, but the local
        # path uses \.
        if self.is_windows and self.re.match(item, local_path.replace(
            '\\', '/')):
          return True
      return False
    return (Find(affected_file, files_to_check) and
            not Find(affected_file, files_to_skip))

  def AffectedSourceFiles(self, source_file):
    """Filter the list of AffectedTestableFiles by the function source_file.

    If source_file is None, InputApi.FilterSourceFile() is used.
    """
    if not source_file:
      source_file = self.FilterSourceFile
    return list(filter(source_file, self.AffectedTestableFiles()))

  def RightHandSideLines(self, source_file_filter=None):
    """An iterator over all text lines in 'new' version of changed files.

    Only lists lines from new or modified text files in the change that are
    contained by the directory of the currently executing presubmit script.

    This is useful for doing line-by-line regex checks, like checking for
    trailing whitespace.

    Yields:
      a 3 tuple:
        the AffectedFile instance of the current file;
        integer line number (1-based); and
        the contents of the line as a string.

    Note: The carriage return (LF or CR) is stripped off.
    """
    files = self.AffectedSourceFiles(source_file_filter)
    return _RightHandSideLinesImpl(files)

  def ReadFile(self, file_item, mode='r'):
    """Reads an arbitrary file.

    Deny reading anything outside the repository.
    """
    if isinstance(file_item, AffectedFile):
      file_item = file_item.AbsoluteLocalPath()
    if not file_item.startswith(self.change.RepositoryRoot()):
      raise IOError('Access outside the repository root is denied.')
    return gclient_utils.FileRead(file_item, mode)

  def CreateTemporaryFile(self, **kwargs):
    """Returns a named temporary file that must be removed with a call to
    RemoveTemporaryFiles().

    All keyword arguments are forwarded to tempfile.NamedTemporaryFile(),
    except for |delete|, which is always set to False.

    Presubmit checks that need to create a temporary file and pass it for
    reading should use this function instead of NamedTemporaryFile(), as
    Windows fails to open a file that is already open for writing.

      with input_api.CreateTemporaryFile() as f:
        f.write('xyz')
        f.close()
        input_api.subprocess.check_output(['script-that', '--reads-from',
                                           f.name])


    Note that callers of CreateTemporaryFile() should not worry about removing
    any temporary file; this is done transparently by the presubmit handling
    code.
    """
    if 'delete' in kwargs:
      # Prevent users from passing |delete|; we take care of file deletion
      # ourselves and this prevents unintuitive error messages when we pass
      # delete=False and 'delete' is also in kwargs.
      raise TypeError('CreateTemporaryFile() does not take a "delete" '
                      'argument, file deletion is handled automatically by '
                      'the same presubmit_support code that creates InputApi '
                      'objects.')
    temp_file = self.tempfile.NamedTemporaryFile(delete=False, **kwargs)
    self._named_temporary_files.append(temp_file.name)
    return temp_file

  @property
  def tbr(self):
    """Returns if a change is TBR'ed."""
    return 'TBR' in self.change.tags or self.change.TBRsFromDescription()

  def RunTests(self, tests_mix, parallel=True):
    tests = []
    msgs = []
    for t in tests_mix:
      if isinstance(t, OutputApi.PresubmitResult) and t:
        msgs.append(t)
      else:
        assert issubclass(t.message, _PresubmitResult)
        tests.append(t)
        if self.verbose:
          t.info = _PresubmitNotifyResult
        if not t.kwargs.get('cwd'):
          t.kwargs['cwd'] = self.PresubmitLocalPath()
    self.thread_pool.AddTests(tests, parallel)
    # When self.parallel is True (i.e. --parallel is passed as an option)
    # RunTests doesn't actually run tests. It adds them to a ThreadPool that
    # will run all tests once all PRESUBMIT files are processed.
    # Otherwise, it will run them and return the results.
    if not self.parallel:
      msgs.extend(self.thread_pool.RunAsync())
    return msgs


class _DiffCache(object):
  """Caches diffs retrieved from a particular SCM."""
  def __init__(self, upstream=None):
    """Stores the upstream revision against which all diffs will be computed."""
    self._upstream = upstream

  def GetDiff(self, path, local_root):
    """Get the diff for a particular path."""
    raise NotImplementedError()

  def GetOldContents(self, path, local_root):
    """Get the old version for a particular path."""
    raise NotImplementedError()


class _GitDiffCache(_DiffCache):
  """DiffCache implementation for git; gets all file diffs at once."""
  def __init__(self, upstream):
    super(_GitDiffCache, self).__init__(upstream=upstream)
    self._diffs_by_file = None

  def GetDiff(self, path, local_root):
    # Compare against None to distinguish between None and an initialized but
    # empty dictionary.
    if self._diffs_by_file == None:
      # Compute a single diff for all files and parse the output; should
      # with git this is much faster than computing one diff for each file.
      diffs = {}

      # Don't specify any filenames below, because there are command line length
      # limits on some platforms and GenerateDiff would fail.
      unified_diff = scm.GIT.GenerateDiff(local_root, files=[], full_move=True,
                                          branch=self._upstream)

      # This regex matches the path twice, separated by a space. Note that
      # filename itself may contain spaces.
      file_marker = re.compile('^diff --git (?P<filename>.*) (?P=filename)$')
      current_diff = []
      keep_line_endings = True
      for x in unified_diff.splitlines(keep_line_endings):
        match = file_marker.match(x)
        if match:
          # Marks the start of a new per-file section.
          diffs[match.group('filename')] = current_diff = [x]
        elif x.startswith('diff --git'):
          raise PresubmitFailure('Unexpected diff line: %s' % x)
        else:
          current_diff.append(x)

      self._diffs_by_file = dict(
        (normpath(path), ''.join(diff)) for path, diff in diffs.items())

    if path not in self._diffs_by_file:
      # SCM didn't have any diff on this file. It could be that the file was not
      # modified at all (e.g. user used --all flag in git cl presubmit).
      # Intead of failing, return empty string.
      # See: https://crbug.com/808346.
      return ''

    return self._diffs_by_file[path]

  def GetOldContents(self, path, local_root):
    return scm.GIT.GetOldContents(local_root, path, branch=self._upstream)


class AffectedFile(object):
  """Representation of a file in a change."""

  DIFF_CACHE = _DiffCache

  # Method could be a function
  # pylint: disable=no-self-use
  def __init__(self, path, action, repository_root, diff_cache):
    self._path = path
    self._action = action
    self._local_root = repository_root
    self._is_directory = None
    self._cached_changed_contents = None
    self._cached_new_contents = None
    self._diff_cache = diff_cache
    logging.debug('%s(%s)', self.__class__.__name__, self._path)

  def LocalPath(self):
    """Returns the path of this file on the local disk relative to client root.

    This should be used for error messages but not for accessing files,
    because presubmit checks are run with CWD=PresubmitLocalPath() (which is
    often != client root).
    """
    return normpath(self._path)

  def AbsoluteLocalPath(self):
    """Returns the absolute path of this file on the local disk.
    """
    return os.path.abspath(os.path.join(self._local_root, self.LocalPath()))

  def Action(self):
    """Returns the action on this opened file, e.g. A, M, D, etc."""
    return self._action

  def IsTestableFile(self):
    """Returns True if the file is a text file and not a binary file.

    Deleted files are not text file."""
    raise NotImplementedError()  # Implement when needed

  def IsTextFile(self):
    """An alias to IsTestableFile for backwards compatibility."""
    return self.IsTestableFile()

  def OldContents(self):
    """Returns an iterator over the lines in the old version of file.

    The old version is the file before any modifications in the user's
    workspace, i.e. the 'left hand side'.

    Contents will be empty if the file is a directory or does not exist.
    Note: The carriage returns (LF or CR) are stripped off.
    """
    return self._diff_cache.GetOldContents(self.LocalPath(),
                                           self._local_root).splitlines()

  def NewContents(self):
    """Returns an iterator over the lines in the new version of file.

    The new version is the file in the user's workspace, i.e. the 'right hand
    side'.

    Contents will be empty if the file is a directory or does not exist.
    Note: The carriage returns (LF or CR) are stripped off.
    """
    if self._cached_new_contents is None:
      self._cached_new_contents = []
      try:
        self._cached_new_contents = gclient_utils.FileRead(
            self.AbsoluteLocalPath(), 'rU').splitlines()
      except IOError:
        pass  # File not found?  That's fine; maybe it was deleted.
      except UnicodeDecodeError as e:
        # log the filename since we're probably trying to read a binary
        # file, and shouldn't be.
        print('Error reading %s: %s' % (self.AbsoluteLocalPath(), e))
        raise

    return self._cached_new_contents[:]

  def ChangedContents(self, keeplinebreaks=False):
    """Returns a list of tuples (line number, line text) of all new lines.

     This relies on the scm diff output describing each changed code section
     with a line of the form

     ^@@ <old line num>,<old size> <new line num>,<new size> @@$
    """
    # Don't return cached results when line breaks are requested.
    if not keeplinebreaks and self._cached_changed_contents is not None:
      return self._cached_changed_contents[:]
    result = []
    line_num = 0

    # The keeplinebreaks parameter to splitlines must be True or else the
    # CheckForWindowsLineEndings presubmit will be a NOP.
    for line in self.GenerateScmDiff().splitlines(keeplinebreaks):
      m = re.match(r'^@@ [0-9\,\+\-]+ \+([0-9]+)\,[0-9]+ @@', line)
      if m:
        line_num = int(m.groups(1)[0])
        continue
      if line.startswith('+') and not line.startswith('++'):
        result.append((line_num, line[1:]))
      if not line.startswith('-'):
        line_num += 1
    # Don't cache results with line breaks.
    if keeplinebreaks:
      return result;
    self._cached_changed_contents = result
    return self._cached_changed_contents[:]

  def __str__(self):
    return self.LocalPath()

  def GenerateScmDiff(self):
    return self._diff_cache.GetDiff(self.LocalPath(), self._local_root)


class GitAffectedFile(AffectedFile):
  """Representation of a file in a change out of a git checkout."""
  # Method 'NNN' is abstract in class 'NNN' but is not overridden
  # pylint: disable=abstract-method

  DIFF_CACHE = _GitDiffCache

  def __init__(self, *args, **kwargs):
    AffectedFile.__init__(self, *args, **kwargs)
    self._server_path = None
    self._is_testable_file = None

  def IsTestableFile(self):
    if self._is_testable_file is None:
      if self.Action() == 'D':
        # A deleted file is not testable.
        self._is_testable_file = False
      else:
        self._is_testable_file = os.path.isfile(self.AbsoluteLocalPath())
    return self._is_testable_file


class Change(object):
  """Describe a change.

  Used directly by the presubmit scripts to query the current change being
  tested.

  Instance members:
    tags: Dictionary of KEY=VALUE pairs found in the change description.
    self.KEY: equivalent to tags['KEY']
  """

  _AFFECTED_FILES = AffectedFile

  # Matches key/value (or 'tag') lines in changelist descriptions.
  TAG_LINE_RE = re.compile(
      '^[ \t]*(?P<key>[A-Z][A-Z_0-9]*)[ \t]*=[ \t]*(?P<value>.*?)[ \t]*$')
  scm = ''

  def __init__(
      self, name, description, local_root, files, issue, patchset, author,
      upstream=None):
    if files is None:
      files = []
    self._name = name
    # Convert root into an absolute path.
    self._local_root = os.path.abspath(local_root)
    self._upstream = upstream
    self.issue = issue
    self.patchset = patchset
    self.author_email = author

    self._full_description = ''
    self.tags = {}
    self._description_without_tags = ''
    self.SetDescriptionText(description)

    assert all(
        (isinstance(f, (list, tuple)) and len(f) == 2) for f in files), files

    diff_cache = self._AFFECTED_FILES.DIFF_CACHE(self._upstream)
    self._affected_files = [
        self._AFFECTED_FILES(path, action.strip(), self._local_root, diff_cache)
        for action, path in files
    ]

  def UpstreamBranch(self):
    """Returns the upstream branch for the change."""
    return self._upstream

  def Name(self):
    """Returns the change name."""
    return self._name

  def DescriptionText(self):
    """Returns the user-entered changelist description, minus tags.

    Any line in the user-provided description starting with e.g. 'FOO='
    (whitespace permitted before and around) is considered a tag line.  Such
    lines are stripped out of the description this function returns.
    """
    return self._description_without_tags

  def FullDescriptionText(self):
    """Returns the complete changelist description including tags."""
    return self._full_description

  def SetDescriptionText(self, description):
    """Sets the full description text (including tags) to |description|.

    Also updates the list of tags."""
    self._full_description = description

    # From the description text, build up a dictionary of key/value pairs
    # plus the description minus all key/value or 'tag' lines.
    description_without_tags = []
    self.tags = {}
    for line in self._full_description.splitlines():
      m = self.TAG_LINE_RE.match(line)
      if m:
        self.tags[m.group('key')] = m.group('value')
      else:
        description_without_tags.append(line)

    # Change back to text and remove whitespace at end.
    self._description_without_tags = (
        '\n'.join(description_without_tags).rstrip())

  def AddDescriptionFooter(self, key, value):
    """Adds the given footer to the change description.

    Args:
      key: A string with the key for the git footer. It must conform to
        the git footers format (i.e. 'List-Of-Tokens') and will be case
        normalized so that each token is title-cased.
      value: A string with the value for the git footer.
    """
    description = git_footers.add_footer(
        self.FullDescriptionText(), git_footers.normalize_name(key), value)
    self.SetDescriptionText(description)

  def RepositoryRoot(self):
    """Returns the repository (checkout) root directory for this change,
    as an absolute path.
    """
    return self._local_root

  def __getattr__(self, attr):
    """Return tags directly as attributes on the object."""
    if not re.match(r'^[A-Z_]*$', attr):
      raise AttributeError(self, attr)
    return self.tags.get(attr)

  def GitFootersFromDescription(self):
    """Return the git footers present in the description.

    Returns:
      footers: A dict of {footer: [values]} containing a multimap of the footers
        in the change description.
    """
    return git_footers.parse_footers(self.FullDescriptionText())

  def BugsFromDescription(self):
    """Returns all bugs referenced in the commit description."""
    bug_tags = ['BUG', 'FIXED']

    tags = []
    for tag in bug_tags:
      values = self.tags.get(tag)
      if values:
        tags += [value.strip() for value in values.split(',')]

    footers = []
    parsed = self.GitFootersFromDescription()
    unsplit_footers = parsed.get('Bug', []) + parsed.get('Fixed', [])
    for unsplit_footer in unsplit_footers:
      footers += [b.strip() for b in unsplit_footer.split(',')]
    return sorted(set(tags + footers))

  def ReviewersFromDescription(self):
    """Returns all reviewers listed in the commit description."""
    # We don't support a 'R:' git-footer for reviewers; that is in metadata.
    tags = [r.strip() for r in self.tags.get('R', '').split(',') if r.strip()]
    return sorted(set(tags))

  def TBRsFromDescription(self):
    """Returns all TBR reviewers listed in the commit description."""
    tags = [r.strip() for r in self.tags.get('TBR', '').split(',') if r.strip()]
    # TODO(crbug.com/839208): Remove support for 'Tbr:' when TBRs are
    # programmatically determined by self-CR+1s.
    footers = self.GitFootersFromDescription().get('Tbr', [])
    return sorted(set(tags + footers))

  # TODO(crbug.com/753425): Delete these once we're sure they're unused.
  @property
  def BUG(self):
    return ','.join(self.BugsFromDescription())
  @property
  def R(self):
    return ','.join(self.ReviewersFromDescription())
  @property
  def TBR(self):
    return ','.join(self.TBRsFromDescription())

  def AllFiles(self, root=None):
    """List all files under source control in the repo."""
    raise NotImplementedError()

  def AffectedFiles(self, include_deletes=True, file_filter=None):
    """Returns a list of AffectedFile instances for all files in the change.

    Args:
      include_deletes: If false, deleted files will be filtered out.
      file_filter: An additional filter to apply.

    Returns:
      [AffectedFile(path, action), AffectedFile(path, action)]
    """
    affected = list(filter(file_filter, self._affected_files))

    if include_deletes:
      return affected
    return list(filter(lambda x: x.Action() != 'D', affected))

  def AffectedTestableFiles(self, include_deletes=None, **kwargs):
    """Return a list of the existing text files in a change."""
    if include_deletes is not None:
      warn('AffectedTeestableFiles(include_deletes=%s)'
               ' is deprecated and ignored' % str(include_deletes),
           category=DeprecationWarning,
           stacklevel=2)
    return list(filter(
        lambda x: x.IsTestableFile(),
        self.AffectedFiles(include_deletes=False, **kwargs)))

  def AffectedTextFiles(self, include_deletes=None):
    """An alias to AffectedTestableFiles for backwards compatibility."""
    return self.AffectedTestableFiles(include_deletes=include_deletes)

  def LocalPaths(self):
    """Convenience function."""
    return [af.LocalPath() for af in self.AffectedFiles()]

  def AbsoluteLocalPaths(self):
    """Convenience function."""
    return [af.AbsoluteLocalPath() for af in self.AffectedFiles()]

  def RightHandSideLines(self):
    """An iterator over all text lines in 'new' version of changed files.

    Lists lines from new or modified text files in the change.

    This is useful for doing line-by-line regex checks, like checking for
    trailing whitespace.

    Yields:
      a 3 tuple:
        the AffectedFile instance of the current file;
        integer line number (1-based); and
        the contents of the line as a string.
    """
    return _RightHandSideLinesImpl(
        x for x in self.AffectedFiles(include_deletes=False)
        if x.IsTestableFile())

  def OriginalOwnersFiles(self):
    """A map from path names of affected OWNERS files to their old content."""
    def owners_file_filter(f):
      return 'OWNERS' in os.path.split(f.LocalPath())[1]
    files = self.AffectedFiles(file_filter=owners_file_filter)
    return {f.LocalPath(): f.OldContents() for f in files}


class GitChange(Change):
  _AFFECTED_FILES = GitAffectedFile
  scm = 'git'

  def AllFiles(self, root=None):
    """List all files under source control in the repo."""
    root = root or self.RepositoryRoot()
    return subprocess.check_output(
        ['git', '-c', 'core.quotePath=false', 'ls-files', '--', '.'],
        cwd=root).decode('utf-8', 'ignore').splitlines()


def ListRelevantPresubmitFiles(files, root):
  """Finds all presubmit files that apply to a given set of source files.

  If inherit-review-settings-ok is present right under root, looks for
  PRESUBMIT.py in directories enclosing root.

  Args:
    files: An iterable container containing file paths.
    root: Path where to stop searching.

  Return:
    List of absolute paths of the existing PRESUBMIT.py scripts.
  """
  files = [normpath(os.path.join(root, f)) for f in files]

  # List all the individual directories containing files.
  directories = {os.path.dirname(f) for f in files}

  # Ignore root if inherit-review-settings-ok is present.
  if os.path.isfile(os.path.join(root, 'inherit-review-settings-ok')):
    root = None

  # Collect all unique directories that may contain PRESUBMIT.py.
  candidates = set()
  for directory in directories:
    while True:
      if directory in candidates:
        break
      candidates.add(directory)
      if directory == root:
        break
      parent_dir = os.path.dirname(directory)
      if parent_dir == directory:
        # We hit the system root directory.
        break
      directory = parent_dir

  # Look for PRESUBMIT.py in all candidate directories.
  results = []
  for directory in sorted(list(candidates)):
    try:
      for f in os.listdir(directory):
        p = os.path.join(directory, f)
        if os.path.isfile(p) and re.match(
            r'PRESUBMIT.*\.py$', f) and not f.startswith('PRESUBMIT_test'):
          results.append(p)
    except OSError:
      pass

  logging.debug('Presubmit files: %s', ','.join(results))
  return results


class GetPostUploadExecuter(object):
  def __init__(self, change, gerrit_obj, use_python3):
    """
    Args:
      change: The Change object.
      gerrit_obj: provides basic Gerrit codereview functionality.
      use_python3: if true, will use python3 instead of python2 by default
                if USE_PYTHON3 is not specified.
    """
    self.change = change
    self.gerrit = gerrit_obj
    self.use_python3 = use_python3

  def ExecPresubmitScript(self, script_text, presubmit_path):
    """Executes PostUploadHook() from a single presubmit script.
    Caller is responsible for validating whether the hook should be executed
    and should only call this function if it should be.

    Args:
      script_text: The text of the presubmit script.
      presubmit_path: Project script to run.

    Return:
      A list of results objects.
    """
    # Change to the presubmit file's directory to support local imports.
    presubmit_dir = os.path.dirname(presubmit_path)
    main_path = os.getcwd()
    try:
      os.chdir(presubmit_dir)
      return self._execute_with_local_working_directory(script_text,
                                                        presubmit_dir,
                                                        presubmit_path)
    finally:
      # Return the process to the original working directory.
      os.chdir(main_path)

  def _execute_with_local_working_directory(self, script_text, presubmit_dir,
                                            presubmit_path):
    context = {}
    try:
      exec(compile(script_text, presubmit_path, 'exec', dont_inherit=True),
           context)
    except Exception as e:
      raise PresubmitFailure('"%s" had an exception.\n%s'
                             % (presubmit_path, e))

    function_name = 'PostUploadHook'
    if function_name not in context:
      return {}
    post_upload_hook = context[function_name]
    if not len(inspect.getargspec(post_upload_hook)[0]) == 3:
      raise PresubmitFailure(
          'Expected function "PostUploadHook" to take three arguments.')
    return post_upload_hook(self.gerrit, self.change, OutputApi(False))


def _MergeMasters(masters1, masters2):
  """Merges two master maps. Merges also the tests of each builder."""
  result = {}
  for (master, builders) in itertools.chain(masters1.items(),
                                            masters2.items()):
    new_builders = result.setdefault(master, {})
    for (builder, tests) in builders.items():
      new_builders.setdefault(builder, set([])).update(tests)
  return result


def DoPostUploadExecuter(change, gerrit_obj, verbose, use_python3=False):
  """Execute the post upload hook.

  Args:
    change: The Change object.
    gerrit_obj: The GerritAccessor object.
    verbose: Prints debug info.
    use_python3: if true, default to using Python3 for presubmit checks
                 rather than Python2.
  """
  python_version = 'Python %s' % sys.version_info.major
  sys.stdout.write('Running %s post upload checks ...\n' % python_version)
  presubmit_files = ListRelevantPresubmitFiles(
      change.LocalPaths(), change.RepositoryRoot())
  if not presubmit_files and verbose:
    sys.stdout.write('Warning, no PRESUBMIT.py found.\n')
  results = []
  executer = GetPostUploadExecuter(change, gerrit_obj, use_python3)
  # The root presubmit file should be executed after the ones in subdirectories.
  # i.e. the specific post upload hooks should run before the general ones.
  # Thus, reverse the order provided by ListRelevantPresubmitFiles.
  presubmit_files.reverse()

  for filename in presubmit_files:
    filename = os.path.abspath(filename)
    # Accept CRLF presubmit script.
    presubmit_script = gclient_utils.FileRead(filename).replace('\r\n', '\n')
    if _ShouldRunPresubmit(presubmit_script, use_python3):
      if sys.version_info[0] == 2:
        sys.stdout.write(
            'Running %s under Python 2. Add USE_PYTHON3 = True to prevent '
            'this.\n' % filename)
      elif verbose:
        sys.stdout.write('Running %s\n' % filename)
      results.extend(executer.ExecPresubmitScript(presubmit_script, filename))

  if not results:
    return 0

  sys.stdout.write('\n')
  sys.stdout.write('** Post Upload Hook Messages **\n')

  exit_code = 0
  for result in results:
    if result.fatal:
      exit_code = 1
    result.handle()
    sys.stdout.write('\n')

  return exit_code

class PresubmitExecuter(object):
  def __init__(self, change, committing, verbose, gerrit_obj, dry_run=None,
               thread_pool=None, parallel=False, use_python3=False,
               no_diffs=False):
    """
    Args:
      change: The Change object.
      committing: True if 'git cl land' is running, False if 'git cl upload' is.
      gerrit_obj: provides basic Gerrit codereview functionality.
      dry_run: if true, some Checks will be skipped.
      parallel: if true, all tests reported via input_api.RunTests for all
                PRESUBMIT files will be run in parallel.
      use_python3: if true, will use python3 instead of python2 by default
                if USE_PYTHON3 is not specified.
      no_diffs: if true, implies that --files or --all was specified so some
                checks can be skipped, and some errors will be messages.
    """
    self.change = change
    self.committing = committing
    self.gerrit = gerrit_obj
    self.verbose = verbose
    self.dry_run = dry_run
    self.more_cc = []
    self.thread_pool = thread_pool
    self.parallel = parallel
    self.use_python3 = use_python3
    self.no_diffs = no_diffs

  def ExecPresubmitScript(self, script_text, presubmit_path):
    """Executes a single presubmit script.
    Caller is responsible for validating whether the hook should be executed
    and should only call this function if it should be.

    Args:
      script_text: The text of the presubmit script.
      presubmit_path: The path to the presubmit file (this will be reported via
        input_api.PresubmitLocalPath()).

    Return:
      A list of result objects, empty if no problems.
    """
    # Change to the presubmit file's directory to support local imports.
    presubmit_dir = os.path.dirname(presubmit_path)
    main_path = os.getcwd()
    try:
      os.chdir(presubmit_dir)
      return self._execute_with_local_working_directory(script_text,
                                                        presubmit_dir,
                                                        presubmit_path)
    finally:
      # Return the process to the original working directory.
      os.chdir(main_path)

  def _execute_with_local_working_directory(self, script_text, presubmit_dir,
                                            presubmit_path):
    # Load the presubmit script into context.
    input_api = InputApi(self.change, presubmit_path, self.committing,
                         self.verbose, gerrit_obj=self.gerrit,
                         dry_run=self.dry_run, thread_pool=self.thread_pool,
                         parallel=self.parallel, no_diffs=self.no_diffs)
    output_api = OutputApi(self.committing)
    context = {}

    try:
      exec(compile(script_text, presubmit_path, 'exec', dont_inherit=True),
           context)
    except Exception as e:
      raise PresubmitFailure('"%s" had an exception.\n%s' % (presubmit_path, e))

    context['__args'] = (input_api, output_api)

    # Get path of presubmit directory relative to repository root.
    # Always use forward slashes, so that path is same in *nix and Windows
    root = input_api.change.RepositoryRoot()
    rel_path = os.path.relpath(presubmit_dir, root)
    rel_path = rel_path.replace(os.path.sep, '/')

    # Get the URL of git remote origin and use it to identify host and project
    host = project = ''
    if self.gerrit:
      host = self.gerrit.host or ''
      project = self.gerrit.project or ''

    # Prefix for test names
    prefix = 'presubmit:%s/%s:%s/' % (host, project, rel_path)

    # Perform all the desired presubmit checks.
    results = []

    try:
      version = [
          int(x) for x in context.get('PRESUBMIT_VERSION', '0.0.0').split('.')
      ]

      with rdb_wrapper.client(prefix) as sink:
        if version >= [2, 0, 0]:
          # Copy the keys to prevent "dictionary changed size during iteration"
          # exception if checks add globals to context. E.g. sometimes the
          # Python runtime will add __warningregistry__.
          for function_name in list(context.keys()):
            if not function_name.startswith('Check'):
              continue
            if function_name.endswith('Commit') and not self.committing:
              continue
            if function_name.endswith('Upload') and self.committing:
              continue
            logging.debug('Running %s in %s', function_name, presubmit_path)
            results.extend(
                self._run_check_function(function_name, context, sink,
                                         presubmit_path))
            logging.debug('Running %s done.', function_name)
            self.more_cc.extend(output_api.more_cc)

        else:  # Old format
          if self.committing:
            function_name = 'CheckChangeOnCommit'
          else:
            function_name = 'CheckChangeOnUpload'
          if function_name in list(context.keys()):
            logging.debug('Running %s in %s', function_name, presubmit_path)
            results.extend(
                self._run_check_function(function_name, context, sink,
                                         presubmit_path))
            logging.debug('Running %s done.', function_name)
            self.more_cc.extend(output_api.more_cc)

    finally:
      for f in input_api._named_temporary_files:
        os.remove(f)

    return results

  def _run_check_function(self, function_name, context, sink, presubmit_path):
    """Evaluates and returns the result of a given presubmit function.

    If sink is given, the result of the presubmit function will be reported
    to the ResultSink.

    Args:
      function_name: the name of the presubmit function to evaluate
      context: a context dictionary in which the function will be evaluated
      sink: an instance of ResultSink. None, by default.
    Returns:
      the result of the presubmit function call.
    """
    start_time = time_time()
    try:
      result = eval(function_name + '(*__args)', context)
      self._check_result_type(result)
    except Exception:
      _, e_value, _ = sys.exc_info()
      result = [
          OutputApi.PresubmitError(
              'Evaluation of %s failed: %s, %s' %
              (function_name, e_value, traceback.format_exc()))
      ]

    elapsed_time = time_time() - start_time
    if elapsed_time > 10.0:
      sys.stdout.write('%6.1fs to run %s from %s.\n' %
                       (elapsed_time, function_name, presubmit_path))
    if sink:
      failure_reason = None
      status = rdb_wrapper.STATUS_PASS
      if any(r.fatal for r in result):
        status = rdb_wrapper.STATUS_FAIL
        failure_reasons = []
        for r in result:
          fields = r.json_format()
          message = fields['message']
          items = '\n'.join('  %s' % item for item in fields['items'])
          failure_reasons.append('\n'.join([message, items]))
        if failure_reasons:
          failure_reason = '\n'.join(failure_reasons)
      sink.report(function_name, status, elapsed_time, failure_reason)

    return result

  def _check_result_type(self, result):
    """Helper function which ensures result is a list, and all elements are
    instances of OutputApi.PresubmitResult"""
    if not isinstance(result, (tuple, list)):
      raise PresubmitFailure('Presubmit functions must return a tuple or list')
    if not all(isinstance(res, OutputApi.PresubmitResult) for res in result):
      raise PresubmitFailure(
        'All presubmit results must be of types derived from '
        'output_api.PresubmitResult')


def DoPresubmitChecks(change,
                      committing,
                      verbose,
                      default_presubmit,
                      may_prompt,
                      gerrit_obj,
                      dry_run=None,
                      parallel=False,
                      json_output=None,
                      use_python3=False,
                      no_diffs=False):
  """Runs all presubmit checks that apply to the files in the change.

  This finds all PRESUBMIT.py files in directories enclosing the files in the
  change (up to the repository root) and calls the relevant entrypoint function
  depending on whether the change is being committed or uploaded.

  Prints errors, warnings and notifications.  Prompts the user for warnings
  when needed.

  Args:
    change: The Change object.
    committing: True if 'git cl land' is running, False if 'git cl upload' is.
    verbose: Prints debug info.
    default_presubmit: A default presubmit script to execute in any case.
    may_prompt: Enable (y/n) questions on warning or error. If False,
                any questions are answered with yes by default.
    gerrit_obj: provides basic Gerrit codereview functionality.
    dry_run: if true, some Checks will be skipped.
    parallel: if true, all tests specified by input_api.RunTests in all
              PRESUBMIT files will be run in parallel.
    use_python3: if true, default to using Python3 for presubmit checks
                 rather than Python2.
    no_diffs: if true, implies that --files or --all was specified so some
              checks can be skipped, and some errors will be messages.
  Return:
    1 if presubmit checks failed or 0 otherwise.
  """
  old_environ = os.environ
  try:
    # Make sure python subprocesses won't generate .pyc files.
    os.environ = os.environ.copy()
    os.environ['PYTHONDONTWRITEBYTECODE'] = '1'

    python_version = 'Python %s' % sys.version_info.major
    if committing:
      sys.stdout.write('Running %s presubmit commit checks ...\n' %
                       python_version)
    else:
      sys.stdout.write('Running %s presubmit upload checks ...\n' %
                       python_version)
    start_time = time_time()
    presubmit_files = ListRelevantPresubmitFiles(
        change.AbsoluteLocalPaths(), change.RepositoryRoot())
    if not presubmit_files and verbose:
      sys.stdout.write('Warning, no PRESUBMIT.py found.\n')
    results = []
    if sys.platform == 'win32':
      temp = os.environ['TEMP']
    else:
      temp = '/tmp'
    python2_usage_log_file = os.path.join(temp, 'python2_usage.txt')
    if os.path.exists(python2_usage_log_file):
      os.remove(python2_usage_log_file)
    thread_pool = ThreadPool()
    executer = PresubmitExecuter(change, committing, verbose, gerrit_obj,
                                 dry_run, thread_pool, parallel, use_python3,
                                 no_diffs)
    skipped_count = 0;
    if default_presubmit:
      if verbose:
        sys.stdout.write('Running default presubmit script.\n')
      fake_path = os.path.join(change.RepositoryRoot(), 'PRESUBMIT.py')
      if _ShouldRunPresubmit(default_presubmit, use_python3):
        results += executer.ExecPresubmitScript(default_presubmit, fake_path)
      else:
        skipped_count += 1
    for filename in presubmit_files:
      filename = os.path.abspath(filename)
      # Accept CRLF presubmit script.
      presubmit_script = gclient_utils.FileRead(filename).replace('\r\n', '\n')
      if _ShouldRunPresubmit(presubmit_script, use_python3):
        if sys.version_info[0] == 2:
          sys.stdout.write(
              'Running %s under Python 2. Add USE_PYTHON3 = True to prevent '
              'this.\n' % filename)
        elif verbose:
          sys.stdout.write('Running %s\n' % filename)
        results += executer.ExecPresubmitScript(presubmit_script, filename)
      else:
        skipped_count += 1

    results += thread_pool.RunAsync()

    if os.path.exists(python2_usage_log_file):
      with open(python2_usage_log_file) as f:
        python2_usage = [x.strip() for x in f.readlines()]
        results.append(
            OutputApi(committing).PresubmitPromptWarning(
                'Python 2 scripts were run during %s presubmits. Please see '
                'https://bugs.chromium.org/p/chromium/issues/detail?id=1313804'
                '#c61 for tips on resolving this.'
                % python_version,
                items=python2_usage))

    messages = {}
    should_prompt = False
    presubmits_failed = False
    for result in results:
      if result.fatal:
        presubmits_failed = True
        messages.setdefault('ERRORS', []).append(result)
      elif result.should_prompt:
        should_prompt = True
        messages.setdefault('Warnings', []).append(result)
      else:
        messages.setdefault('Messages', []).append(result)

    # Print the different message types in a consistent order. ERRORS go last
    # so that they will be most visible in the local-presubmit output.
    for name in ['Messages', 'Warnings', 'ERRORS']:
      if name in messages:
        items = messages[name]
        sys.stdout.write('** Presubmit %s: %d **\n' % (name, len(items)))
        for item in items:
          item.handle()
          sys.stdout.write('\n')

    total_time = time_time() - start_time
    if total_time > 1.0:
      sys.stdout.write(
          'Presubmit checks took %.1fs to calculate.\n' % total_time)

    if not should_prompt and not presubmits_failed:
      sys.stdout.write('%s presubmit checks passed.\n\n' % python_version)
    elif should_prompt and not presubmits_failed:
      sys.stdout.write('There were %s presubmit warnings. ' % python_version)
      if may_prompt:
        presubmits_failed = not prompt_should_continue(
            'Are you sure you wish to continue? (y/N): ')
      else:
        sys.stdout.write('\n')
    else:
      sys.stdout.write('There were %s presubmit errors.\n' % python_version)

    if json_output:
      # Write the presubmit results to json output
      presubmit_results = {
        'errors': [
            error.json_format()
            for error in messages.get('ERRORS', [])
        ],
        'notifications': [
            notification.json_format()
            for notification in messages.get('Messages', [])
        ],
        'warnings': [
            warning.json_format()
            for warning in messages.get('Warnings', [])
        ],
        'more_cc': executer.more_cc,
        'skipped_presubmits': skipped_count,
      }

      gclient_utils.FileWrite(
          json_output, json.dumps(presubmit_results, sort_keys=True))

    global _ASKED_FOR_FEEDBACK
    # Ask for feedback one time out of 5.
    if (results and random.randint(0, 4) == 0 and not _ASKED_FOR_FEEDBACK):
      sys.stdout.write(
          'Was the presubmit check useful? If not, run "git cl presubmit -v"\n'
          'to figure out which PRESUBMIT.py was run, then run git blame\n'
          'on the file to figure out who to ask for help.\n')
      _ASKED_FOR_FEEDBACK = True

    return 1 if presubmits_failed else 0
  finally:
    os.environ = old_environ


def _scan_sub_dirs(mask, recursive):
  if not recursive:
    return [x for x in glob.glob(mask) if x not in ('.svn', '.git')]

  results = []
  for root, dirs, files in os.walk('.'):
    if '.svn' in dirs:
      dirs.remove('.svn')
    if '.git' in dirs:
      dirs.remove('.git')
    for name in files:
      if fnmatch.fnmatch(name, mask):
        results.append(os.path.join(root, name))
  return results


def _parse_files(args, recursive):
  logging.debug('Searching for %s', args)
  files = []
  for arg in args:
    files.extend([('M', f) for f in _scan_sub_dirs(arg, recursive)])
  return files


def _parse_change(parser, options):
  """Process change options.

  Args:
    parser: The parser used to parse the arguments from command line.
    options: The arguments parsed from command line.
  Returns:
    A GitChange if the change root is a git repository, or a Change otherwise.
  """
  if options.files and options.all_files:
    parser.error('<files> cannot be specified when --all-files is set.')

  change_scm = scm.determine_scm(options.root)
  if change_scm != 'git' and not options.files:
    parser.error('<files> is not optional for unversioned directories.')

  if options.files:
    if options.source_controlled_only:
      # Get the filtered set of files from SCM.
      change_files = []
      for name in scm.GIT.GetAllFiles(options.root):
        for mask in options.files:
          if fnmatch.fnmatch(name, mask):
            change_files.append(('M', name))
            break
    else:
      # Get the filtered set of files from a directory scan.
      change_files = _parse_files(options.files, options.recursive)
  elif options.all_files:
    change_files = [('M', f) for f in scm.GIT.GetAllFiles(options.root)]
  else:
    change_files = scm.GIT.CaptureStatus(
        options.root, options.upstream or None)

  logging.info('Found %d file(s).', len(change_files))

  change_class = GitChange if change_scm == 'git' else Change
  return change_class(
      options.name,
      options.description,
      options.root,
      change_files,
      options.issue,
      options.patchset,
      options.author,
      upstream=options.upstream)


def _parse_gerrit_options(parser, options):
  """Process gerrit options.

  SIDE EFFECTS: Modifies options.author and options.description from Gerrit if
  options.gerrit_fetch is set.

  Args:
    parser: The parser used to parse the arguments from command line.
    options: The arguments parsed from command line.
  Returns:
    A GerritAccessor object if options.gerrit_url is set, or None otherwise.
  """
  gerrit_obj = None
  if options.gerrit_url:
    gerrit_obj = GerritAccessor(
        url=options.gerrit_url,
        project=options.gerrit_project,
        branch=options.gerrit_branch)

  if not options.gerrit_fetch:
    return gerrit_obj

  if not options.gerrit_url or not options.issue or not options.patchset:
    parser.error(
        '--gerrit_fetch requires --gerrit_url, --issue and --patchset.')

  options.author = gerrit_obj.GetChangeOwner(options.issue)
  options.description = gerrit_obj.GetChangeDescription(
      options.issue, options.patchset)

  logging.info('Got author: "%s"', options.author)
  logging.info('Got description: """\n%s\n"""', options.description)

  return gerrit_obj


@contextlib.contextmanager
def canned_check_filter(method_names):
  filtered = {}
  try:
    for method_name in method_names:
      if not hasattr(presubmit_canned_checks, method_name):
        logging.warning('Skipping unknown "canned" check %s' % method_name)
        continue
      filtered[method_name] = getattr(presubmit_canned_checks, method_name)
      setattr(presubmit_canned_checks, method_name, lambda *_a, **_kw: [])
    yield
  finally:
    for name, method in filtered.items():
      setattr(presubmit_canned_checks, name, method)


def main(argv=None):
  parser = argparse.ArgumentParser(usage='%(prog)s [options] <files...>')
  hooks = parser.add_mutually_exclusive_group()
  hooks.add_argument('-c', '--commit', action='store_true',
                     help='Use commit instead of upload checks.')
  hooks.add_argument('-u', '--upload', action='store_false', dest='commit',
                     help='Use upload instead of commit checks.')
  hooks.add_argument('--post_upload', action='store_true',
                     help='Run post-upload commit hooks.')
  parser.add_argument('-r', '--recursive', action='store_true',
                      help='Act recursively.')
  parser.add_argument('-v', '--verbose', action='count', default=0,
                      help='Use 2 times for more debug info.')
  parser.add_argument('--name', default='no name')
  parser.add_argument('--author')
  desc = parser.add_mutually_exclusive_group()
  desc.add_argument('--description', default='', help='The change description.')
  desc.add_argument('--description_file',
                    help='File to read change description from.')
  parser.add_argument('--issue', type=int, default=0)
  parser.add_argument('--patchset', type=int, default=0)
  parser.add_argument('--root', default=os.getcwd(),
                      help='Search for PRESUBMIT.py up to this directory. '
                      'If inherit-review-settings-ok is present in this '
                      'directory, parent directories up to the root file '
                      'system directories will also be searched.')
  parser.add_argument('--upstream',
                      help='Git only: the base ref or upstream branch against '
                      'which the diff should be computed.')
  parser.add_argument('--default_presubmit')
  parser.add_argument('--may_prompt', action='store_true', default=False)
  parser.add_argument('--skip_canned', action='append', default=[],
                      help='A list of checks to skip which appear in '
                      'presubmit_canned_checks. Can be provided multiple times '
                      'to skip multiple canned checks.')
  parser.add_argument('--dry_run', action='store_true', help=argparse.SUPPRESS)
  parser.add_argument('--gerrit_url', help=argparse.SUPPRESS)
  parser.add_argument('--gerrit_project', help=argparse.SUPPRESS)
  parser.add_argument('--gerrit_branch', help=argparse.SUPPRESS)
  parser.add_argument('--gerrit_fetch', action='store_true',
                      help=argparse.SUPPRESS)
  parser.add_argument('--parallel', action='store_true',
                      help='Run all tests specified by input_api.RunTests in '
                           'all PRESUBMIT files in parallel.')
  parser.add_argument('--json_output',
                      help='Write presubmit errors to json output.')
  parser.add_argument('--all_files', action='store_true',
                      help='Mark all files under source control as modified.')

  parser.add_argument('files', nargs='*',
                      help='List of files to be marked as modified when '
                      'executing presubmit or post-upload hooks. fnmatch '
                      'wildcards can also be used.')
  parser.add_argument('--source_controlled_only', action='store_true',
                      help='Constrain \'files\' to those in source control.')
  parser.add_argument('--use-python3', action='store_true',
                      help='Use python3 for presubmit checks by default')
  parser.add_argument('--no_diffs', action='store_true',
                      help='Assume that all "modified" files have no diffs.')
  options = parser.parse_args(argv)

  log_level = logging.ERROR
  if options.verbose >= 2:
    log_level = logging.DEBUG
  elif options.verbose:
    log_level = logging.INFO
  log_format = ('[%(levelname).1s%(asctime)s %(process)d %(thread)d '
                '%(filename)s] %(message)s')
  logging.basicConfig(format=log_format, level=log_level)

  # Print call stacks when _PresubmitResult objects are created with -v -v is
  # specified. This helps track down where presubmit messages are coming from.
  if options.verbose >= 2:
    global _SHOW_CALLSTACKS
    _SHOW_CALLSTACKS = True

  if options.description_file:
    options.description = gclient_utils.FileRead(options.description_file)
  gerrit_obj = _parse_gerrit_options(parser, options)
  change = _parse_change(parser, options)

  try:
    if options.post_upload:
      return DoPostUploadExecuter(change, gerrit_obj, options.verbose,
                                  options.use_python3)
    with canned_check_filter(options.skip_canned):
      return DoPresubmitChecks(
          change,
          options.commit,
          options.verbose,
          options.default_presubmit,
          options.may_prompt,
          gerrit_obj,
          options.dry_run,
          options.parallel,
          options.json_output,
          options.use_python3,
          options.no_diffs)
  except PresubmitFailure as e:
    import utils
    print(e, file=sys.stderr)
    print('Maybe your depot_tools is out of date?', file=sys.stderr)
    print('depot_tools version: %s' % utils.depot_tools_version(),
          file=sys.stderr)
    return 2


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  try:
    sys.exit(main())
  except KeyboardInterrupt:
    sys.stderr.write('interrupted\n')
    sys.exit(2)
