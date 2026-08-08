# Copyright 2026 The Cobalt Authors. All rights reserved.
# TAG=agy
"""Tests for combined github_download.py (discovery + download)."""

import datetime
import json
import os
import sys
import tempfile
import unittest
from unittest import mock

_scripts_dir = os.path.dirname(os.path.abspath(__file__))
if _scripts_dir not in sys.path:
  sys.path.append(_scripts_dir)
# pylint: disable=wrong-import-position
import github_download


class TestGitHubDiscover(unittest.TestCase):
  """Unit tests for discovery functions now in github_download."""

  def setUp(self):
    super().setUp()
    # pylint: disable=consider-using-with
    self.test_dir = tempfile.TemporaryDirectory()

  def tearDown(self):
    self.test_dir.cleanup()
    super().tearDown()

  def test_parse_build_status(self):
    content = (
        '# Build Status\n'
        '## Main\n'
        '| Platform      | Build Status                             |'
        ' Nightly Build                                            |\n'
        '| :-------------| :----------------------------------------|'
        ' :--------------------------------------------------------|\n'
        '| **android**   | [![Status][android-badge]][android-link] |'
        ' [![Status][android-nightly-badge]][android-nightly-link] |\n'
        '\n'
        '[android-badge]: https://github.com/youtube/cobalt/actions/'
        'workflows/android.yaml/badge.svg?event=push&branch=main\n'
        '[android-nightly-badge]: https://github.com/youtube/cobalt/'
        'actions/workflows/android.yaml/badge.svg?'
        'event=workflow_dispatch&branch=main\n'
        '[android-link]: https://github.com/youtube/cobalt/actions/'
        'workflows/android.yaml?query=event%3Apush+branch%3Amain\n'
        '\n'
        '### 27.lts\n'
        '| Platform      | Build Status                             |\n'
        '| :-------------| :----------------------------------------|\n'
        '| **android**   | [![Status][android-27-badge]][android-27-link] |\n'
        '\n'
        '[android-27-badge]: https://github.com/youtube/cobalt/actions/'
        'workflows/android_27.lts.yaml/badge.svg?event=push&branch=27.lts\n'
        '[android-27-link]: https://github.com/youtube/cobalt/actions/'
        'workflows/android_27.lts.yaml?query=event%3Apush+branch%3A27.lts\n')
    path = os.path.join(self.test_dir.name, 'BUILD_STATUS.md')
    with open(path, 'w', encoding='utf-8') as f:
      f.write(content)

    workflows = github_download.parse_build_status(path)

    expected = [
        {
            'workflow': 'android.yaml',
            'event': 'push',
            'branch': 'main'
        },
        {
            'workflow': 'android.yaml',
            'event': 'workflow_dispatch',
            'branch': 'main',
        },
        {
            'workflow': 'android_27.lts.yaml',
            'event': 'push',
            'branch': '27.lts',
        },
    ]
    self.assertEqual(workflows, expected)

  @mock.patch.object(github_download, 'run_gh_command')
  def test_discover_runs(self, mock_gh):
    now = datetime.datetime.now(datetime.timezone.utc)
    recent_date = (now - datetime.timedelta(hours=2)).isoformat().replace('+00:00', 'Z')
    workflows = [
        {
            'workflow': 'android.yaml',
            'event': 'push',
            'branch': 'main'
        },
        {
            'workflow': 'linux.yaml',
            'event': 'push',
            'branch': 'main'
        },
    ]

    run_android = [{
        'databaseId': 123,
        'conclusion': 'failure',
        'status': 'completed',
        'url': 'url1',
        'createdAt': recent_date
    }]
    jobs_android = {
        'jobs': [{
            'databaseId': 1,
            'name': 'build',
            'conclusion': 'failure',
            'url': 'joburl1'
        }, {
            'databaseId': 2,
            'name': 'test',
            'conclusion': 'success',
            'url': 'joburl2'
        }]
    }

    run_linux = [{
        'databaseId': 456,
        'conclusion': 'success',
        'status': 'completed',
        'url': 'url2',
        'createdAt': recent_date
    }]
    jobs_linux = {
        'jobs': [{
            'databaseId': 3,
            'name': 'build',
            'conclusion': 'success',
            'url': 'joburl3'
        }]
    }

    def mock_run_gh_side_effect(args):
      if 'run' in args and 'list' in args:
        if 'android.yaml' in args:
          return json.dumps(run_android)
        elif 'linux.yaml' in args:
          return json.dumps(run_linux)
      elif 'run' in args and 'view' in args:
        if '123' in args:
          return json.dumps(jobs_android)
        elif '456' in args:
          return json.dumps(jobs_linux)
      return '[]'

    mock_gh.side_effect = mock_run_gh_side_effect

    results = github_download.discover_runs(workflows)

    self.assertEqual(results['total_jobs_fetched'], 2)
    self.assertEqual(
        len(results['runs']),
        2)  # Both are returned, but linux is success (no failed jobs)

    # Verify android run (failed)
    android_run = next(r for r in results['runs'] if r['run_id'] == '123')
    self.assertEqual(android_run['job_name'], 'android.yaml')
    self.assertEqual(len(android_run['failed_jobs']), 1)
    self.assertEqual(android_run['failed_jobs'][0]['job_id'], 1)
    self.assertEqual(android_run['conclusion'], 'failure')

    # Verify linux run (success)
    linux_run = next(r for r in results['runs'] if r['run_id'] == '456')
    self.assertEqual(linux_run['job_name'], 'linux.yaml')
    self.assertEqual(len(linux_run['failed_jobs']), 0)
    self.assertEqual(linux_run['conclusion'], 'success')

    # Check that gh run view was NOT called for 456
    view_456_call = mock.call(
        ['run', 'view', '456', '--json', 'jobs', '-R', 'youtube/cobalt'])
    self.assertNotIn(view_456_call, mock_gh.call_args_list)

  @mock.patch.object(github_download, 'run_gh_command')
  def test_discover_run_by_id(self, mock_gh):
    jobs_data = {
        'databaseId':
            123,
        'workflowName':
            'android.yaml',
        'headBranch':
            'main',
        'event':
            'push',
        'createdAt':
            '2026-07-22T08:00:00Z',
        'url':
            'url1',
        'conclusion':
            'failure',
        'jobs': [{
            'databaseId': 1,
            'name': 'build',
            'conclusion': 'failure',
            'url': 'joburl1'
        }, {
            'databaseId': 2,
            'name': 'test',
            'conclusion': 'success',
            'url': 'joburl2'
        }]
    }
    mock_gh.return_value = json.dumps(jobs_data)

    results = github_download.discover_run_by_id(123)

    self.assertEqual(results['total_jobs_fetched'], 2)
    self.assertEqual(len(results['runs']), 1)
    run = results['runs'][0]
    self.assertEqual(run['run_id'], '123')
    self.assertEqual(run['job_name'], 'android.yaml')
    self.assertEqual(run['conclusion'], 'failure')
    self.assertEqual(len(run['failed_jobs']), 1)
    self.assertEqual(run['failed_jobs'][0]['job_id'], 1)
    self.assertTrue(run['ignore_age'])


class TestGitHubDownload(unittest.TestCase):
  """Unit tests for download functions in github_download."""

  def setUp(self):
    super().setUp()
    # pylint: disable=consider-using-with
    self.test_dir = tempfile.TemporaryDirectory()

  def tearDown(self):
    self.test_dir.cleanup()
    super().tearDown()

  def test_parse_junit_xml_success(self):
    xml_content = """<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="2" failures="0" disabled="0" errors="0"
            time="0.01" name="AllTests">
  <testsuite name="MySuite" tests="2" failures="0" disabled="0"
             errors="0" time="0.01">
    <testcase name="Test1" status="run" time="0.005" classname="MySuite" />
    <testcase name="Test2" status="run" time="0.005" classname="MySuite" />
  </testsuite>
</testsuites>
"""
    xml_path = os.path.join(self.test_dir.name, 'test_success.xml')
    with open(xml_path, 'w', encoding='utf-8') as f:
      f.write(xml_content)

    failures = github_download.parse_junit_xml(xml_path)
    self.assertEqual(failures, [])

  def test_parse_junit_xml_failures(self):
    xml_content = """<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="3" failures="1" disabled="0" errors="1"
            time="0.02" name="AllTests">
  <testsuite name="MySuite" tests="3" failures="1" disabled="0"
             errors="1" time="0.02">
    <testcase name="Test1" status="run" time="0.005" classname="MySuite">
      <failure message="assertion failed">expected 1 but got 2</failure>
    </testcase>
    <testcase name="Test2" status="run" time="0.005" classname="MySuite" />
    <testcase name="Test3" status="run" time="0.01" classname="MySuite">
      <error message="runtime error">null pointer exception</error>
    </testcase>
  </testsuite>
</testsuites>
"""
    xml_path = os.path.join(self.test_dir.name, 'test_failures.xml')
    with open(xml_path, 'w', encoding='utf-8') as f:
      f.write(xml_content)

    failures = github_download.parse_junit_xml(xml_path)
    self.assertEqual(len(failures), 2)
    self.assertEqual(failures[0]['testsuite'], 'MySuite')
    self.assertEqual(failures[0]['testcase'], 'Test1')
    self.assertEqual(failures[0]['message'], 'assertion failed')
    self.assertEqual(failures[0]['details'], 'expected 1 but got 2')

    self.assertEqual(failures[1]['testsuite'], 'MySuite')
    self.assertEqual(failures[1]['testcase'], 'Test3')
    self.assertEqual(failures[1]['message'], 'runtime error')
    self.assertEqual(failures[1]['details'], 'null pointer exception')

  @mock.patch.object(github_download, 'download_job_log')
  def test_process_job_test_job_success(self, mock_dl_log):
    job = {'job_id': 1001, 'name': 'android-arm / arm_yts_tests'}
    run_temp_dir = os.path.join(self.test_dir.name, 'run_artifacts')
    os.makedirs(run_temp_dir)

    xml_content = """<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="1" failures="1" disabled="0" errors="0"
            time="0.01" name="AllTests">
  <testsuite name="S" tests="1" failures="1" disabled="0" errors="0"
             time="0.01">
    <testcase name="T" status="run" time="0.005" classname="S">
      <failure message="msg">det</failure>
    </testcase>
  </testsuite>
</testsuites>
"""
    xml_path = os.path.join(run_temp_dir, 'results.xml')
    with open(xml_path, 'w', encoding='utf-8') as f:
      f.write(xml_content)

    cache_dir = os.path.join(self.test_dir.name, 'cache')
    os.makedirs(cache_dir, exist_ok=True)

    github_download.process_job(job, cache_dir, run_temp_dir)

    expected_log_path = os.path.join(cache_dir, '1001.log')
    self.assertEqual(job['local_log_path'], expected_log_path)
    self.assertTrue(os.path.exists(expected_log_path))
    with open(expected_log_path, 'r', encoding='utf-8') as f:
      content = f.read()
    self.assertIn('JUnit Failure: S.T', content)
    self.assertIn('Message: msg', content)
    self.assertIn('Details:\ndet', content)
    self.assertEqual(job['log_type'], 'synthetic')
    mock_dl_log.assert_not_called()

  @mock.patch.object(github_download, 'download_job_log')
  def test_process_job_build_job(self, mock_dl_log):
    job = {'job_id': 1002, 'name': 'chromium_android-arm / arm_gold'}

    def mock_dl_log_side_effect(job_id, dest_path):
      del job_id
      os.makedirs(os.path.dirname(dest_path), exist_ok=True)
      with open(dest_path, 'w', encoding='utf-8') as f:
        f.write('Full build log here')
      return True

    mock_dl_log.side_effect = mock_dl_log_side_effect
    cache_dir = os.path.join(self.test_dir.name, 'cache')
    os.makedirs(cache_dir, exist_ok=True)

    github_download.process_job(job, cache_dir, '/dummy/dir')

    expected_log_path = os.path.join(cache_dir, '1002.log')
    self.assertEqual(job['local_log_path'], expected_log_path)
    self.assertTrue(os.path.exists(expected_log_path))
    with open(expected_log_path, 'r', encoding='utf-8') as f:
      content = f.read()
    self.assertEqual(content, 'Full build log here')
    mock_dl_log.assert_called_once()
    self.assertEqual(job['log_type'], 'gha_log')

  @mock.patch.object(github_download, 'download_job_log')
  def test_process_job_test_job_no_failures_fallback(self, mock_dl_log):
    job = {'job_id': 1003, 'name': 'android-arm / arm_yts_tests'}
    run_temp_dir = os.path.join(self.test_dir.name, 'run_artifacts_empty')
    os.makedirs(run_temp_dir)

    def mock_dl_log_side_effect(job_id, dest_path):
      del job_id
      os.makedirs(os.path.dirname(dest_path), exist_ok=True)
      with open(dest_path, 'w', encoding='utf-8') as f:
        f.write('Full job log with crash outside unit tests')
      return True

    mock_dl_log.side_effect = mock_dl_log_side_effect
    cache_dir = os.path.join(self.test_dir.name, 'cache')
    os.makedirs(cache_dir, exist_ok=True)

    github_download.process_job(job, cache_dir, run_temp_dir)

    expected_log_path = os.path.join(cache_dir, '1003.log')
    self.assertEqual(job['local_log_path'], expected_log_path)
    self.assertTrue(os.path.exists(expected_log_path))
    with open(expected_log_path, 'r', encoding='utf-8') as f:
      content = f.read()
    self.assertEqual(content, 'Full job log with crash outside unit tests')
    mock_dl_log.assert_called_once()
    self.assertEqual(job['device_logs_status'], 'MISSING')
    self.assertEqual(job['log_type'], 'gha_log')

  @mock.patch.object(github_download, 'download_job_log')
  def test_process_job_test_job_results_download_fails_fallback(
      self, mock_dl_log):
    job = {'job_id': 1004, 'name': 'android-arm / arm_yts_tests'}

    def mock_dl_log_side_effect(job_id, dest_path):
      del job_id
      os.makedirs(os.path.dirname(dest_path), exist_ok=True)
      with open(dest_path, 'w', encoding='utf-8') as f:
        f.write('Full job log fallback')
      return True

    mock_dl_log.side_effect = mock_dl_log_side_effect
    cache_dir = os.path.join(self.test_dir.name, 'cache')
    os.makedirs(cache_dir, exist_ok=True)

    github_download.process_job(job, cache_dir, None)

    expected_log_path = os.path.join(cache_dir, '1004.log')
    self.assertEqual(job['local_log_path'], expected_log_path)
    self.assertTrue(os.path.exists(expected_log_path))
    with open(expected_log_path, 'r', encoding='utf-8') as f:
      content = f.read()
    self.assertEqual(content, 'Full job log fallback')
    mock_dl_log.assert_called_once()
    self.assertEqual(job['device_logs_status'], 'MISSING')
    self.assertEqual(job['log_type'], 'gha_log')

  @mock.patch.object(github_download, 'download_job_log')
  def test_process_job_log_download_fails(self, mock_dl_log):
    job = {'job_id': 1005, 'name': 'chromium_android-arm / arm_gold'}
    mock_dl_log.return_value = False

    cache_dir = os.path.join(self.test_dir.name, 'cache')
    os.makedirs(cache_dir, exist_ok=True)

    github_download.process_job(job, cache_dir, None)

    self.assertEqual(job['local_log_path'], '')
    mock_dl_log.assert_called_once()

  @mock.patch.object(github_download, 'run_gh_command')
  def test_download_job_log_success(self, mock_run_gh):
    mock_run_gh.return_value = 'Log content'
    dest_path = os.path.join(self.test_dir.name, 'job.log')
    result = github_download.download_job_log(9999, dest_path)
    self.assertTrue(result)
    self.assertTrue(os.path.exists(dest_path))
    with open(dest_path, 'r', encoding='utf-8') as f:
      self.assertEqual(f.read(), 'Log content')
    mock_run_gh.assert_called_once_with(
        ['api', 'repos/youtube/cobalt/actions/jobs/9999/logs'])

  @mock.patch.object(github_download, 'download_job_log')
  def test_process_job_target_log_extraction(self, mock_dl_log):
    del mock_dl_log
    job = {'job_id': 2001, 'name': 'android-arm64 / skia:skia_unittests'}
    run_temp_dir = os.path.join(self.test_dir.name, 'run_artifacts_target')
    platform_dir = os.path.join(run_temp_dir, 'android-arm64', '1')
    os.makedirs(platform_dir)

    with open(
        os.path.join(platform_dir, 'skia_unittests_log.txt'),
        'w',
        encoding='utf-8') as f:
      f.write('Skia test log content')
    with open(
        os.path.join(platform_dir, 'skia_unittests_device_logcat.txt'),
        'w',
        encoding='utf-8') as f:
      f.write('Skia device logcat content')

    cache_dir = os.path.join(self.test_dir.name, 'cache')
    os.makedirs(cache_dir, exist_ok=True)

    github_download.process_job(job, cache_dir, run_temp_dir)

    expected_log_path = os.path.join(cache_dir, '2001.log')
    expected_system_log_path = os.path.join(cache_dir, '2001_system_log.txt')

    self.assertEqual(job['local_log_path'], expected_log_path)
    self.assertEqual(job['device_system_log_path'], expected_system_log_path)
    self.assertEqual(job['log_type'], 'test_log')

    self.assertTrue(os.path.exists(expected_log_path))
    with open(expected_log_path, 'r', encoding='utf-8') as f:
      self.assertEqual(f.read(), 'Skia test log content')

    self.assertTrue(os.path.exists(expected_system_log_path))
    with open(expected_system_log_path, 'r', encoding='utf-8') as f:
      self.assertEqual(f.read(), 'Skia device logcat content')

  def test_process_job_cached_logs(self):
    cache_dir = os.path.join(self.test_dir.name, 'cache_existing')
    os.makedirs(cache_dir, exist_ok=True)

    # Scenario A: Both exist -> log_type should be 'test_log'
    job_a = {'job_id': 3001, 'name': 'android-arm64 / skia:skia_unittests'}
    log_path_a = os.path.join(cache_dir, '3001.log')
    system_log_path_a = os.path.join(cache_dir, '3001_system_log.txt')
    with open(log_path_a, 'w', encoding='utf-8') as f:
      f.write('Some test output')
    with open(system_log_path_a, 'w', encoding='utf-8') as f:
      f.write('Some system log')

    github_download.process_job(job_a, cache_dir, None)
    self.assertEqual(job_a['local_log_path'], log_path_a)
    self.assertEqual(job_a['device_system_log_path'], system_log_path_a)
    self.assertEqual(job_a['log_type'], 'test_log')

    # Scenario B: Job log starts with JUnit Failure -> 'synthetic'
    job_b = {'job_id': 3002, 'name': 'android-arm / arm_yts_tests'}
    log_path_b = os.path.join(cache_dir, '3002.log')
    with open(log_path_b, 'w', encoding='utf-8') as f:
      f.write('JUnit Failure: SomeSuite.SomeTest\nDetails...')

    github_download.process_job(job_b, cache_dir, None)
    self.assertEqual(job_b['local_log_path'], log_path_b)
    self.assertNotIn('device_system_log_path', job_b)
    self.assertEqual(job_b['log_type'], 'synthetic')

    # Scenario C: Job log is standard GHA log -> 'gha_log'
    job_c = {'job_id': 3003, 'name': 'linux_compilation'}
    log_path_c = os.path.join(cache_dir, '3003.log')
    with open(log_path_c, 'w', encoding='utf-8') as f:
      f.write('Standard GHA compilation log output')

    github_download.process_job(job_c, cache_dir, None)
    self.assertEqual(job_c['local_log_path'], log_path_c)
    self.assertNotIn('device_system_log_path', job_c)
    self.assertEqual(job_c['log_type'], 'gha_log')

  @mock.patch.object(github_download, 'discover_runs')
  @mock.patch.object(github_download, 'parse_build_status')
  @mock.patch.object(github_download, 'download_test_results')
  @mock.patch.object(github_download, 'process_job')
  @mock.patch.object(github_download.os.path, 'exists')
  # pylint: disable=too-many-positional-arguments
  def test_main_skips_outdated_runs(
      self,
      mock_exists,
      mock_process_job,
      mock_dl_results,
      mock_parse_status,
      mock_discover,
  ):
    mock_exists.return_value = True
    fixed_now = datetime.datetime(
        2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)

    class MockDateTime(datetime.datetime):

      @classmethod
      def now(cls, tz=None):
        return fixed_now

    mock_dl_results.return_value = True
    mock_parse_status.return_value = []

    mock_discovered_data = {
        'total_jobs_fetched':
            3,
        'runs': [
            {
                'run_id': '101',
                'job_name': 'recent-run',
                'branch': 'main',
                'event': 'push',
                'createdAt': ((fixed_now -
                               datetime.timedelta(days=2)).isoformat().replace(
                                   '+00:00', 'Z')),
                'failed_jobs': [{
                    'job_id': 1,
                    'name': 'job1'
                }],
            },
            {
                'run_id': '102',
                'job_name': 'outdated-run',
                'branch': 'main',
                'event': 'push',
                'createdAt': ((fixed_now -
                               datetime.timedelta(days=10)).isoformat().replace(
                                   '+00:00', 'Z')),
                'failed_jobs': [{
                    'job_id': 2,
                    'name': 'job2'
                }],
            },
            {
                'run_id': '103',
                'job_name': 'outdated-ignore-age-run',
                'branch': 'main',
                'event': 'push',
                'createdAt': ((fixed_now -
                               datetime.timedelta(days=10)).isoformat().replace(
                                   '+00:00', 'Z')),
                'ignore_age': True,
                'failed_jobs': [{
                    'job_id': 3,
                    'name': 'job3'
                }],
            },
        ]
    }
    mock_discover.return_value = mock_discovered_data

    output_path = os.path.join(self.test_dir.name, 'output.json')
    test_cache_dir = os.path.join(self.test_dir.name, 'cache')

    with mock.patch.object(github_download.datetime, 'datetime', MockDateTime):
      with mock.patch(
          'sys.argv',
          [
              'github_download.py', '--output', output_path, '--cache-dir',
              test_cache_dir
          ],
      ):
        github_download.main()

    # Process job should be called for run 101 (job1) and 103 (job3), but NOT
    # for 102 (job2).
    mock_process_job.assert_has_calls([
        mock.call({
            'job_id': 1,
            'name': 'job1'
        }, mock.ANY, mock.ANY),
        mock.call({
            'job_id': 3,
            'name': 'job3'
        }, mock.ANY, mock.ANY),
    ],
                                      any_order=True)
    self.assertEqual(mock_process_job.call_count, 2)

    # Verify written file
    self.assertTrue(os.path.exists(output_path))
    with open(output_path, 'r', encoding='utf-8') as f:
      written_data = json.load(f)
    self.assertEqual(written_data['runs'][0]['run_id'], '101')
    self.assertEqual(written_data['runs'][1]['run_id'], '102')
    self.assertEqual(written_data['runs'][2]['run_id'], '103')
    self.assertEqual(written_data['source'], 'github')


class TestGitHubDownloadUtils(unittest.TestCase):
  """Unit tests for utility functions folded into github_download."""

  def test_check_run_age(self):
    now = datetime.datetime(2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)

    # 1. Recent nightly (23h ago) -> not outdated
    run_recent_nightly = {
        'createdAt': ((now - datetime.timedelta(hours=23)).isoformat().replace(
            '+00:00', 'Z')),
        'event': 'schedule',
    }
    is_outdated, age_str = github_download.check_run_age(
        run_recent_nightly, now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, '23 hour(s) ago')

    # 2. Outdated nightly (25h ago) -> outdated
    run_outdated_nightly = {
        'createdAt': ((now - datetime.timedelta(hours=25)).isoformat().replace(
            '+00:00', 'Z')),
        'event': 'schedule',
    }
    is_outdated, age_str = github_download.check_run_age(
        run_outdated_nightly, now)
    self.assertTrue(is_outdated)
    self.assertEqual(age_str, '1 day(s) ago')

    # 3. Recent postsubmit (6 days ago) -> not outdated
    run_recent_postsubmit = {
        'createdAt': ((now - datetime.timedelta(days=6)).isoformat().replace(
            '+00:00', 'Z')),
        'event': 'push',
    }
    is_outdated, age_str = github_download.check_run_age(
        run_recent_postsubmit, now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, '6 day(s) ago')

    # 4. Outdated postsubmit (8 days ago) -> outdated
    run_outdated_postsubmit = {
        'createdAt': ((now - datetime.timedelta(days=8)).isoformat().replace(
            '+00:00', 'Z')),
        'event': 'push',
    }
    is_outdated, age_str = github_download.check_run_age(
        run_outdated_postsubmit, now)
    self.assertTrue(is_outdated)
    self.assertEqual(age_str, '8 day(s) ago')

  def test_check_run_age_missing_created_at(self):
    now = datetime.datetime(2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)
    run = {'event': 'push'}
    is_outdated, age_str = github_download.check_run_age(run, now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, 'unknown age')

  def test_check_run_age_invalid_created_at(self):
    now = datetime.datetime(2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)
    run = {'createdAt': 'invalid-date-string', 'event': 'push'}
    # Redirect stderr to avoid cluttering test output
    with mock.patch('sys.stderr') as mock_stderr:
      is_outdated, age_str = github_download.check_run_age(run, now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, 'unknown age')
    mock_stderr.write.assert_called()

  def test_check_run_age_missing_event_defaults_to_push(self):
    now = datetime.datetime(2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)
    run_recent = {
        'createdAt': ((now - datetime.timedelta(days=6)).isoformat().replace(
            '+00:00', 'Z'))
    }
    is_outdated, age_str = github_download.check_run_age(run_recent, now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, '6 day(s) ago')

    run_outdated = {
        'createdAt': ((now - datetime.timedelta(days=8)).isoformat().replace(
            '+00:00', 'Z'))
    }
    is_outdated, age_str = github_download.check_run_age(run_outdated, now)
    self.assertTrue(is_outdated)
    self.assertEqual(age_str, '8 day(s) ago')

  def test_check_run_age_naive_created_at(self):
    now = datetime.datetime(2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)
    run = {'createdAt': '2026-07-14T12:00:00', 'event': 'push'}
    is_outdated, age_str = github_download.check_run_age(run, now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, '6 day(s) ago')

  @mock.patch.object(github_download.subprocess, 'run')
  def test_run_gh_command_success(self, mock_run):
    mock_response = mock.MagicMock()
    mock_response.returncode = 0
    mock_response.stdout = 'some output'
    mock_run.return_value = mock_response

    output = github_download.run_gh_command(['api', 'repos/youtube/cobalt'])
    self.assertEqual(output, 'some output')
    mock_run.assert_called_once_with(
        ['gh', 'api', 'repos/youtube/cobalt'],
        capture_output=True,
        text=True,
        check=False,
    )

  @mock.patch.object(github_download.subprocess, 'run')
  def test_run_gh_command_failure(self, mock_run):
    mock_response = mock.MagicMock()
    mock_response.returncode = 1
    mock_response.stderr = 'some error'
    mock_run.return_value = mock_response

    with self.assertRaises(Exception) as context:
      github_download.run_gh_command(['invalid'])
    self.assertIn('gh command failed: some error', str(context.exception))


class TestProcessRun(unittest.TestCase):
  """Tests for process_run function."""

  def setUp(self):
    super().setUp()
    # pylint: disable=consider-using-with
    self.test_dir = tempfile.TemporaryDirectory()

  def tearDown(self):
    self.test_dir.cleanup()
    super().tearDown()

  @mock.patch.object(github_download, 'download_test_results')
  @mock.patch.object(github_download, 'process_job')
  def test_process_run_no_test_jobs(self, mock_process_job,
                                    mock_download):
    run_data = {
        'jobs': [
            {
                'job_id': 1,
                'name': 'build_only'
            },
            {
                'job_id': 2,
                'name': 'compile_only'
            },
        ]
    }
    github_download.process_run('101', run_data, self.test_dir.name)

    mock_download.assert_not_called()
    self.assertEqual(mock_process_job.call_count, 2)
    mock_process_job.assert_has_calls([
        mock.call(run_data['jobs'][0], self.test_dir.name, None),
        mock.call(run_data['jobs'][1], self.test_dir.name, None),
    ])

  @mock.patch.object(github_download, 'download_test_results')
  @mock.patch.object(github_download, 'process_job')
  @mock.patch.object(github_download, 'is_job_cached')
  def test_process_run_all_test_jobs_cached(self, mock_is_cached,
                                            mock_process_job, mock_download):
    run_data = {'jobs': [{'job_id': 1, 'name': 'android-arm / arm_yts_tests'},]}
    mock_is_cached.return_value = True

    github_download.process_run('101', run_data, self.test_dir.name)

    mock_is_cached.assert_called_once_with(1, self.test_dir.name)
    mock_download.assert_not_called()
    mock_process_job.assert_called_once_with(run_data['jobs'][0],
                                             self.test_dir.name, None)

  @mock.patch.object(github_download, 'download_test_results')
  @mock.patch.object(github_download, 'process_job')
  @mock.patch.object(github_download, 'is_job_cached')
  def test_process_run_uncached_test_job(self, mock_is_cached, mock_process_job,
                                         mock_download):
    run_data = {'jobs': [{'job_id': 1, 'name': 'android-arm / arm_yts_tests'},]}
    mock_is_cached.return_value = False
    mock_download.return_value = True

    github_download.process_run('101', run_data, self.test_dir.name)

    mock_is_cached.assert_called_once_with(1, self.test_dir.name)
    mock_download.assert_called_once_with('101', mock.ANY)
    mock_process_job.assert_called_once()
    args, _ = mock_process_job.call_args
    self.assertEqual(args[0], run_data['jobs'][0])
    self.assertEqual(args[1], self.test_dir.name)
    self.assertIsNotNone(args[2])  # run_temp_dir


class TestParseJobName(unittest.TestCase):
  """Tests for parse_job_name function."""

  def test_standard_test_job(self):
    is_test, platform, target = github_download.parse_job_name(
        'android-arm / arm_yts_tests')
    self.assertTrue(is_test)
    self.assertEqual(platform, 'android-arm')
    self.assertEqual(target, None)

  def test_test_job_with_target(self):
    is_test, platform, target = github_download.parse_job_name(
        'android-arm64 / skia:skia_unittests')
    self.assertTrue(is_test)
    self.assertEqual(platform, 'android-arm64')
    self.assertEqual(target, 'skia_unittests')

  def test_build_only_job(self):
    is_test, platform, target = github_download.parse_job_name('build_only')
    self.assertFalse(is_test)
    self.assertEqual(platform, None)
    self.assertEqual(target, None)

  def test_test_in_name_no_slashes(self):
    is_test, platform, target = github_download.parse_job_name('run_unit_tests')
    self.assertTrue(is_test)
    self.assertEqual(platform, None)
    self.assertEqual(target, None)


class TestIsJobCached(unittest.TestCase):
  """Tests for is_job_cached function."""

  def setUp(self):
    super().setUp()
    # pylint: disable=consider-using-with
    self.test_dir = tempfile.TemporaryDirectory()

  def tearDown(self):
    self.test_dir.cleanup()
    super().tearDown()

  def test_job_not_cached(self):
    self.assertFalse(github_download.is_job_cached(123, self.test_dir.name))

  def test_job_cached_empty_file(self):
    log_path = os.path.join(self.test_dir.name, '123.log')
    with open(log_path, 'a', encoding='utf-8'):
      pass
    self.assertFalse(github_download.is_job_cached(123, self.test_dir.name))

  def test_job_cached_with_content(self):
    log_path = os.path.join(self.test_dir.name, '123.log')
    with open(log_path, 'w', encoding='utf-8') as f:
      f.write('log content')
    self.assertTrue(github_download.is_job_cached(123, self.test_dir.name))


if __name__ == '__main__':
  unittest.main()
