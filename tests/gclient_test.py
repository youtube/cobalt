#!/usr/bin/env vpython3
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for gclient.py.

See gclient_smoketest.py for integration tests.
"""

import copy
import json
import logging
import ntpath
import os
import sys
import six
import unittest

if sys.version_info.major == 2:
  import mock
  import Queue
else:
  from unittest import mock
  import queue as Queue

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import metrics_utils
# We have to disable monitoring before importing gclient.
metrics_utils.COLLECT_METRICS = False

import gclient
import gclient_eval
import gclient_utils
import gclient_scm
from testing_support import trial_dir

def write(filename, content):
  """Writes the content of a file and create the directories as needed."""
  filename = os.path.abspath(filename)
  dirname = os.path.dirname(filename)
  if not os.path.isdir(dirname):
    os.makedirs(dirname)
  with open(filename, 'w') as f:
    f.write(content)


class SCMMock(object):
  unit_test = None
  def __init__(self, parsed_url, root_dir, name, out_fh=None, out_cb=None,
               print_outbuf=False):
    self.unit_test.assertTrue(
        parsed_url.startswith('svn://example.com/'), parsed_url)
    self.unit_test.assertTrue(
        root_dir.startswith(self.unit_test.root_dir), root_dir)
    self.name = name
    self.url = parsed_url

  def RunCommand(self, command, options, args, file_list):
    self.unit_test.assertEqual('None', command)
    self.unit_test.processed.put((self.name, self.url))

  # pylint: disable=no-self-use
  def DoesRemoteURLMatch(self, _):
    return True

  def GetActualRemoteURL(self, _):
    return self.url

  def revinfo(self, _, _a, _b):
    return 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbb'


class GclientTest(trial_dir.TestCase):
  def setUp(self):
    super(GclientTest, self).setUp()
    self.processed = Queue.Queue()
    self.previous_dir = os.getcwd()
    os.chdir(self.root_dir)
    # Manual mocks.
    self._old_createscm = gclient.gclient_scm.GitWrapper
    gclient.gclient_scm.GitWrapper = SCMMock
    SCMMock.unit_test = self

    mock.patch('os.environ', {}).start()
    self.addCleanup(mock.patch.stopall)

  def tearDown(self):
    self.assertEqual([], self._get_processed())
    gclient.gclient_scm.GitWrapper = self._old_createscm
    os.chdir(self.previous_dir)
    super(GclientTest, self).tearDown()

  def testDependencies(self):
    self._dependencies('1')

  def testDependenciesJobs(self):
    self._dependencies('1000')

  def _dependencies(self, jobs):
    """Verifies that dependencies are processed in the right order.

    e.g. if there is a dependency 'src' and another 'src/third_party/bar', that
    bar isn't fetched until 'src' is done.

    Args:
      |jobs| is the number of parallel jobs simulated.
    """
    parser = gclient.OptionParser()
    options, args = parser.parse_args(['--jobs', jobs])
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo" },\n'
        '  { "name": "bar", "url": "svn://example.com/bar" },\n'
        '  { "name": "bar/empty", "url": "svn://example.com/bar_empty" },\n'
        ']')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "foo/dir1": "/dir1",\n'
        # This one will depend on dir1/dir2 in bar.
        '  "foo/dir1/dir2/dir3": "/dir1/dir2/dir3",\n'
        '  "foo/dir1/dir2/dir3/dir4": "/dir1/dir2/dir3/dir4",\n'
        '}')
    write(
        os.path.join('bar', 'DEPS'),
        'deps = {\n'
        # There is two foo/dir1/dir2. This one is fetched as bar/dir1/dir2.
        '  "foo/dir1/dir2": "/dir1/dir2",\n'
        '}')
    write(
        os.path.join('bar/empty', 'DEPS'),
        'deps = {\n'
        '}')

    obj = gclient.GClient.LoadCurrentConfig(options)
    self._check_requirements(obj.dependencies[0], {})
    self._check_requirements(obj.dependencies[1], {})
    obj.RunOnDeps('None', args)
    actual = self._get_processed()
    first_3 = [
        ('bar', 'svn://example.com/bar'),
        ('bar/empty', 'svn://example.com/bar_empty'),
        ('foo', 'svn://example.com/foo'),
    ]
    if jobs != 1:
      # We don't care of the ordering of these items except that bar must be
      # before bar/empty.
      self.assertTrue(
          actual.index(('bar', 'svn://example.com/bar')) <
          actual.index(('bar/empty', 'svn://example.com/bar_empty')))
      self.assertEqual(first_3, sorted(actual[0:3]))
    else:
      self.assertEqual(first_3, actual[0:3])
    self.assertEqual(
        [
          ('foo/dir1', 'svn://example.com/dir1'),
          ('foo/dir1/dir2', 'svn://example.com/dir1/dir2'),
          ('foo/dir1/dir2/dir3', 'svn://example.com/dir1/dir2/dir3'),
          ('foo/dir1/dir2/dir3/dir4',
           'svn://example.com/dir1/dir2/dir3/dir4'),
        ],
        actual[3:])

    self.assertEqual(3, len(obj.dependencies))
    self.assertEqual('foo', obj.dependencies[0].name)
    self.assertEqual('bar', obj.dependencies[1].name)
    self.assertEqual('bar/empty', obj.dependencies[2].name)
    self._check_requirements(
        obj.dependencies[0],
        {
          'foo/dir1': ['bar', 'bar/empty', 'foo'],
          'foo/dir1/dir2/dir3':
              ['bar', 'bar/empty', 'foo', 'foo/dir1', 'foo/dir1/dir2'],
          'foo/dir1/dir2/dir3/dir4':
              [ 'bar', 'bar/empty', 'foo', 'foo/dir1', 'foo/dir1/dir2',
                'foo/dir1/dir2/dir3'],
        })
    self._check_requirements(
        obj.dependencies[1],
        {
          'foo/dir1/dir2': ['bar', 'bar/empty', 'foo', 'foo/dir1'],
        })
    self._check_requirements(
        obj.dependencies[2],
        {})
    self._check_requirements(
        obj,
        {
          'foo': [],
          'bar': [],
          'bar/empty': ['bar'],
        })

  def _check_requirements(self, solution, expected):
    for dependency in solution.dependencies:
      e = expected.pop(dependency.name)
      a = sorted(dependency.requirements)
      self.assertEqual(e, a, (dependency.name, e, a))
    self.assertEqual({}, expected)

  def _get_processed(self):
    """Retrieves the item in the order they were processed."""
    items = []
    try:
      while True:
        items.append(self.processed.get_nowait())
    except Queue.Empty:
      pass
    return items

  def _get_hooks(self):
    """Retrieves the hooks that would be run"""
    parser = gclient.OptionParser()
    options, _ = parser.parse_args([])
    options.force = True

    client = gclient.GClient.LoadCurrentConfig(options)
    work_queue = gclient_utils.ExecutionQueue(options.jobs, None, False)
    for s in client.dependencies:
      work_queue.enqueue(s)
    work_queue.flush({},
                     None, [],
                     options=options,
                     patch_refs={},
                     target_branches={},
                     skip_sync_revisions={})

    return client.GetHooks(options)

  def testAutofix(self):
    # Invalid urls causes pain when specifying requirements. Make sure it's
    # auto-fixed.
    url = 'proto://host/path/@revision'
    d = gclient.Dependency(
        parent=None,
        name='name',
        url=url,
        managed=None,
        custom_deps=None,
        custom_vars=None,
        custom_hooks=None,
        deps_file='',
        should_process=True,
        should_recurse=False,
        relative=False,
        condition=None,
        protocol='https',
        print_outbuf=True)
    self.assertEqual('proto://host/path@revision', d.url)

  def testStr(self):
    parser = gclient.OptionParser()
    options, _ = parser.parse_args([])
    obj = gclient.GClient('foo', options)
    obj.add_dependencies_and_close(
      [
        gclient.Dependency(
            parent=obj,
            name='foo',
            url='svn://example.com/foo',
            managed=None,
            custom_deps=None,
            custom_vars=None,
            custom_hooks=None,
            deps_file='DEPS',
            should_process=True,
            should_recurse=True,
            relative=False,
            condition=None,
            protocol='https',
            print_outbuf=True),
        gclient.Dependency(
            parent=obj,
            name='bar',
            url='svn://example.com/bar',
            managed=None,
            custom_deps=None,
            custom_vars=None,
            custom_hooks=None,
            deps_file='DEPS',
            should_process=True,
            should_recurse=False,
            relative=False,
            condition=None,
            protocol='https',
            print_outbuf=True),
      ],
      [])
    obj.dependencies[0].add_dependencies_and_close(
      [
        gclient.Dependency(
            parent=obj.dependencies[0],
            name='foo/dir1',
            url='svn://example.com/foo/dir1',
            managed=None,
            custom_deps=None,
            custom_vars=None,
            custom_hooks=None,
            deps_file='DEPS',
            should_process=True,
            should_recurse=False,
            relative=False,
            condition=None,
            protocol='https',
            print_outbuf=True),
      ],
      [])
    # TODO(ehmaldonado): Improve this test.
    # Make sure __str__() works fine.
    # pylint: disable=protected-access
    obj.dependencies[0]._file_list.append('foo')
    str_obj = str(obj)
    self.assertEqual(322, len(str_obj), '%d\n%s' % (len(str_obj), str_obj))

  def testHooks(self):
    hooks = [{'pattern':'.', 'action':['cmd1', 'arg1', 'arg2']}]

    write('.gclient',
          'solutions = [{\n'
          '  "name": "top",\n'
          '  "url": "svn://example.com/top"\n'
          '}]')
    write(os.path.join('top', 'DEPS'),
          'hooks =  %s' % repr(hooks))
    write(os.path.join('top', 'fake.txt'),
          "bogus content")

    self.assertEqual(
        [h.action for h in self._get_hooks()],
        [tuple(x['action']) for x in hooks])

  def testCustomHooks(self):
    extra_hooks = [{'name': 'append', 'pattern': '.', 'action': ['supercmd']}]

    write('.gclient',
          'solutions = [\n'
          '  {\n'
          '    "name": "top",\n'
          '    "url": "svn://example.com/top",\n' +
         ('    "custom_hooks": %s' % repr(extra_hooks + [{'name': 'skip'}])) +
          '  },\n'
          '  {\n'
          '    "name": "bottom",\n'
          '    "url": "svn://example.com/bottom"\n'
          '  }\n'
          ']')

    hooks = [
      {'pattern':'.', 'action': ['cmd1', 'arg1', 'arg2']},
      {'pattern':'.', 'action': ['cmd2', 'arg1', 'arg2']},
    ]
    skip_hooks = [
        {'name': 'skip', 'pattern':'.', 'action': ['cmd3', 'arg1', 'arg2']},
        {'name': 'skip', 'pattern':'.', 'action': ['cmd4', 'arg1', 'arg2']},
    ]
    write(os.path.join('top', 'DEPS'),
          'hooks =  %s' % repr(hooks + skip_hooks))

    # Make sure the custom hooks for that project don't affect the next one.
    sub_hooks = [
      {'pattern':'.', 'action':['response1', 'yes1', 'yes2']},
      {'name': 'skip', 'pattern': '.', 'action': ['response2', 'yes', 'sir']},
    ]
    write(os.path.join('bottom', 'DEPS'),
          'hooks =  %s' % repr(sub_hooks))

    write(os.path.join('bottom', 'fake.txt'),
          "bogus content")

    self.assertEqual(
        [h.action for h in self._get_hooks()],
        [tuple(x['action']) for x in hooks + extra_hooks + sub_hooks])

  def testRecurseDepsAndHooks(self):
    """Verifies that hooks in recursedeps are ran."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo" },\n'
        ']')
    write(
        os.path.join('foo', 'DEPS'),
        'use_relative_paths = True\n'
        'deps = {\n'
        '  "bar": "/bar",\n'
        '}\n'
        'recursedeps = ["bar"]')
    write(
        os.path.join('foo', 'bar', 'DEPS'),
        'hooks = [{\n'
        '  "name": "toto",\n'
        '  "pattern": ".",\n'
        '  "action": ["tata", "titi"]\n'
        '}]\n')
    write(os.path.join('foo', 'bar', 'fake.txt'),
          "bogus content")

    self.assertEqual(
        [h.action for h in self._get_hooks()],
        [('tata', 'titi')])

  def testRecurseDepsAndHooksCwd(self):
    """Verifies that hooks run in the correct directory with our without
    use_relative_paths"""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo" },\n'
        ']')
    write(
        os.path.join('foo', 'DEPS'),
        'use_relative_paths = True\n'
        'deps = {\n'
        '  "bar": "/bar",\n'
        '  "baz": "/baz",\n'
        '}\n'
        'recursedeps = ["bar", "baz"]')

    write(
        os.path.join('foo', 'bar', 'DEPS'),
        'hooks = [{\n'
        '  "name": "toto",\n'
        '  "pattern": ".",\n'
        '  "action": ["tata", "titi"]\n'
        '}]\n')
    write(os.path.join('foo', 'bar', 'fake.txt'),
          "bogus content")

    write(
        os.path.join('foo', 'baz', 'DEPS'),
        'use_relative_paths=True\n'
        'hooks = [{\n'
        '  "name": "lazors",\n'
        '  "pattern": ".",\n'
        '  "action": ["fire", "lazors"]\n'
        '}]\n')
    write(os.path.join('foo', 'baz', 'fake.txt'),
          "bogus content")

    self.assertEqual(
        [(h.action, h.effective_cwd) for h in self._get_hooks()],
        [(('tata', 'titi'), self.root_dir),
         (('fire', 'lazors'), os.path.join(self.root_dir, 'foo/baz'))])

  def testTargetOS(self):
    """Verifies that specifying a target_os pulls in all relevant dependencies.

    The target_os variable allows specifying the name of an additional OS which
    should be considered when selecting dependencies from a DEPS' deps_os. The
    value will be appended to the _enforced_os tuple.
    """

    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo",\n'
        '    "url": "svn://example.com/foo",\n'
        '  }]\n'
        'target_os = ["baz"]')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "foo/dir1": "/dir1",'
        '}\n'
        'deps_os = {\n'
        '  "unix": { "foo/dir2": "/dir2", },\n'
        '  "baz": { "foo/dir3": "/dir3", },\n'
        '}')

    parser = gclient.OptionParser()
    options, _ = parser.parse_args(['--jobs', '1'])
    options.deps_os = "unix"

    obj = gclient.GClient.LoadCurrentConfig(options)
    self.assertEqual(['baz', 'unix'], sorted(obj.enforced_os))

  def testTargetOsWithTargetOsOnly(self):
    """Verifies that specifying a target_os and target_os_only pulls in only
    the relevant dependencies.

    The target_os variable allows specifying the name of an additional OS which
    should be considered when selecting dependencies from a DEPS' deps_os. With
    target_os_only also set, the _enforced_os tuple will be set to only the
    target_os value.
    """

    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo",\n'
        '    "url": "svn://example.com/foo",\n'
        '  }]\n'
        'target_os = ["baz"]\n'
        'target_os_only = True')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "foo/dir1": "/dir1",'
        '}\n'
        'deps_os = {\n'
        '  "unix": { "foo/dir2": "/dir2", },\n'
        '  "baz": { "foo/dir3": "/dir3", },\n'
        '}')

    parser = gclient.OptionParser()
    options, _ = parser.parse_args(['--jobs', '1'])
    options.deps_os = "unix"

    obj = gclient.GClient.LoadCurrentConfig(options)
    self.assertEqual(['baz'], sorted(obj.enforced_os))

  def testTargetOsOnlyWithoutTargetOs(self):
    """Verifies that specifying a target_os_only without target_os_only raises
    an exception.
    """

    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo",\n'
        '    "url": "svn://example.com/foo",\n'
        '  }]\n'
        'target_os_only = True')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "foo/dir1": "/dir1",'
        '}\n'
        'deps_os = {\n'
        '  "unix": { "foo/dir2": "/dir2", },\n'
        '}')

    parser = gclient.OptionParser()
    options, _ = parser.parse_args(['--jobs', '1'])
    options.deps_os = "unix"

    exception_raised = False
    try:
      gclient.GClient.LoadCurrentConfig(options)
    except gclient_utils.Error:
      exception_raised = True
    self.assertTrue(exception_raised)

  def testTargetOsInDepsFile(self):
    """Verifies that specifying a target_os value in a DEPS file pulls in all
    relevant dependencies.

    The target_os variable in a DEPS file allows specifying the name of an
    additional OS which should be considered when selecting dependencies from a
    DEPS' deps_os. The value will be appended to the _enforced_os tuple.
    """

    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo",\n'
        '    "url": "svn://example.com/foo",\n'
        '  },\n'
        '  { "name": "bar",\n'
        '    "url": "svn://example.com/bar",\n'
        '  }]\n')
    write(
        os.path.join('foo', 'DEPS'),
        'target_os = ["baz"]\n')
    write(
        os.path.join('bar', 'DEPS'),
        '')

    parser = gclient.OptionParser()
    options, _ = parser.parse_args(['--jobs', '1'])
    options.deps_os = 'unix'

    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    self.assertEqual(['unix'], sorted(obj.enforced_os))
    self.assertEqual([['baz', 'unix'], ['unix']],
                     [sorted(dep.target_os) for dep in obj.dependencies])
    self.assertEqual([('foo', 'svn://example.com/foo'),
                      ('bar', 'svn://example.com/bar')],
                     self._get_processed())

  def testTargetOsForHooksInDepsFile(self):
    """Verifies that specifying a target_os value in a DEPS file runs the right
    entries in hooks_os.
    """

    write(
        'DEPS',
        'hooks = [\n'
        '  {\n'
        '    "name": "a",\n'
        '    "pattern": ".",\n'
        '    "action": [ "python", "do_a" ],\n'
        '  },\n'
        ']\n'
        '\n'
        'hooks_os = {\n'
        '  "blorp": ['
        '    {\n'
        '      "name": "b",\n'
        '      "pattern": ".",\n'
        '      "action": [ "python", "do_b" ],\n'
        '    },\n'
        '  ],\n'
        '}\n')

    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": ".",\n'
        '    "url": "svn://example.com/",\n'
        '  }]\n')
    # Test for an OS not in hooks_os.
    parser = gclient.OptionParser()
    options, args = parser.parse_args(['--jobs', '1'])
    options.deps_os = 'zippy'

    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', args)
    self.assertEqual(['zippy'], sorted(obj.enforced_os))
    all_hooks = obj.GetHooks(options)
    self.assertEqual(
        [('.', 'svn://example.com/'),],
        sorted(self._get_processed()))
    self.assertEqual([h.action for h in all_hooks],
                      [('python', 'do_a'),
                       ('python', 'do_b')])
    self.assertEqual([h.condition for h in all_hooks],
                      [None, 'checkout_blorp'])

  def testOverride(self):
    """Verifies expected behavior of URL overrides."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo",\n'
        '    "url": "svn://example.com/foo",\n'
        '    "custom_deps": {\n'
        '      "foo/bar": "svn://example.com/override",\n'
        '      "foo/skip2": None,\n'
        '      "foo/new": "svn://example.com/new",\n'
        '    },\n'
        '  },]\n')
    write(
        os.path.join('foo', 'DEPS'),
        'vars = {\n'
        '  "origin": "svn://example.com",\n'
        '}\n'
        'deps = {\n'
        '  "foo/skip": None,\n'
        '  "foo/bar": "{origin}/bar",\n'
        '  "foo/baz": "{origin}/baz",\n'
        '  "foo/skip2": "{origin}/skip2",\n'
        '  "foo/rel": "/rel",\n'
        '}')
    parser = gclient.OptionParser()
    options, _ = parser.parse_args(['--jobs', '1'])

    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])

    sol = obj.dependencies[0]
    self.assertEqual([
        ('foo', 'svn://example.com/foo'),
        ('foo/bar', 'svn://example.com/override'),
        ('foo/baz', 'svn://example.com/baz'),
        ('foo/new', 'svn://example.com/new'),
        ('foo/rel', 'svn://example.com/rel'),
    ], self._get_processed())

    self.assertEqual(6, len(sol.dependencies))
    self.assertEqual([
        ('foo/bar', 'svn://example.com/override'),
        ('foo/baz', 'svn://example.com/baz'),
        ('foo/new', 'svn://example.com/new'),
        ('foo/rel', 'svn://example.com/rel'),
        ('foo/skip', None),
        ('foo/skip2', None),
    ], [(dep.name, dep.url) for dep in sol.dependencies])

  def testVarOverrides(self):
    """Verifies expected behavior of variable overrides."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo",\n'
        '    "url": "svn://example.com/foo",\n'
        '    "custom_vars": {\n'
        '      "path": "c-d",\n'
        '    },\n'
        '  },]\n')
    write(
        os.path.join('foo', 'DEPS'),
        'vars = {\n'
        '  "path": Str("a-b"),\n'
        '}\n'
        'deps = {\n'
        '  "foo/bar": "svn://example.com/foo/" + Var("path"),\n'
        '}')
    parser = gclient.OptionParser()
    options, _ = parser.parse_args(['--jobs', '1'])

    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])

    sol = obj.dependencies[0]
    self.assertEqual([
        ('foo', 'svn://example.com/foo'),
        ('foo/bar', 'svn://example.com/foo/c-d'),
    ], self._get_processed())

    self.assertEqual(1, len(sol.dependencies))
    self.assertEqual([
        ('foo/bar', 'svn://example.com/foo/c-d'),
    ], [(dep.name, dep.url) for dep in sol.dependencies])

  def testDepsOsOverrideDepsInDepsFile(self):
    """Verifies that a 'deps_os' path cannot override a 'deps' path. Also
    see testUpdateWithOsDeps above.
    """

    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo",\n'
        '    "url": "svn://example.com/foo",\n'
        '  },]\n')
    write(
        os.path.join('foo', 'DEPS'),
        'target_os = ["baz"]\n'
        'deps = {\n'
        '  "foo/src": "/src",\n' # This path is to be overridden by similar path
                                 # in deps_os['unix'].
        '}\n'
        'deps_os = {\n'
        '  "unix": { "foo/unix": "/unix",'
        '            "foo/src": "/src_unix"},\n'
        '  "baz": { "foo/baz": "/baz",\n'
        '           "foo/src": None},\n'
        '  "jaz": { "foo/jaz": "/jaz", },\n'
        '}')

    parser = gclient.OptionParser()
    options, _ = parser.parse_args(['--jobs', '1'])
    options.deps_os = 'unix'

    obj = gclient.GClient.LoadCurrentConfig(options)
    with self.assertRaises(gclient_utils.Error):
      obj.RunOnDeps('None', [])
    self.assertEqual(['unix'], sorted(obj.enforced_os))
    self.assertEqual(
        [
          ('foo', 'svn://example.com/foo'),
          ],
        sorted(self._get_processed()))

  def testRecursedepsOverride(self):
    """Verifies gclient respects the |recursedeps| var syntax.

    This is what we mean to check here:
    - |recursedeps| = [...] on 2 levels means we pull exactly 3 deps
      (up to /fizz, but not /fuzz)
    - pulling foo/bar with no recursion (in .gclient) is overridden by
      a later pull of foo/bar with recursion (in the dep tree)
    - pulling foo/tar with no recursion (in .gclient) is no recursively
      pulled (taz is left out)
    """
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo" },\n'
        '  { "name": "foo/bar", "url": "svn://example.com/bar" },\n'
        '  { "name": "foo/tar", "url": "svn://example.com/tar" },\n'
          ']')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "bar": "/bar",\n'
        '}\n'
        'recursedeps = ["bar"]')
    write(
        os.path.join('bar', 'DEPS'),
        'deps = {\n'
        '  "baz": "/baz",\n'
        '}\n'
        'recursedeps = ["baz"]')
    write(
        os.path.join('baz', 'DEPS'),
        'deps = {\n'
        '  "fizz": "/fizz",\n'
        '}')
    write(
        os.path.join('fizz', 'DEPS'),
        'deps = {\n'
        '  "fuzz": "/fuzz",\n'
        '}')
    write(
        os.path.join('tar', 'DEPS'),
        'deps = {\n'
        '  "taz": "/taz",\n'
        '}')

    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    self.assertEqual(
        [
          ('bar', 'svn://example.com/bar'),
          ('baz', 'svn://example.com/baz'),
          ('fizz', 'svn://example.com/fizz'),
          ('foo', 'svn://example.com/foo'),
          ('foo/bar', 'svn://example.com/bar'),
          ('foo/tar', 'svn://example.com/tar'),
        ],
        sorted(self._get_processed()))

  def testRecursedepsOverrideWithRelativePaths(self):
    """Verifies gclient respects |recursedeps| with relative paths."""

    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo" },\n'
        ']')
    write(
        os.path.join('foo', 'DEPS'),
        'use_relative_paths = True\n'
        'deps = {\n'
        '  "bar": "/bar",\n'
        '}\n'
        'recursedeps = ["bar"]')
    write(
        os.path.join('foo/bar', 'DEPS'),
        'deps = {\n'
        '  "baz": "/baz",\n'
        '}')
    write(
        os.path.join('baz', 'DEPS'),
        'deps = {\n'
        '  "fizz": "/fizz",\n'
        '}')

    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    self.assertEqual([
        ('foo', 'svn://example.com/foo'),
        ('foo/bar', 'svn://example.com/bar'),
        ('foo/baz', 'svn://example.com/baz'),
    ], self._get_processed())

  def testRecursedepsCustomdepsOverride(self):
    """Verifies gclient overrides deps within recursedeps using custom deps"""

    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo",\n'
        '    "url": "svn://example.com/foo",\n'
        '    "custom_deps": {\n'
        '      "foo/bar": "svn://example.com/override",\n'
        '    },\n'
        '  },]\n')
    write(
        os.path.join('foo', 'DEPS'),
        'use_relative_paths = True\n'
        'deps = {\n'
        '  "bar": "/bar",\n'
        '}\n'
        'recursedeps = ["bar"]')
    write(
        os.path.join('foo', 'bar', 'DEPS'),
        'deps = {\n'
        '  "baz": "/baz",\n'
        '}')

    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    six.assertCountEqual(self, [
        ('foo', 'svn://example.com/foo'),
        ('foo/bar', 'svn://example.com/override'),
        ('foo/foo/bar', 'svn://example.com/override'),
        ('foo/baz', 'svn://example.com/baz'),
    ], self._get_processed())

  def testRelativeRecursion(self):
    """Verifies that nested use_relative_paths is always respected."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo" },\n'
        ']')
    write(
        os.path.join('foo', 'DEPS'),
        'use_relative_paths = True\n'
        'deps = {\n'
        '  "bar": "/bar",\n'
        '}\n'
        'recursedeps = ["bar"]')
    write(
        os.path.join('foo/bar', 'DEPS'),
        'use_relative_paths = True\n'
        'deps = {\n'
        '  "baz": "/baz",\n'
        '}')
    write(
        os.path.join('baz', 'DEPS'),
        'deps = {\n'
        '  "fizz": "/fizz",\n'
        '}')

    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    self.assertEqual([
        ('foo', 'svn://example.com/foo'),
        ('foo/bar', 'svn://example.com/bar'),
        ('foo/bar/baz', 'svn://example.com/baz'),
    ], self._get_processed())

  def testRelativeRecursionInNestedDir(self):
    """Verifies a gotcha of relative recursion where the parent uses relative
    paths but not the dependency being recursed in. In that case the recursed
    dependencies will only take into account the first directory of its path.
    In this test it can be seen in baz being placed in foo/third_party."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo" },\n'
        ']')
    write(
        os.path.join('foo', 'DEPS'),
        'use_relative_paths = True\n'
        'deps = {\n'
        '  "third_party/bar": "/bar",\n'
        '}\n'
        'recursedeps = ["third_party/bar"]')
    write(
        os.path.join('foo/third_party/bar', 'DEPS'),
        'deps = {\n'
        '  "baz": "/baz",\n'
        '}')
    write(
        os.path.join('baz', 'DEPS'),
        'deps = {\n'
        '  "fizz": "/fizz",\n'
        '}')

    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    self.assertEqual([
        ('foo', 'svn://example.com/foo'),
        ('foo/third_party/bar', 'svn://example.com/bar'),
        ('foo/third_party/baz', 'svn://example.com/baz'),
    ], self._get_processed())

  def testRecursedepsAltfile(self):
    """Verifies gclient respects the |recursedeps| var syntax with overridden
    target DEPS file.

    This is what we mean to check here:
    - Naming an alternate DEPS file in recursedeps pulls from that one.
    """
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo" },\n'
        ']')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "bar": "/bar",\n'
        '}\n'
        'recursedeps = [("bar", "DEPS.alt")]')
    write(os.path.join('bar', 'DEPS'), 'ERROR ERROR ERROR')
    write(
        os.path.join('bar', 'DEPS.alt'),
        'deps = {\n'
        '  "baz": "/baz",\n'
        '}')

    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    self.assertEqual(
        [
          ('foo', 'svn://example.com/foo'),
          ('bar', 'svn://example.com/bar'),
          ('baz', 'svn://example.com/baz'),
        ],
        self._get_processed())

  def testGitDeps(self):
    """Verifies gclient respects a .DEPS.git deps file.

    Along the way, we also test that if both DEPS and .DEPS.git are present,
    that gclient does not read the DEPS file.  This will reliably catch bugs
    where gclient is always hitting the wrong file (DEPS).
    """
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', '.DEPS.git'),
        'deps = {\n'
        '  "bar": "/bar",\n'
        '}')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "baz": "/baz",\n'
        '}')

    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    self.assertEqual(
        [
          ('foo', 'svn://example.com/foo'),
          ('bar', 'svn://example.com/bar'),
        ],
        self._get_processed())

  def testGitDepsFallback(self):
    """Verifies gclient respects fallback to DEPS upon missing deps file."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "bar": "/bar",\n'
        '}')

    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    self.assertEqual(
        [
          ('foo', 'svn://example.com/foo'),
          ('bar', 'svn://example.com/bar'),
        ],
        self._get_processed())

  def testIgnoresGitDependenciesWhenFlagIsSet(self):
    """Verifies that git deps are ignored if --ignore-dep-type git is set."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "https://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', 'DEPS'),
        'vars = {\n'
        '  "lemur_version": "version:1234",\n'
        '}\n'
        'deps = {\n'
        '  "bar": "/bar",\n'
        '  "baz": {\n'
        '    "packages": [{\n'
        '      "package": "lemur",\n'
        '      "version": Var("lemur_version"),\n'
        '    }],\n'
        '    "dep_type": "cipd",\n'
        '  }\n'
        '}')
    options, _ = gclient.OptionParser().parse_args([])
    options.ignore_dep_type = 'git'
    obj = gclient.GClient.LoadCurrentConfig(options)

    self.assertEqual(1, len(obj.dependencies))
    sol = obj.dependencies[0]
    sol._condition = 'some_condition'

    sol.ParseDepsFile()
    self.assertEqual(1, len(sol.dependencies))
    dep = sol.dependencies[0]

    self.assertIsInstance(dep, gclient.CipdDependency)
    self.assertEqual(
        'https://chrome-infra-packages.appspot.com/lemur@version:1234',
        dep.url)

  def testDepsFromNotAllowedHostsUnspecified(self):
    """Verifies gclient works fine with DEPS without allowed_hosts."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "bar": "/bar",\n'
        '}')
    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    dep = obj.dependencies[0]
    self.assertEqual([], dep.findDepsFromNotAllowedHosts())
    self.assertEqual(frozenset(), dep.allowed_hosts)
    self._get_processed()

  def testDepsFromNotAllowedHostsOK(self):
    """Verifies gclient works fine with DEPS with proper allowed_hosts."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', '.DEPS.git'),
        'allowed_hosts = ["example.com"]\n'
        'deps = {\n'
        '  "bar": "svn://example.com/bar",\n'
        '}')
    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    dep = obj.dependencies[0]
    self.assertEqual([], dep.findDepsFromNotAllowedHosts())
    self.assertEqual(frozenset(['example.com']), dep.allowed_hosts)
    self._get_processed()

  def testDepsFromNotAllowedHostsBad(self):
    """Verifies gclient works fine with DEPS with proper allowed_hosts."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', '.DEPS.git'),
        'allowed_hosts = ["other.com"]\n'
        'deps = {\n'
        '  "bar": "svn://example.com/bar",\n'
        '}')
    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    obj.RunOnDeps('None', [])
    dep = obj.dependencies[0]
    self.assertEqual(frozenset(['other.com']), dep.allowed_hosts)
    self.assertEqual([dep.dependencies[0]], dep.findDepsFromNotAllowedHosts())
    self._get_processed()

  def testDepsParseFailureWithEmptyAllowedHosts(self):
    """Verifies gclient fails with defined but empty allowed_hosts."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', 'DEPS'),
        'allowed_hosts = []\n'
        'deps = {\n'
        '  "bar": "/bar",\n'
        '}')
    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    try:
      obj.RunOnDeps('None', [])
      self.fail()
    except gclient_utils.Error as e:
      self.assertIn('allowed_hosts must be', str(e))
    finally:
      self._get_processed()

  def testDepsParseFailureWithNonIterableAllowedHosts(self):
    """Verifies gclient fails with defined but non-iterable allowed_hosts."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', 'DEPS'),
        'allowed_hosts = None\n'
        'deps = {\n'
        '  "bar": "/bar",\n'
        '}')
    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    try:
      obj.RunOnDeps('None', [])
      self.fail()
    except gclient_utils.Error as e:
      self.assertIn('Key \'allowed_hosts\' error:', str(e))
    finally:
      self._get_processed()

  def testCreatesCipdDependencies(self):
    """Verifies that CIPD deps are created correctly."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "svn://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', 'DEPS'),
        'vars = {\n'
        '  "lemur_version": "version:1234",\n'
        '}\n'
        'deps = {\n'
        '  "bar": {\n'
        '    "packages": [{\n'
        '      "package": "lemur",\n'
        '      "version": Var("lemur_version"),\n'
        '    }],\n'
        '    "dep_type": "cipd",\n'
        '  }\n'
        '}')
    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)

    self.assertEqual(1, len(obj.dependencies))
    sol = obj.dependencies[0]
    sol._condition = 'some_condition'

    sol.ParseDepsFile()
    self.assertEqual(1, len(sol.dependencies))
    dep = sol.dependencies[0]

    self.assertIsInstance(dep, gclient.CipdDependency)
    self.assertEqual(
        'https://chrome-infra-packages.appspot.com/lemur@version:1234',
        dep.url)

  def testIgnoresCipdDependenciesWhenFlagIsSet(self):
    """Verifies that CIPD deps are ignored if --ignore-dep-type cipd is set."""
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "https://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', 'DEPS'),
        'vars = {\n'
        '  "lemur_version": "version:1234",\n'
        '}\n'
        'deps = {\n'
        '  "bar": "/bar",\n'
        '  "baz": {\n'
        '    "packages": [{\n'
        '      "package": "lemur",\n'
        '      "version": Var("lemur_version"),\n'
        '    }],\n'
        '    "dep_type": "cipd",\n'
        '  }\n'
        '}')
    options, _ = gclient.OptionParser().parse_args([])
    options.ignore_dep_type = 'cipd'
    obj = gclient.GClient.LoadCurrentConfig(options)

    self.assertEqual(1, len(obj.dependencies))
    sol = obj.dependencies[0]
    sol._condition = 'some_condition'

    sol.ParseDepsFile()
    self.assertEqual(1, len(sol.dependencies))
    dep = sol.dependencies[0]

    self.assertIsInstance(dep, gclient.GitDependency)
    self.assertEqual('https://example.com/bar', dep.url)

  def testParseDepsFile_FalseShouldSync_WithCustoms(self):
    """Only process custom_deps/hooks when should_sync is False."""
    solutions = [{
        'name':
        'chicken',
        'url':
        'https://example.com/chicken',
        'deps_file':
        '.DEPS.git',
        'custom_deps': {
            'override/foo': 'https://example.com/overridefoo@123',
            'new/foo': 'https://example.come/newfoo@123'
        },
        'custom_hooks': [{
            'name': 'overridehook',
            'pattern': '.',
            'action': ['echo', 'chicken']
        }, {
            'name': 'newhook',
            'pattern': '.',
            'action': ['echo', 'chick']
        }],
    }]
    write('.gclient', 'solutions = %s' % repr(solutions))

    deps = {
        'override/foo': 'https://example.com/override.git@bar_version',
        'notouch/foo': 'https://example.com/notouch.git@bar_version'
    }
    hooks = [{
        'name': 'overridehook',
        'pattern': '.',
        'action': ['echo', 'cow']
    }, {
        'name': 'notouchhook',
        'pattern': '.',
        'action': ['echo', 'fail']
    }]
    pre_deps_hooks = [{
        'name': 'runfirst',
        'pattern': '.',
        'action': ['echo', 'prehook']
    }]
    write(
        os.path.join('chicken', 'DEPS'), 'deps = %s\n'
        'hooks = %s\n'
        'pre_deps_hooks = %s' % (repr(deps), repr(hooks), repr(pre_deps_hooks)))

    expected_dep_names = ['override/foo', 'new/foo']
    expected_hook_names = ['overridehook', 'newhook']

    options, _ = gclient.OptionParser().parse_args([])
    client = gclient.GClient.LoadCurrentConfig(options)
    self.assertEqual(1, len(client.dependencies))
    sol = client.dependencies[0]

    sol._should_sync = False
    sol.ParseDepsFile()
    self.assertEqual(1, len(sol.pre_deps_hooks))
    six.assertCountEqual(self, expected_dep_names,
                          [d.name for d in sol.dependencies])
    six.assertCountEqual(self, expected_hook_names,
                          [h.name for h in sol._deps_hooks])

  def testParseDepsFile_FalseShouldSync_NoCustoms(self):
    """Parse DEPS when should_sync is False and no custom hooks/deps."""
    solutions = [{
        'name': 'chicken',
        'url': 'https://example.com/chicken',
        'deps_file': '.DEPS.git',
    }]
    write('.gclient', 'solutions = %s' % repr(solutions))

    deps = {
        'override/foo': 'https://example.com/override.git@bar_version',
        'notouch/foo': 'https://example.com/notouch.git@bar_version'
    }
    hooks = [{
        'name': 'overridehook',
        'pattern': '.',
        'action': ['echo', 'cow']
    }, {
        'name': 'notouchhook',
        'pattern': '.',
        'action': ['echo', 'fail']
    }]
    pre_deps_hooks = [{
        'name': 'runfirst',
        'pattern': '.',
        'action': ['echo', 'prehook']
    }]
    write(
        os.path.join('chicken', 'DEPS'), 'deps = %s\n'
        'hooks = %s\n'
        'pre_deps_hooks = %s' % (repr(deps), repr(hooks), repr(pre_deps_hooks)))

    options, _ = gclient.OptionParser().parse_args([])
    client = gclient.GClient.LoadCurrentConfig(options)
    self.assertEqual(1, len(client.dependencies))
    sol = client.dependencies[0]

    sol._should_sync = False
    sol.ParseDepsFile()
    self.assertFalse(sol.pre_deps_hooks)
    self.assertFalse(sol.dependencies)
    self.assertFalse(sol._deps_hooks)

  def testSameDirAllowMultipleCipdDeps(self):
    """Verifies gclient allow multiple cipd deps under same directory."""
    parser = gclient.OptionParser()
    options, _ = parser.parse_args([])
    obj = gclient.GClient('foo', options)
    cipd_root = gclient_scm.CipdRoot(
        os.path.join(self.root_dir, 'dir1'), 'https://example.com')
    obj.add_dependencies_and_close(
      [
        gclient.Dependency(
            parent=obj,
            name='foo',
            url='svn://example.com/foo',
            managed=None,
            custom_deps=None,
            custom_vars=None,
            custom_hooks=None,
            deps_file='DEPS',
            should_process=True,
            should_recurse=True,
            relative=False,
            condition=None,
            protocol='https',
            print_outbuf=True),
      ],
      [])
    obj.dependencies[0].add_dependencies_and_close(
      [
        gclient.CipdDependency(
            parent=obj.dependencies[0],
            name='foo',
            dep_value={'package': 'foo_package',
                       'version': 'foo_version'},
            cipd_root=cipd_root,
            custom_vars=None,
            should_process=True,
            relative=False,
            condition='fake_condition'),
        gclient.CipdDependency(
            parent=obj.dependencies[0],
            name='bar',
            dep_value={'package': 'bar_package',
                       'version': 'bar_version'},
            cipd_root=cipd_root,
            custom_vars=None,
            should_process=True,
            relative=False,
            condition='fake_condition'),
      ],
      [])
    dep0 = obj.dependencies[0].dependencies[0]
    dep1 = obj.dependencies[0].dependencies[1]
    self.assertEqual('https://example.com/foo_package@foo_version', dep0.url)
    self.assertEqual('https://example.com/bar_package@bar_version', dep1.url)

  def _testPosixpathImpl(self):
    parser = gclient.OptionParser()
    options, _ = parser.parse_args([])
    obj = gclient.GClient('src', options)
    cipd_root = obj.GetCipdRoot()

    cipd_dep = gclient.CipdDependency(
        parent=obj,
        name='src/foo/bar/baz',
        dep_value={
          'package': 'baz_package',
          'version': 'baz_version',
        },
        cipd_root=cipd_root,
        custom_vars=None,
        should_process=True,
        relative=False,
        condition=None)
    self.assertEqual(cipd_dep._cipd_subdir, 'src/foo/bar/baz')

  def testPosixpathCipdSubdir(self):
    self._testPosixpathImpl()

  # CIPD wants posixpath separators for subdirs, even on windows.
  # See crbug.com/854219.
  def testPosixpathCipdSubdirOnWindows(self):
    with mock.patch('os.path', new=ntpath), (
         mock.patch('os.sep', new=ntpath.sep)):
      self._testPosixpathImpl()

  def testFuzzyMatchUrlByURL(self):
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "https://example.com/foo.git",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "bar": "https://example.com/bar.git@bar_version",\n'
        '}')
    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    foo_sol = obj.dependencies[0]
    self.assertEqual(
        'https://example.com/foo.git',
        foo_sol.FuzzyMatchUrl(['https://example.com/foo.git', 'foo'])
    )

  def testFuzzyMatchUrlByURLNoGit(self):
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "https://example.com/foo.git",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "bar": "https://example.com/bar.git@bar_version",\n'
        '}')
    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    foo_sol = obj.dependencies[0]
    self.assertEqual(
        'https://example.com/foo',
        foo_sol.FuzzyMatchUrl(['https://example.com/foo', 'foo'])
    )

  def testFuzzyMatchUrlByName(self):
    write(
        '.gclient',
        'solutions = [\n'
        '  { "name": "foo", "url": "https://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
          ']')
    write(
        os.path.join('foo', 'DEPS'),
        'deps = {\n'
        '  "bar": "https://example.com/bar.git@bar_version",\n'
        '}')
    options, _ = gclient.OptionParser().parse_args([])
    obj = gclient.GClient.LoadCurrentConfig(options)
    foo_sol = obj.dependencies[0]
    self.assertEqual('foo', foo_sol.FuzzyMatchUrl(['foo']))

  def testEnforceSkipSyncRevisions_DepsPatchRefs(self):
    """Patch_refs for any deps removes all skip_sync_revisions."""
    write(
        '.gclient', 'solutions = [\n'
        '  { "name": "foo/src", "url": "https://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
        ']')
    write(
        os.path.join('foo/src', 'DEPS'), 'deps = {\n'
        '  "bar": "https://example.com/bar.git@bar_version",\n'
        '}')
    write(gclient.PREVIOUS_SYNC_COMMITS_FILE,
          json.dumps({'foo/src': '1234'}))
    options, _ = gclient.OptionParser().parse_args([])

    client = gclient.GClient.LoadCurrentConfig(options)
    patch_refs = {'foo/src': '1222', 'somedeps': '1111'}
    self.assertEqual({}, client._EnforceSkipSyncRevisions(patch_refs))

  def testEnforceSkipSyncRevisions_CustomVars(self):
    """Changes in a sol's custom_vars removes its revisions."""
    write(
        '.gclient', 'solutions = [\n'
        '  { "name": "samevars", "url": "https://example.com/foo",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '    "custom_vars" : { "checkout_foo": "true" },\n'
        '  },\n'
        '  { "name": "diffvars", "url": "https://example.com/chicken",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '    "custom_vars" : { "checkout_chicken": "true" },\n'
        '  },\n'
        '  { "name": "novars", "url": "https://example.com/cow",\n'
        '    "deps_file" : ".DEPS.git",\n'
        '  },\n'
        ']')
    write(
        os.path.join('samevars', 'DEPS'), 'deps = {\n'
        '  "bar": "https://example.com/bar.git@bar_version",\n'
        '}')
    write(
        os.path.join('diffvars', 'DEPS'), 'deps = {\n'
        '  "moo": "https://example.com/moo.git@moo_version",\n'
        '}')
    write(
        os.path.join('novars', 'DEPS'), 'deps = {\n'
        '  "poo": "https://example.com/poo.git@poo_version",\n'
        '}')
    previous_custom_vars = {
        'samevars': {
            'checkout_foo': 'true'
        },
        'diffvars': {
            'checkout_chicken': 'false'
        },
    }
    write(gclient.PREVIOUS_CUSTOM_VARS_FILE,
          json.dumps(previous_custom_vars))
    previous_sync_commits = {'samevars': '10001',
                             'diffvars': '10002',
                             'novars': '10003'}
    write(gclient.PREVIOUS_SYNC_COMMITS_FILE,
          json.dumps(previous_sync_commits))

    options, _ = gclient.OptionParser().parse_args([])

    patch_refs = {'samevars': '1222'}
    expected_skip_sync_revisions = {'samevars': '10001', 'novars': '10003'}

    client = gclient.GClient.LoadCurrentConfig(options)
    self.assertEqual(expected_skip_sync_revisions,
                     client._EnforceSkipSyncRevisions(patch_refs))


class MergeVarsTest(unittest.TestCase):

  def test_merge_vars(self):
    merge_vars = gclient.merge_vars
    Str = gclient_eval.ConstantString

    l = {'foo': 'bar', 'baz': True}
    merge_vars(l, {'foo': Str('quux')})
    self.assertEqual(l, {'foo': 'quux', 'baz': True})

    l = {'foo': 'bar', 'baz': True}
    merge_vars(l, {'foo': 'quux'})
    self.assertEqual(l, {'foo': 'quux', 'baz': True})

    l = {'foo': Str('bar'), 'baz': True}
    merge_vars(l, {'foo': Str('quux')})
    self.assertEqual(l, {'foo': Str('quux'), 'baz': True})

    l = {'foo': Str('bar'), 'baz': True}
    merge_vars(l, {'foo': Str('quux')})
    self.assertEqual(l, {'foo': Str('quux'), 'baz': True})

    l = {'foo': 'bar'}
    merge_vars(l, {'baz': True})
    self.assertEqual(l, {'foo': 'bar', 'baz': True})



if __name__ == '__main__':
  sys.stdout = gclient_utils.MakeFileAutoFlush(sys.stdout)
  sys.stdout = gclient_utils.MakeFileAnnotated(sys.stdout)
  sys.stderr = gclient_utils.MakeFileAutoFlush(sys.stderr)
  sys.stderr = gclient_utils.MakeFileAnnotated(sys.stderr)
  logging.basicConfig(
      level=[logging.ERROR, logging.WARNING, logging.INFO, logging.DEBUG][
        min(sys.argv.count('-v'), 3)],
      format='%(relativeCreated)4d %(levelname)5s %(module)13s('
              '%(lineno)d) %(message)s')
  unittest.main()
