# Copyright 2026 The Cobalt Authors. All rights reserved.
"""Tests for github_download.py."""

import datetime
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


class TestGitHubDownload(unittest.TestCase):
  """Unit tests for github_download functions."""

  def setUp(self):
    super().setUp()
    self.test_dir = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with

  def tearDown(self):
    self.test_dir.cleanup()
    super().tearDown()

  def test_parse_junit_xml_success(self):
    xml_content = """<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="2" failures="0" disabled="0" errors="0" time="0.01" name="AllTests">
  <testsuite name="MySuite" tests="2" failures="0" disabled="0" errors="0" time="0.01">
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
<testsuites tests="3" failures="1" disabled="0" errors="1" time="0.02" name="AllTests">
  <testsuite name="MySuite" tests="3" failures="1" disabled="0" errors="1" time="0.02">
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

  @mock.patch.object(github_download, 'download_test_results')
  @mock.patch.object(github_download, 'download_job_log')
  @mock.patch.object(github_download, 'parse_junit_xml')
  @mock.patch.object(github_download.glob, 'glob')
  def test_process_job_test_job_success(self, mock_glob, mock_parse,
                                        mock_dl_log, mock_dl_results):
    job = {'job_id': 1001, 'name': 'android-arm / arm_yts_tests'}
    mock_dl_results.return_value = True
    mock_glob.return_value = ['/tmp/fake/results.xml']
    mock_parse.return_value = [{
        'testsuite': 'S',
        'testcase': 'T',
        'message': 'msg',
        'details': 'det',
    }]

    cache_dir = os.path.join(self.test_dir.name, 'cache')
    github_download.process_job(123, job, cache_dir)

    expected_log_path = os.path.join(cache_dir, '1001.log')
    self.assertEqual(job['local_log_path'], expected_log_path)
    self.assertTrue(os.path.exists(expected_log_path))
    with open(expected_log_path, 'r', encoding='utf-8') as f:
      content = f.read()
    self.assertIn('JUnit Failure: S.T', content)
    self.assertIn('Message: msg', content)
    self.assertIn('Details:\ndet', content)

    # Should NOT call download_job_log
    mock_dl_log.assert_not_called()

  @mock.patch.object(github_download, 'download_test_results')
  @mock.patch.object(github_download, 'download_job_log')
  def test_process_job_build_job(self, mock_dl_log, mock_dl_results):
    job = {'job_id': 1002, 'name': 'chromium_android-arm / arm_gold'}

    def mock_dl_log_side_effect(job_id, dest_path):
      del job_id
      os.makedirs(os.path.dirname(dest_path), exist_ok=True)
      with open(dest_path, 'w', encoding='utf-8') as f:
        f.write('Full build log here')
      return True

    mock_dl_log.side_effect = mock_dl_log_side_effect

    cache_dir = os.path.join(self.test_dir.name, 'cache')
    github_download.process_job(123, job, cache_dir)

    expected_log_path = os.path.join(cache_dir, '1002.log')
    self.assertEqual(job['local_log_path'], expected_log_path)
    self.assertTrue(os.path.exists(expected_log_path))
    with open(expected_log_path, 'r', encoding='utf-8') as f:
      content = f.read()
    self.assertEqual(content, 'Full build log here')

    # Should NOT try to download test results because it's not a test job
    mock_dl_results.assert_not_called()

  @mock.patch.object(github_download, 'download_test_results')
  @mock.patch.object(github_download, 'download_job_log')
  @mock.patch.object(github_download.glob, 'glob')
  @mock.patch.object(github_download, 'parse_junit_xml')
  def test_process_job_test_job_no_failures_fallback(self, mock_parse,
                                                     mock_glob, mock_dl_log,
                                                     mock_dl_results):
    del mock_dl_results
    # Test case: Job contains 'test' in name, XML downloads successfully,
    # but there are 0 failures. It should fallback to full log download.
    job = {'job_id': 1003, 'name': 'android-arm / arm_yts_tests'}
    mock_glob.return_value = ['/tmp/fake/results.xml']
    mock_parse.return_value = []  # 0 failures

    def mock_dl_log_side_effect(job_id, dest_path):
      del job_id
      os.makedirs(os.path.dirname(dest_path), exist_ok=True)
      with open(dest_path, 'w', encoding='utf-8') as f:
        f.write('Full job log with crash outside unit tests')
      return True

    mock_dl_log.side_effect = mock_dl_log_side_effect

    cache_dir = os.path.join(self.test_dir.name, 'cache')
    github_download.process_job(123, job, cache_dir)

    expected_log_path = os.path.join(cache_dir, '1003.log')
    self.assertEqual(job['local_log_path'], expected_log_path)
    self.assertTrue(os.path.exists(expected_log_path))
    with open(expected_log_path, 'r', encoding='utf-8') as f:
      content = f.read()
    self.assertEqual(content, 'Full job log with crash outside unit tests')
    mock_dl_log.assert_called_once()

  @mock.patch.object(github_download, 'download_test_results')
  @mock.patch.object(github_download, 'download_job_log')
  def test_process_job_test_job_results_download_fails_fallback(
      self, mock_dl_log, mock_dl_results):
    # If download_test_results returns False, should fallback to log download
    job = {'job_id': 1004, 'name': 'android-arm / arm_yts_tests'}
    mock_dl_results.return_value = False

    def mock_dl_log_side_effect(job_id, dest_path):
      del job_id
      os.makedirs(os.path.dirname(dest_path), exist_ok=True)
      with open(dest_path, 'w', encoding='utf-8') as f:
        f.write('Full job log fallback')
      return True

    mock_dl_log.side_effect = mock_dl_log_side_effect

    cache_dir = os.path.join(self.test_dir.name, 'cache')
    github_download.process_job(123, job, cache_dir)

    expected_log_path = os.path.join(cache_dir, '1004.log')
    self.assertEqual(job['local_log_path'], expected_log_path)
    self.assertTrue(os.path.exists(expected_log_path))
    with open(expected_log_path, 'r', encoding='utf-8') as f:
      content = f.read()
    self.assertEqual(content, 'Full job log fallback')
    mock_dl_log.assert_called_once()

  @mock.patch.object(github_download, 'download_test_results')
  @mock.patch.object(github_download, 'download_job_log')
  def test_process_job_log_download_fails(self, mock_dl_log, mock_dl_results):
    del mock_dl_results
    # If both fail (or if it's a build job and log download fails),
    # local_log_path is empty string
    job = {'job_id': 1005, 'name': 'chromium_android-arm / arm_gold'}
    mock_dl_log.return_value = False

    cache_dir = os.path.join(self.test_dir.name, 'cache')
    github_download.process_job(123, job, cache_dir)

    self.assertEqual(job['local_log_path'], '')
    mock_dl_log.assert_called_once()

  @mock.patch.object(github_download.gardener_utils, 'run_gh_command')
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

  @mock.patch.object(github_download, 'process_job')
  @mock.patch.object(github_download, 'open', create=True)
  @mock.patch.object(github_download.os.path, 'exists')
  @mock.patch.object(github_download.json, 'dump')
  def test_main_skips_outdated_runs(self, mock_json_dump, mock_exists,
                                    mock_open, mock_process_job):
    fixed_now = datetime.datetime(
        2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)

    class MockDateTime(datetime.datetime):

      @classmethod
      def now(cls, tz=None):
        return fixed_now

    mock_exists.return_value = True

    mock_input_data = {
        'runs': [
            {
                'run_id': 101,
                'workflow_name': 'recent-run',
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
                'run_id': 102,
                'workflow_name': 'outdated-run',
                'event': 'push',
                'createdAt': ((fixed_now -
                               datetime.timedelta(days=10)).isoformat().replace(
                                   '+00:00', 'Z')),
                'failed_jobs': [{
                    'job_id': 2,
                    'name': 'job2'
                }],
            },
        ]
    }

    mock_file = mock.MagicMock()
    mock_file.__enter__.return_value = mock_file
    mock_open.return_value = mock_file

    with mock.patch.object(
        github_download.json,
        'load',
        return_value=mock_input_data,
    ):
      with mock.patch.object(
          github_download.datetime,
          'datetime',
          MockDateTime,
      ):
        with mock.patch(
            'sys.argv',
            ['github_download.py', 'input.json', '--output', 'output.json'],
        ):
          github_download.main()

    mock_process_job.assert_called_once_with(101, {
        'job_id': 1,
        'name': 'job1'
    }, mock.ANY)
    mock_json_dump.assert_called_once()
    written_data = mock_json_dump.call_args[0][0]
    self.assertEqual(written_data, mock_input_data)


if __name__ == '__main__':
  unittest.main()
