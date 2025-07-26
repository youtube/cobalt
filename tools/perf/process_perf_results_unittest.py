#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
import json
import os
import shutil
import tempfile
import unittest
from unittest import mock

import six

from core import path_util
path_util.AddAndroidPylibToPath()

from pylib.utils import google_storage_helper  # pylint: disable=import-error

path_util.AddTelemetryToPath()

from telemetry import decorators

import process_perf_results as ppr_module
import json_util

UUID_SIZE = 36

BUILTIN_MODULE = '__builtin__' if six.PY2 else 'builtins'


class _FakeLogdogStream(object):

  def write(self, data):
    del data  # unused

  def close(self):
    pass

  def get_viewer_url(self):
    return 'http://foobar.not.exit'


# pylint: disable=protected-access
class DataFormatParsingUnitTest(unittest.TestCase):
  def tearDown(self):
    ppr_module._data_format_cache = {}

  def testGtest(self):
    with mock.patch(BUILTIN_MODULE + '.open', mock.mock_open(read_data='{}')):
      self.assertTrue(ppr_module._is_gtest('test.json'))
      self.assertFalse(ppr_module._is_histogram('test.json'))
    self.assertTrue(ppr_module._is_gtest('test.json'))
    self.assertFalse(ppr_module._is_histogram('test.json'))

  def testChartJSON(self):
    with mock.patch(BUILTIN_MODULE + '.open',
                    mock.mock_open(read_data='{"charts": 1}')):
      self.assertFalse(ppr_module._is_gtest('test.json'))
      self.assertFalse(ppr_module._is_histogram('test.json'))
    self.assertFalse(ppr_module._is_gtest('test.json'))
    self.assertFalse(ppr_module._is_histogram('test.json'))

  def testHistogram(self):
    with mock.patch(BUILTIN_MODULE + '.open', mock.mock_open(read_data='[]')):
      self.assertTrue(ppr_module._is_histogram('test.json'))
      self.assertFalse(ppr_module._is_gtest('test.json'))
    self.assertTrue(ppr_module._is_histogram('test.json'))
    self.assertFalse(ppr_module._is_gtest('test.json'))


class ProcessPerfResultsIntegrationTest(unittest.TestCase):
  def setUp(self):
    self.test_dir = tempfile.mkdtemp()
    self.output_json = os.path.join(self.test_dir, 'output.json')
    self.task_output_dir = os.path.join(
        os.path.dirname(__file__), 'testdata', 'task_output_dir')

    m1 = mock.patch(
        'process_perf_results.logdog_helper.text',
        return_value = 'http://foo.link')
    m1.start()
    self.addCleanup(m1.stop)

    m2 = mock.patch(
        'process_perf_results.logdog_helper.open_text',
        return_value=_FakeLogdogStream())
    m2.start()
    self.addCleanup(m2.stop)

    m3 = mock.patch('core.results_dashboard.SendResults')
    m3.start()
    self.addCleanup(m3.stop)

  def tearDown(self):
    shutil.rmtree(self.test_dir)

  @decorators.Disabled('chromeos')  # crbug.com/865800
  @decorators.Disabled('win')  # crbug.com/860677, mock doesn't integrate well
  # with multiprocessing on Windows.
  @decorators.Disabled('all')  # crbug.com/967125
  def testIntegration(self):
    build_properties = json.dumps({
        'perf_dashboard_machine_group': 'test-builder',
        'buildername': 'test-builder',
        'buildnumber': 777,
        'got_v8_revision': 'beef1234',
        'got_revision_cp': 'refs/heads/master@{#1234}',
        'got_webrtc_revision': 'fee123',
        'git_revision': 'deadbeef',
        'buildbucket': r"""{"build":
            {"bucket": "master.tryserver.chromium.perf",
             "created_by": "user:foo",
             "created_ts": "1535490272757820",
             "id": "8936915467712010816",
             "project": "chrome",
             "lease_key": "461228535",
             "tags": ["builder:android-go-perf", "buildset:patch/1194825/3",
                      "cq_experimental:False",
                      "master:master.tryserver.chromium.perf",
                      "user_agent:cq"]}}"""
        })

    output_results_dir = os.path.join(self.test_dir, 'outputresults')
    os.mkdir(output_results_dir)

    return_code, benchmark_upload_result_map = ppr_module.process_perf_results(
        self.output_json, configuration_name='test-builder',
        build_properties=build_properties,
        task_output_dir=self.task_output_dir,
        smoke_test_mode=False,
        output_results_dir=output_results_dir)

    # Output filenames are prefixed with a UUID. Strip it off.
    output_results = {
        filename[UUID_SIZE:]: os.stat(os.path.join(
            output_results_dir, filename)).st_size
        for filename in os.listdir(output_results_dir)}
    self.assertEqual(31, len(output_results))

    self.assertLess(10 << 10, output_results['power.desktop.reference'])
    self.assertLess(10 << 10, output_results['blink_perf.image_decoder'])
    self.assertLess(10 << 10, output_results['octane.reference'])
    self.assertLess(10 << 10, output_results['power.desktop'])
    self.assertLess(10 << 10, output_results['speedometer-future'])
    self.assertLess(10 << 10, output_results['blink_perf.owp_storage'])
    self.assertLess(10 << 10, output_results['memory.desktop'])
    self.assertLess(10 << 10, output_results['wasm'])
    self.assertLess(10 << 10, output_results[
      'dummy_benchmark.histogram_benchmark_1'])
    self.assertLess(10 << 10, output_results[
      'dummy_benchmark.histogram_benchmark_1.reference'])
    self.assertLess(10 << 10, output_results['wasm.reference'])
    self.assertLess(10 << 10, output_results['speedometer'])
    self.assertLess(10 << 10, output_results[
      'memory.long_running_idle_gmail_tbmv2'])
    self.assertLess(10 << 10, output_results['v8.runtime_stats.top_25'])
    self.assertLess(1 << 10, output_results[
      'dummy_benchmark.noisy_benchmark_1'])
    self.assertLess(10 << 10, output_results['blink_perf.svg'])
    self.assertLess(10 << 10, output_results[
      'v8.runtime_stats.top_25.reference'])
    self.assertLess(10 << 10, output_results['jetstream.reference'])
    self.assertLess(10 << 10, output_results['jetstream'])
    self.assertLess(10 << 10, output_results['speedometer2-future.reference'])
    self.assertLess(10 << 10, output_results['blink_perf.svg.reference'])
    self.assertLess(10 << 10, output_results[
      'blink_perf.image_decoder.reference'])
    self.assertLess(10 << 10, output_results['power.idle_platform.reference'])
    self.assertLess(10 << 10, output_results['power.idle_platform'])
    self.assertLess(1 << 10, output_results[
      'dummy_benchmark.noisy_benchmark_1.reference'])
    self.assertLess(10 << 10, output_results['speedometer-future.reference'])
    self.assertLess(10 << 10, output_results[
      'memory.long_running_idle_gmail_tbmv2.reference'])
    self.assertLess(10 << 10, output_results['memory.desktop.reference'])
    self.assertLess(10 << 10, output_results[
      'blink_perf.owp_storage.reference'])
    self.assertLess(10 << 10, output_results['octane'])
    self.assertLess(10 << 10, output_results['speedometer.reference'])

    self.assertEqual(return_code, 1)
    self.assertEqual(
        benchmark_upload_result_map,
        {
            'power.desktop.reference': True,
            'blink_perf.image_decoder': True,
            'octane.reference': True,
            'power.desktop': True,
            'speedometer-future': True,
            'blink_perf.owp_storage': True,
            'memory.desktop': True,
            'wasm': True,
            'dummy_benchmark.histogram_benchmark_1': True,
            'dummy_benchmark.histogram_benchmark_1.reference': True,
            'wasm.reference': True,
            'speedometer': True,
            'memory.long_running_idle_gmail_tbmv2': True,
            'v8.runtime_stats.top_25': True,
            'dummy_benchmark.noisy_benchmark_1': True,
            'blink_perf.svg': True,
            'v8.runtime_stats.top_25.reference': True,
            'jetstream.reference': True,
            'jetstream': True,
            'speedometer2-future.reference': True,
            'speedometer2-future':
            False,  # Only this fails due to malformed data
            'blink_perf.svg.reference': True,
            'blink_perf.image_decoder.reference': True,
            'power.idle_platform.reference': True,
            'power.idle_platform': True,
            'dummy_benchmark.noisy_benchmark_1.reference': True,
            'speedometer-future.reference': True,
            'memory.long_running_idle_gmail_tbmv2.reference': True,
            'memory.desktop.reference': True,
            'blink_perf.owp_storage.reference': True,
            'octane': True,
            'speedometer.reference': True
        })


class ProcessPerfResults_HardenedUnittest(unittest.TestCase):
  def setUp(self):
    self._logdog_text = mock.patch(
        'process_perf_results.logdog_helper.text',
        return_value = 'http://foo.link')
    self._logdog_text.start()
    self.addCleanup(self._logdog_text.stop)

    self._logdog_open_text = mock.patch(
        'process_perf_results.logdog_helper.open_text',
        return_value=_FakeLogdogStream())
    self._logdog_open_text.start()
    self.addCleanup(self._logdog_open_text.stop)

  @decorators.Disabled('chromeos')  # crbug.com/956178
  def test_handle_perf_json_test_results_IOError(self):
    directory_map = {
        'benchmark.example': ['directory_that_does_not_exist']}
    test_results_list = []
    ppr_module._handle_perf_json_test_results(directory_map, test_results_list)
    self.assertEqual(test_results_list, [])

  @decorators.Disabled('chromeos')  # crbug.com/956178
  def test_last_shard_has_no_tests(self):
    benchmark_name = 'benchmark.example'
    temp_parent_dir = tempfile.mkdtemp(suffix='test_results_outdir')
    try:
      shard1_dir = os.path.join(temp_parent_dir, 'shard1')
      os.mkdir(shard1_dir)
      shard2_dir = os.path.join(temp_parent_dir, 'shard2')
      os.mkdir(shard2_dir)
      with open(os.path.join(shard1_dir, 'test_results.json'), 'w') as fh:
        fh.write(
            '{"version": 3, "tests":{"v8.browsing_desktop-future": "blah"}}')
      with open(os.path.join(shard2_dir, 'test_results.json'), 'w') as fh:
        fh.write('{"version": 3,"tests":{}}')
      directory_map = {
          benchmark_name: [shard1_dir, shard2_dir]}
      benchmark_enabled_map = ppr_module._handle_perf_json_test_results(
          directory_map, [])
      self.assertTrue(benchmark_enabled_map[benchmark_name],
                      'Regression test for crbug.com/984565')
    finally:
      shutil.rmtree(temp_parent_dir)

  @decorators.Disabled('chromeos')  # crbug.com/956178
  def test_merge_perf_results_IOError(self):
    results_filename = None
    directories = ['directory_that_does_not_exist']
    ppr_module._merge_perf_results('benchmark.example', results_filename,
                                   directories)

  @decorators.Disabled('chromeos')  # crbug.com/956178
  def test_handle_perf_logs_no_log(self):
    tempdir = tempfile.mkdtemp()
    try:
      dir1 = os.path.join(tempdir, '1')
      dir2 = os.path.join(tempdir, '2')
      os.makedirs(dir1)
      os.makedirs(dir2)
      with open(os.path.join(dir1, 'benchmark_log.txt'), 'w') as logfile:
        logfile.write('hello world')
      directory_map = {
          'benchmark.with.log': [dir1],
          'benchmark.with.no.log': [dir2],
      }
      extra_links = {}
      ppr_module._handle_perf_logs(directory_map, extra_links)
    finally:
      shutil.rmtree(tempdir)


class ProcessPerfResults_PerfSkiaJsonUnittest(unittest.TestCase):
  @mock.patch.object(json_util, 'gcs_buckets_from_builder_name', autospec=True,
                     return_value=['chrome-perf-dashboard-test'])
  @mock.patch('builtins.open', new_callable=mock.mock_open)
  @mock.patch.object(json, 'dump', autospec=True)
  @mock.patch.object(ppr_module, '_process_skia_json', autospec=True)
  @mock.patch.object(json_util, 'get_gcs_prefix_path', autospec=True)
  @mock.patch.object(google_storage_helper, 'upload', autospec=True,
                     return_value='')
  @mock.patch.object(google_storage_helper, 'unique_name', autospec=True)
  def test_upload_skia_json(
      self,
      mock_unique_name,
      mock_upload,
      mock_get_gcs_prefix_path,
      mock_process_skia_json,
      mock_dump,
      mock_file_open,
      _):
    key = {
        'improvement_direction': 'down',
        'unit': 'ms_smallerIsBetter',
        'test': 'Editor-TipTap',
        'subtest_1': 'Speedometer3',
    }
    measurement = {
        'measurements': {
            'stat': [
                {
                    'value': 'value',
                    'measurement': 140.6900000002235
                },
                {
                    'value': 'error',
                    'measurement': 13.676537086499565
                },
                {
                    'value': 'count',
                    'measurement': 10.0
                },
                {
                    'value': 'max',
                    'measurement': 172.90000000130385
                },
                {
                    'value': 'min',
                    'measurement': 130.90000000037253
                },
                {
                    'value': 'sum',
                    'measurement': 1406.9000000022352
                },
            ]
        },
        'key': key,
    }
    links = {
        'Build Page':
        ('https://ci.chromium.org/ui/p/chrome/builders/ci/win-10-perf/39376'),
        'OS Version':
        '10.0.19045',
        'Bot Id':
        'win-222-e504',
        'Chromium Commit Position':
        'https://crrev.com/1405221',
        'V8 Git Hash': ('https://chromium.googlesource.com/v8/v8/+/'
                        '60e67b93909a1c858305b27111d9988f94fff0f8'),
        'WebRTC Git Hash': ('https://webrtc.googlesource.com/src/+/'
                            '1e19045eaa63d00a3b4017fd43c5b502c6ed73a2'),
    }
    skia_json = {
        'version': 1,
        'git_hash': 'CP:1405221',
        'key': {
            'master': 'ChromiumPerf',
            'bot': 'win-10-perf',
            'benchmark': 'speedometer3',
        },
        'results': [measurement],
        'links': links,
    }
    mock_process_skia_json.return_value = skia_json
    mock_get_gcs_prefix_path.return_value = (
        'ingest/2024/08/29/ChromiumPerf'
        '/win-11-perf/9719/benchmark.example/skia_results.json')
    mock_unique_name.return_value = ('skia_results_benchmark.example_'
                                     'win-11-perf_9719_2024_08_25_T00_39_41-UTC'
                                     '.json')
    benchmark_name = 'benchmark.example'
    configuration_name = 'win-11-perf'
    results_filename = 'results.json'
    tmpfile_dir = 'tmpfile_dir'
    build_properties = {
        'buildername': 'win-11-perf',
        'buildnumber': 9719,
        'got_revision_cp': 'refs/heads/main@{#1415171}',
        'got_v8_revision': '0f87a54dade4353b6ece1d7591ca8c66f90c1c93',
        'got_webrtc_revision': '0533b5eafe69b744f10fa178f5a6f9657eaeeb25',
        'perf_dashboard_machine_group': 'ChromiumPerf',
    }
    builder_details = json_util.PerfBuilderDetails(
        bot='win-11-perf',
        builder_page=('https://ci.chromium.org/ui/p/chrome/builders/ci/'
                      'win-11-perf/9719'),
        git_hash='CP:1415171',
        master='ChromiumPerf',
        chromium_commit_position='refs/heads/main@{#1415171}',
        v8_git_hash='0f87a54dade4353b6ece1d7591ca8c66f90c1c93',
        webrtc_git_hash='0533b5eafe69b744f10fa178f5a6f9657eaeeb25',
    )
    logdog_benchmark_dict = {
        'skia_json_conversion_failed': 'False',
        'skia_json_upload_failed': 'False',
    }
    got = ppr_module._upload_skia_json(
        benchmark_name=benchmark_name,
        configuration_name=configuration_name,
        results_filename=results_filename,
        tmpfile_dir=tmpfile_dir,
        build_properties=build_properties,
        logdog_benchmark_dict=logdog_benchmark_dict)
    self.assertEqual(got, 0)
    mock_process_skia_json.assert_called_once_with(
        results_filename=results_filename,
        builder_details=builder_details)
    mock_get_gcs_prefix_path.assert_called_once_with(
        build_properties=build_properties,
        builder_details=builder_details,
        benchmark_name=benchmark_name,
        given_datetime=None,
        filename=('skia_results_benchmark.example_'
                  'win-11-perf_9719_2024_08_25_T00_39_41-UTC.json'))
    mock_upload.assert_called_once_with(
        name=('ingest/2024/08/29/ChromiumPerf/win-11-perf/9719/'
              'benchmark.example/skia_results.json'),
        filepath='tmpfile_dir/skia_results.json',
        bucket='chrome-perf-dashboard-test',
        content_type='application/json',
        authenticated_link=True
    )
    mock_dump.assert_called_once_with(skia_json, mock.ANY)
    mock_file_open.assert_called_once_with(
        'tmpfile_dir/skia_results.json', 'w')


if __name__ == '__main__':
  unittest.main()
