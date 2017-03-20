#!/usr/bin/env python

'''
    As is tests for installApp.py prior to refactoring,

    just going for characterization, not completeness
'''

import unittest
import os
from mock import Mock

# fake the DM, since it doesn't exist in repo land
# taken from http://code.activestate.com/recipes/82234-importing-a
# -dynamically-generated-module/
import sys
import imp


def make_fake_module(name):
    module = imp.new_module(name)
    exec '' in module.__dict__
    sys.modules[name] = module
    return module

# now add the mocks we need to the dm
devicemanagerSUT = make_fake_module('devicemanagerSUT')

# the_mock is the instance of DeviceManagerSUT class that will be
# instantiated. By setting it as the return value, no other mocking is
# done in the DeviceManagerSUT class.
the_mock = Mock()
devicemanagerSUT.DeviceManagerSUT = Mock(return_value=the_mock)

# since sut_lib methods are imported directly, we'll create the fake
# module, then add mocks for the cited routines
sut_lib = make_fake_module('sut_lib')
sut_lib = Mock()

# And the mocks we need from sut_lib
import sut_lib
# the routines we import by name from sut_lib, each need their own
# mock, so they are present when installApp imports them by name
sut_lib.getOurIP = Mock()
sut_lib.calculatePort = Mock()
sut_lib.clearFlag = Mock()
sut_lib.setFlag = Mock()
sut_lib.checkDeviceRoot = Mock(return_value='/fake_root')
sut_lib.getDeviceTimestamp = Mock()
sut_lib.setDeviceTimestamp = Mock()
sut_lib.getResolution = Mock(return_value=[1024, 768])
sut_lib.waitForDevice = Mock()
sut_lib.runCommand = Mock()

import time

import installApp


class InstallAppTestCase(unittest.TestCase):
    def setUp(self):
        # every test should reset the counts
        the_mock.reset_mock()
        # we want to ensure a "success" from the installApp method
        the_mock.installApp.return_value = None
        # don't exercise find code by default
        os.path.exists = Mock(return_value=True)
        # we don't really want to wait during testing
        time.sleep = Mock()


class CheckBasicExecution(InstallAppTestCase):
    def setUp(self):
        super(CheckBasicExecution, self).setUp()
        os.environ['SUT_NAME'] = 'fred'

    def test_errors_no_args(self):
        # TypeError is a side effect of being run in
        # not-the-normal-foopy environment.
        self.assertRaises(TypeError, installApp.main)

    def test_error_short_args(self):
        # we expect the script to exit via sys.exit(1)
        try:
            # since a "normal" or "clean" script exits via sys.exit(0),
            # we need to catch the exception, and inspect the return
            # code (i.e.  we can't use the normal "assertRaises"
            # approach)
            installApp.main(['app', 1])
        except SystemExit as e:
            self.assertEqual(1, e.code)
        else:
            self.fail('should have exited via sys.exit(1)')

    def test_two_args_okay(self):
        installApp.main(['app', 1, '/path/to/source_file'])

    def test_three_args_okay(self):
        installApp.main(['app', 1, '/path/to/source_file', 3])


class CheckArguementHandling(InstallAppTestCase):
    # N.B. the 3rd arguement to installApp.py is never used, so can
    # not be tested

    def testIPAddress(self):
        for ip in ('hostname', '255.255.255.255'):
            installApp.main(['app', ip, 'path/to/something'])
            devicemanagerSUT.DeviceManagerSUT.assert_called_with(ip)

    def testSourceFileName(self):
        root_path = sut_lib.checkDeviceRoot()
        for source_file in ['test_1', 'test_2']:
            src_path = os.path.join('/path/to', source_file)
            installed_path = os.path.join(root_path, source_file)
            expected_args = ((installed_path,), {})
            installApp.main(['app', 1, src_path])
            self.assertTrue(
                expected_args in the_mock.installApp.call_args_list)


class CheckOriginalContract(InstallAppTestCase):
    def setUp(self):
        super(CheckOriginalContract, self).setUp()
        the_mock.reset_mock()

    # bummer expectedFailure decorator is not supported before py 2.7
    # unittest, so just disable the test
    # @unittest.expectedFailure
    def XXXtestOneSetOfCalls(self):
        root_path = sut_lib.checkDeviceRoot()
        source_file = 'testing'
        source_path = os.path.join('/path/to', source_file)
        installed_path = os.path.join(root_path, source_file)
        installApp.main(['app', 1, source_path])
        the_mock.pushFile.assert_called_once_with(source_path, installed_path)
        the_mock.installApp.assert_called_once_with(installed_path)


class CheckNewContract(InstallAppTestCase):

    def test_robocop_found(self):
        root_path = sut_lib.checkDeviceRoot()
        source_file = 'fennec.eggs.arm.apk'
        robocop_file = 'robocop.apk'
        source_path = os.path.join('build/', source_file)
        robocop_source_path = os.path.join('build/tests/bin', robocop_file)
        installed_path = os.path.join(root_path, source_file)
        robocop_installed_path = os.path.join(root_path, robocop_file)
        expected_pushFile_calls = [((source_path, installed_path), {}),
                                   ((robocop_source_path, robocop_installed_path), {})]
        expected_installApp_calls = [((installed_path,), {}),
                                     ((robocop_installed_path,), {})]

        installApp.main(['app', 1, source_path])

        self.assertEqual(the_mock.installApp.call_args_list,
                         expected_installApp_calls)
        self.assertEqual(the_mock.pushFile.call_args_list,
                         expected_pushFile_calls)

    def test_robocop_not_found(self):
        root_path = sut_lib.checkDeviceRoot()
        source_file = 'fennec.eggs.arm.apk'
        source_path = os.path.join('build/', source_file)
        installed_path = os.path.join(root_path, source_file)
        expected_pushFile_calls = [((source_path, installed_path), {}), ]
        expected_installApp_calls = [((installed_path,), {}), ]

        # simulate not finding robocop.apk - should not attempt install
        # then
        os.path.exists.return_value = False
        installApp.main(['app', 1, source_path])

        self.assertEqual(the_mock.installApp.call_args_list,
                         expected_installApp_calls)
        self.assertEqual(the_mock.pushFile.call_args_list,
                         expected_pushFile_calls)


class CheckRobocopFind(InstallAppTestCase):

    def test_null_path_if_not_found(self):
        os.path.exists.return_value = False
        found_path = installApp.find_robocop()
        self.assertFalse(found_path,
                         "Should not have found robocop.apk")

    def test_correct_path_if_found(self):
        os.path.exists.return_value = True
        found_path = installApp.find_robocop()
        self.assertTrue(found_path,
                        "Should have found robocop.apk")

if __name__ == '__main__':
    unittest.main()
