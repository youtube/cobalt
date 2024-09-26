# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""Unit tests for manager.py."""

import optparse
import time
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.web_tests.controllers.manager import Manager
from blinkpy.web_tests.models import test_expectations
from blinkpy.web_tests.models.test_run_results import TestRunResults


class FakePrinter(object):
    def write_update(self, s):
        pass


class ManagerTest(unittest.TestCase):
    def test_needs_servers(self):
        def get_manager():
            host = MockHost()
            port = host.port_factory.get('test-mac-mac10.10')
            manager = Manager(
                port,
                options=optparse.Values({
                    'http': True,
                    'max_locked_shards': 1
                }),
                printer=FakePrinter())
            return manager

        manager = get_manager()
        self.assertFalse(manager._needs_servers(['fast/html']))

        manager = get_manager()
        self.assertTrue(manager._needs_servers(['http/tests/misc']))

    def test_servers_started(self):
        def get_manager(port):
            manager = Manager(
                port,
                options=optparse.Values({
                    'http': True,
                    'max_locked_shards': 1
                }),
                printer=FakePrinter())
            return manager

        def start_http_server(additional_dirs, number_of_drivers):
            self.http_started = True

        def start_websocket_server():
            self.websocket_started = True

        def stop_http_server():
            self.http_stopped = True

        def stop_websocket_server():
            self.websocket_stopped = True

        host = MockHost()
        port = host.port_factory.get('test-mac-mac10.10')
        port.start_http_server = start_http_server
        port.start_websocket_server = start_websocket_server
        port.stop_http_server = stop_http_server
        port.stop_websocket_server = stop_websocket_server

        self.http_started = self.http_stopped = self.websocket_started = self.websocket_stopped = False
        manager = get_manager(port)
        manager._start_servers(['http/tests/foo.html'])
        self.assertEqual(self.http_started, True)
        self.assertEqual(self.websocket_started, False)
        manager._stop_servers()
        self.assertEqual(self.http_stopped, True)
        self.assertEqual(self.websocket_stopped, False)

        self.http_started = self.http_stopped = self.websocket_started = self.websocket_stopped = False
        manager._start_servers(['http/tests/websocket/foo.html'])
        self.assertEqual(self.http_started, True)
        self.assertEqual(self.websocket_started, True)
        manager._stop_servers()
        self.assertEqual(self.http_stopped, True)
        self.assertEqual(self.websocket_stopped, True)

        self.http_started = self.http_stopped = self.websocket_started = self.websocket_stopped = False
        manager._start_servers(['fast/html/foo.html'])
        self.assertEqual(self.http_started, False)
        self.assertEqual(self.websocket_started, False)
        manager._stop_servers()
        self.assertEqual(self.http_stopped, False)
        self.assertEqual(self.websocket_stopped, False)

    def test_look_for_new_crash_logs(self):
        def get_manager():
            host = MockHost()
            port = host.port_factory.get('test-mac-mac10.10')
            manager = Manager(
                port,
                options=optparse.Values({
                    'test_list': None,
                    'http': True,
                    'max_locked_shards': 1
                }),
                printer=FakePrinter())
            return manager

        host = MockHost()
        port = host.port_factory.get('test-mac-mac10.10')
        tests = ['failures/expected/crash.html']
        expectations = test_expectations.TestExpectations(port)
        run_results = TestRunResults(expectations, len(tests), None)
        manager = get_manager()
        manager._look_for_new_crash_logs(run_results, time.time())

    def _make_fake_test_result(self, host, results_directory):
        host.filesystem.maybe_make_directory(results_directory)
        host.filesystem.write_binary_file(results_directory + '/results.html',
                                          'This is a test results file')

    def test_rename_results_folder(self):
        host = MockHost()
        port = host.port_factory.get('test-mac-mac10.10')

        def get_manager():
            manager = Manager(
                port,
                options=optparse.Values({
                    'max_locked_shards': 1
                }),
                printer=FakePrinter())
            return manager

        self._make_fake_test_result(port.host, '/tmp/layout-test-results')
        self.assertTrue(
            port.host.filesystem.exists('/tmp/layout-test-results'))
        timestamp = time.strftime(
            '%Y-%m-%d-%H-%M-%S',
            time.localtime(
                port.host.filesystem.mtime(
                    '/tmp/layout-test-results/results.html')))
        archived_file_name = '/tmp/layout-test-results' + '_' + timestamp
        manager = get_manager()
        manager._rename_results_folder()
        self.assertFalse(
            port.host.filesystem.exists('/tmp/layout-test-results'))
        self.assertTrue(port.host.filesystem.exists(archived_file_name))

    def test_clobber_old_results(self):
        host = MockHost()
        port = host.port_factory.get('test-mac-mac10.10')

        def get_manager():
            manager = Manager(
                port,
                options=optparse.Values({
                    'max_locked_shards': 1
                }),
                printer=FakePrinter())
            return manager

        self._make_fake_test_result(port.host, '/tmp/layout-test-results')
        self.assertTrue(
            port.host.filesystem.exists('/tmp/layout-test-results'))
        manager = get_manager()
        manager._clobber_old_results()
        self.assertFalse(
            port.host.filesystem.exists('/tmp/layout-test-results'))

    def test_limit_archived_results_count(self):
        host = MockHost()
        port = host.port_factory.get('test-mac-mac10.10')

        def get_manager():
            manager = Manager(
                port,
                options=optparse.Values({
                    'max_locked_shards': 1
                }),
                printer=FakePrinter())
            return manager

        for x in range(1, 31):
            dir_name = '/tmp/layout-test-results' + '_' + str(x)
            self._make_fake_test_result(port.host, dir_name)
        manager = get_manager()
        manager._limit_archived_results_count()
        deleted_dir_count = 0
        for x in range(1, 31):
            dir_name = '/tmp/layout-test-results' + '_' + str(x)
            if not port.host.filesystem.exists(dir_name):
                deleted_dir_count = deleted_dir_count + 1
        self.assertEqual(deleted_dir_count, 5)

    def test_restore_order(self):
        host = MockHost()
        port = host.port_factory.get('test-mac-mac10.10')

        def get_manager():
            manager = Manager(port,
                              options=optparse.Values({'max_locked_shards':
                                                       1}),
                              printer=FakePrinter())
            return manager

        manager = get_manager()
        paths = [
            "external/wpt/css/css-backgrounds/animations/background-size-interpolation.html",
            "virtual/gpu/fast/canvas/CanvasFillTextWithMinimalSize.html",
            "fast/backgrounds/size/backgroundSize-*"
        ]
        # As returned by base.tests()
        test_names = [
            "virtual/gpu/fast/canvas/CanvasFillTextWithMinimalSize.html",
            "fast/backgrounds/size/backgroundSize-in-background-shorthand.html",
            "fast/backgrounds/size/backgroundSize-viewportPercentage-width.html",
            "external/wpt/css/css-backgrounds/animations/background-size-interpolation.html"
        ]
        expected_order = [
            "external/wpt/css/css-backgrounds/animations/background-size-interpolation.html",
            "virtual/gpu/fast/canvas/CanvasFillTextWithMinimalSize.html",
            "fast/backgrounds/size/backgroundSize-in-background-shorthand.html",
            "fast/backgrounds/size/backgroundSize-viewportPercentage-width.html"
        ]
        test_names_restored = manager._restore_order(paths, test_names)
        self.assertEqual(expected_order, test_names_restored)
