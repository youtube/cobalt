#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Tests for the RDK deployment script.

This module provides unit tests for deploy_rdk.py using mocks to verify
the construction of shell commands and the execution flow without
requiring actual hardware or builds.
"""

import importlib.util
import os
from pathlib import Path
import sys
import subprocess
import unittest
from unittest import mock

# Dynamically import the script to test since it's not in the python path.
_SCRIPT_DIR = Path(__file__).parent
_DEPLOY_RDK_PATH = _SCRIPT_DIR / "deploy_rdk.py"

spec = importlib.util.spec_from_file_location("deploy_rdk", str(_DEPLOY_RDK_PATH))
deploy_rdk = importlib.util.module_from_spec(spec)
if spec.loader:
    spec.loader.exec_module(deploy_rdk)


class TestDeployRdk(unittest.TestCase):
    """Unit tests for the deploy_rdk script."""

    def setUp(self):
        """Sets up mocks for all system-level calls."""
        super().setUp()
        self.mock_run = mock.patch.object(deploy_rdk, "run_command").start()
        def run_cmd_side_effect(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else str(cmd)
            if "cat /version.txt" in cmd_str:
                return "imagename: RDK_V6\ncustom version: RDK_V6_AH212_20260420\n"
            return "Everything is up-to-date"
        self.mock_run.side_effect = run_cmd_side_effect

        self.mock_sub_run = mock.patch("subprocess.run").start()
        def sub_run_side_effect(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
            if "adb devices" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="List of devices attached\ntest_device\tdevice\n")
            elif "cat /etc/device.properties" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="MODEL_NAME=AH212\n")
            elif "cat /version.txt" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="custom version: RDK_V6_AH212_20260420\n")
            elif "readlink /usr/lib/libloader_app.so" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="/data/out_cobalt/libloader_app.so\n")
            return mock.MagicMock(returncode=0, stdout="")
        self.mock_sub_run.side_effect = sub_run_side_effect

        # Mock filesystem and environment
        mock.patch("time.sleep").start()
        mock.patch("os.path.exists", return_value=True).start()
        mock.patch("pathlib.Path.exists", return_value=True).start()
        mock.patch("pathlib.Path.mkdir").start()
        mock.patch("pathlib.Path.unlink").start()
        mock.patch("os.remove").start()
        mock.patch("os.makedirs").start()
        self.mock_exit = mock.patch("sys.exit").start()

    def tearDown(self):
        """Cleans up all active mocks."""
        mock.patch.stopall()
        super().tearDown()

    def test_reset_only(self):
        """Verifies --reset flag runs rdkDisplay remove and exits early."""
        with mock.patch("sys.argv", ["deploy_rdk.py", "--reset"]):
            deploy_rdk.main()

        found_reset = any(
            "rdkDisplay remove" in str(call) for call in self.mock_run.call_args_list
        )
        self.assertTrue(found_reset, "rdkDisplay remove was not called for --reset")

    def test_plugin_mode_packaging(self):
        """Verifies targets and tar command for plugin mode."""
        self.mock_run.side_effect = lambda *args, **kwargs: ""

        argv = ["deploy_rdk.py", "--mode", "plugin", "--force-deploy"]
        with mock.patch("sys.argv", argv):
            deploy_rdk.main()

        # Check targets built: cobalt_loader, loader_app and loader_app_rdk_plugin
        build_call = next(call for call in self.mock_run.call_args_list 
                         if "autoninja" in str(call))
        targets = build_call[0][0]
        self.assertIn("cobalt_loader", targets)
        self.assertIn("loader_app", targets)
        self.assertIn("loader_app_rdk_plugin", targets)

        # Check tar command: robust flag order -czvf, -T <deps_file>, then -C <out_dir>
        tar_call = next(call for call in self.mock_run.call_args_list 
                       if "tar" in str(call))
        tar_args = tar_call[0][0]
        self.assertIn("-czvf", tar_args)
        self.assertIn("-T", tar_args)
        self.assertIn("-C", tar_args)
        self.assertIn("cobalt_loader.runtime_deps", str(tar_args))
        self.assertIn("libloader_app.so", tar_args)

    def test_executable_mode_packaging(self):
        """Verifies targets and tar command for executable mode."""
        self.mock_run.side_effect = lambda *args, **kwargs: ""

        argv = ["deploy_rdk.py", "--mode", "executable", "--force-deploy"]
        with mock.patch("sys.argv", argv):
            deploy_rdk.main()

        # Check targets built: cobalt_loader and loader_app
        build_call = next(call for call in self.mock_run.call_args_list 
                         if "autoninja" in str(call))
        targets = build_call[0][0]
        self.assertIn("cobalt_loader", targets)
        self.assertIn("loader_app", targets)
        self.assertNotIn("loader_app_rdk_plugin", targets)

        # Check tar command: robust flag order -czvf, -T <deps_file>, then -C <out_dir>
        tar_call = next(call for call in self.mock_run.call_args_list 
                       if "tar" in str(call))
        tar_args = tar_call[0][0]
        self.assertIn("-czvf", tar_args)
        self.assertIn("-T", tar_args)
        self.assertIn("-C", tar_args)
        self.assertIn("cobalt_loader.runtime_deps", str(tar_args))
        self.assertNotIn("libloader_app.so", tar_args)

    def test_plugin_mode_launch(self):
        """Verifies plugin mode uses deactivate -> sleep -> activate sequence."""
        self.mock_run.side_effect = lambda *args, **kwargs: ""

        argv = ["deploy_rdk.py", "--mode", "plugin", "--run", "--force-deploy"]
        with mock.patch("sys.argv", argv):
            deploy_rdk.main()

        full_remote_cmd = ""
        for call_args in self.mock_run.call_args_list:
            cmd_arg = str(call_args[0][0])
            if "Controller.1.activate" in cmd_arg:
                full_remote_cmd = cmd_arg
                break

        self.assertIn("Controller.1.deactivate", full_remote_cmd)
        self.assertIn("Controller.1.activate", full_remote_cmd)
        self.assertIn("YouTube", full_remote_cmd)
        self.assertIn("bash -l -c", full_remote_cmd)
        self.assertIn("/data/out_cobalt", full_remote_cmd)

    def test_plugin_mode_deeplink_launch(self):
        """Verifies plugin mode activates YouTube and executes YouTube.deeplink JSON-RPC call."""
        def run_cmd_mock(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else str(cmd)
            if "cat /version.txt" in cmd_str:
                return "imagename: RDK_V6\ncustom version: RDK_V6_AH212_20260420\n"
            elif "Controller.1.configuration@YouTube" in cmd_str:
                return '{"jsonrpc":"2.0","id":1,"result":{"url":"https://www.youtube.com/tv","sbmainargs":[]}}'
            return "Everything is up-to-date"

        self.mock_run.side_effect = run_cmd_mock

        argv = ["deploy_rdk.py", "--mode", "plugin", "--run", "--force-deploy", "--deeplink", "v=dQw4w9WgXcQ"]
        with mock.patch("sys.argv", argv):
            deploy_rdk.main()

        deeplink_call = next((call for call in self.mock_run.call_args_list 
                             if "YouTube.deeplink" in str(call)), None)
        self.assertIsNotNone(deeplink_call)
        self.assertIn("v=dQw4w9WgXcQ", str(deeplink_call[0][0]))

        activate_call = next((call for call in self.mock_run.call_args_list 
                             if "Controller.1.activate" in str(call)), None)
        self.assertIsNotNone(activate_call)

    def test_deeplink_in_executable_mode_fails(self):
        """Verifies --deeplink in executable mode fails and exits."""
        argv = ["deploy_rdk.py", "--mode", "executable", "--run", "--deeplink", "v=123"]
        with mock.patch("sys.argv", argv):
            deploy_rdk.main()

        self.mock_exit.assert_called_once_with(1)

    def test_executable_mode_remote_dir(self):
        """Verifies executable mode uses the correct remote directory."""
        self.mock_run.side_effect = lambda *args, **kwargs: ""

        argv = ["deploy_rdk.py", "--mode", "executable", "--run", "--force-deploy"]
        with mock.patch("sys.argv", argv):
            deploy_rdk.main()

        found_executable_dir = False
        for call_args in self.mock_run.call_args_list:
            cmd_arg = str(call_args[0][0])
            if "/data/out_loader_app_executable" in cmd_arg:
                found_executable_dir = True

        self.assertTrue(
            found_executable_dir,
            "Executable mode did not use /data/out_loader_app_executable",
        )

    def test_only_lib_flag(self):
        """Verifies that --only-lib pushes the lz4 file to correct folder."""
        self.mock_run.side_effect = lambda *args, **kwargs: ""

        argv = ["deploy_rdk.py", "--only-lib", "--force-deploy"]
        with mock.patch("sys.argv", argv):
            deploy_rdk.main()

        found_remote_dir = False
        found_push = False
        for call_args in self.mock_run.call_args_list:
            cmd = call_args[0][0]
            cmd_str = str(cmd)
            if "mkdir -p /data/out_cobalt/app/cobalt/lib" in cmd_str:
                found_remote_dir = True
            if (
                "push" in cmd_str
                and "libcobalt.lz4" in cmd_str
                and "/data/out_cobalt/app/cobalt/lib" in cmd_str
            ):
                found_push = True

        self.assertTrue(found_remote_dir, "Did not create remote lib directory")
        self.assertTrue(found_push, "Did not push libcobalt.lz4 to correct folder")

    def test_revert_c25(self):
        """Verifies --revert-c25 runs chCobalt c25 and reboot -f."""
        self.mock_run.side_effect = None
        self.mock_run.return_value = "/data/out_cobalt/libloader_app.so"
        with mock.patch("sys.argv", ["deploy_rdk.py", "--revert-c25"]):
            deploy_rdk.main()

        calls = [str(c) for c in self.mock_run.call_args_list]
        self.assertTrue(any("chCobalt c25" in c for c in calls), "chCobalt c25 not called")
        self.assertTrue(any("reboot -f" in c for c in calls), "reboot -f not called")

    def test_no_rbe_flag(self):
        """Verifies --no-rbe flag passes --no-rbe to GN."""
        self.mock_run.side_effect = lambda *args, **kwargs: ""
        with mock.patch.dict(os.environ, {"RDK_HOME": "/mock/rdk/home"}):
            with mock.patch("sys.argv", ["deploy_rdk.py", "--no-rbe", "--force-deploy"]):
                deploy_rdk.main()

        gn_call = next(c for c in self.mock_run.call_args_list if "gn.py" in str(c))
        self.assertIn("--no-rbe", gn_call[0][0])


class TestDeployRdkDeviceDetection(unittest.TestCase):
    """Unit tests for the device auto-detection logic in deploy_rdk script."""

    def setUp(self):
        super().setUp()
        self.mock_run = mock.patch.object(deploy_rdk, "run_command").start()
        def run_cmd_side_effect(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else str(cmd)
            if "cat /version.txt" in cmd_str:
                return "imagename: RDK_V6\ncustom version: RDK_V6_AH212_20260420\n"
            elif "readlink /usr/lib/libloader_app.so" in cmd_str:
                return "/data/out_cobalt/libloader_app.so\n"
            return "Everything is up-to-date"
        self.mock_run.side_effect = run_cmd_side_effect

        self.mock_sub_run = mock.patch("subprocess.run").start()

        # Mock filesystem
        mock.patch("os.path.exists", return_value=True).start()
        mock.patch("pathlib.Path.exists", return_value=True).start()
        mock.patch("pathlib.Path.mkdir").start()
        mock.patch("pathlib.Path.unlink").start()
        mock.patch("os.remove").start()
        mock.patch("os.makedirs").start()

        # Ensure RDK_DEVICE_ID is NOT in env
        self.env_patcher = mock.patch.dict(os.environ, {}, clear=True)
        self.env_patcher.start()

        self.mock_exit = mock.patch("sys.exit").start()
        self.mock_exit.side_effect = SystemExit

    def tearDown(self):
        mock.patch.stopall()
        super().tearDown()

    def test_no_devices(self):
        adb_devices_mock = mock.MagicMock(returncode=0, stdout="List of devices attached\n")
        self.mock_sub_run.return_value = adb_devices_mock

        with mock.patch("sys.argv", ["deploy_rdk.py"]):
            with self.assertRaises(SystemExit):
                deploy_rdk.main()

        self.mock_exit.assert_called_once_with(1)

    def test_single_device(self):
        def sub_run_side_effect(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
            if "adb devices" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="List of devices attached\nonly_device\tdevice\n")
            elif "cat /etc/device.properties" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="MODEL_NAME=AH212\n")
            elif "cat /version.txt" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="custom version: RDK_V6_AH212_20260420\n")
            elif "readlink /usr/lib/libloader_app.so" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="/data/out_cobalt/libloader_app.so\n")
            return mock.MagicMock(returncode=1, stdout="")

        self.mock_sub_run.side_effect = sub_run_side_effect

        with mock.patch("sys.argv", ["deploy_rdk.py", "--reset"]):
            deploy_rdk.main()

        reset_call = next(call for call in self.mock_run.call_args_list if "adb" in str(call))
        self.assertIn("only_device", reset_call[0][0])
        self.assertEqual(deploy_rdk.get_model_name("only_device"), "AH212")

    def test_single_device_with_device_properties(self):
        def sub_run_side_effect(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
            if "adb devices" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="List of devices attached\nonly_device\tdevice\n")
            elif "cat /etc/device.properties" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="MODEL_NAME=AH212\nDEVICE_NAME=ignore_this\n")
            elif "cat /version.txt" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="custom version: RDK_V6_AH212_20260420\n")
            elif "readlink /usr/lib/libloader_app.so" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="/data/out_cobalt/libloader_app.so\n")
            return mock.MagicMock(returncode=0, stdout="")

        self.mock_sub_run.side_effect = sub_run_side_effect

        with mock.patch("sys.argv", ["deploy_rdk.py", "--reset"]):
            deploy_rdk.main()

        reset_call = next(call for call in self.mock_run.call_args_list if "adb" in str(call))
        self.assertIn("only_device", reset_call[0][0])
        self.assertEqual(deploy_rdk.get_model_name("only_device"), "AH212")

    def test_multiple_devices_one_with_rdk_properties(self):
        # dev1 and dev2. dev2 has device.properties.
        def sub_run_side_effect(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
            if "adb devices" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="List of devices attached\ndev1\tdevice\ndev2\tdevice\n")
            elif "cat /etc/device.properties" in cmd_str:
                if "dev2" in cmd_str:
                    return mock.MagicMock(returncode=0, stdout="MODEL_NAME=AH212\n")
                else:
                    return mock.MagicMock(returncode=1, stdout="")
            elif "cat /version.txt" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="custom version: RDK_V6_AH212_20260420\n")
            elif "readlink /usr/lib/libloader_app.so" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="/data/out_cobalt/libloader_app.so\n")
            return mock.MagicMock(returncode=1, stdout="")

        self.mock_sub_run.side_effect = sub_run_side_effect

        with mock.patch("sys.argv", ["deploy_rdk.py", "--reset"]):
            deploy_rdk.main()

        reset_call = next(call for call in self.mock_run.call_args_list if "adb" in str(call))
        self.assertIn("dev2", reset_call[0][0])

    def test_multiple_devices_multiple_rdk(self):
        # Both are RDK devices (device.properties exists)
        def sub_run_side_effect(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
            if "adb devices" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="List of devices attached\ndev1\tdevice\ndev2\tdevice\n")
            elif "cat /etc/device.properties" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="MODEL_NAME=AH212\n")
            elif "cat /version.txt" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="custom version: RDK_V6_AH212_20260420\n")
            elif "readlink /usr/lib/libloader_app.so" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="/data/out_cobalt/libloader_app.so\n")
            return mock.MagicMock(returncode=1, stdout="")

        self.mock_sub_run.side_effect = sub_run_side_effect

        with mock.patch("sys.argv", ["deploy_rdk.py", "--reset"]):
            deploy_rdk.main()

        reset_call = next(call for call in self.mock_run.call_args_list if "adb" in str(call))
        self.assertIn("dev1", reset_call[0][0])

    def test_multiple_devices_no_rdk(self):
        def sub_run_side_effect(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
            if "adb devices" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="List of devices attached\ndev1\tdevice\ndev2\tdevice\n")
            elif "cat /etc/device.properties" in cmd_str:
                return mock.MagicMock(returncode=1, stdout="")
            return mock.MagicMock(returncode=1, stdout="")

        self.mock_sub_run.side_effect = sub_run_side_effect

        with mock.patch("sys.argv", ["deploy_rdk.py"]):
            with self.assertRaises(SystemExit):
                deploy_rdk.main()

        self.mock_exit.assert_called_once_with(1)

    @mock.patch("time.sleep")
    def test_switch_cobalt_version_and_reboot(self, mock_sleep):
        def sub_run_mock(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
            if "adb devices" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="List of devices attached\nonly_device\tdevice\n")
            elif "cat /etc/device.properties" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="MODEL_NAME=AH212\n")
            return mock.MagicMock(returncode=0, stdout="")

        self.mock_sub_run.side_effect = sub_run_mock

        def run_cmd_mock(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else str(cmd)
            if "cat /version.txt" in cmd_str:
                return "imagename: RDK_V6\ncustom version: RDK_V6_AH212_20260420\n"
            elif "readlink /usr/lib/libloader_app.so" in cmd_str:
                return "/usr/lib/libloader_app_c25.so\n"
            elif "echo ok" in cmd_str:
                return "ok\n"
            return "Everything is up-to-date"

        self.mock_run.side_effect = run_cmd_mock

        with mock.patch.dict(os.environ, {"RDK_HOME": "/mock/rdk/home"}):
            with mock.patch("sys.argv", ["deploy_rdk.py", "--force-deploy"]):
                deploy_rdk.main()

        # Check that chCobalt custom_cobalt, reboot, and echo ok were called
        run_calls = [str(call) for call in self.mock_run.call_args_list]
        self.assertTrue(any("chCobalt custom_cobalt" in call for call in run_calls))
        self.assertTrue(any("reboot" in call for call in run_calls))
        self.assertTrue(any("echo ok" in call for call in run_calls))
        
        # Check that sleep was called to wait for reboot
        mock_sleep.assert_any_call(30)

    def test_software_version_too_old(self):
        def sub_run_side_effect(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
            if "adb devices" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="List of devices attached\nonly_device\tdevice\n")
            elif "cat /etc/device.properties" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="MODEL_NAME=AH212\n")
            elif "cat /version.txt" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="custom version: RDK_V6_AH212_20251218\n")
            return mock.MagicMock(returncode=1, stdout="")

        self.mock_sub_run.side_effect = sub_run_side_effect

        with mock.patch("sys.argv", ["deploy_rdk.py"]):
            with self.assertRaises(SystemExit):
                deploy_rdk.main()

        self.mock_exit.assert_called_once_with(1)

    def test_software_version_invalid_format(self):
        def sub_run_side_effect(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
            if "adb devices" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="List of devices attached\nonly_device\tdevice\n")
            elif "cat /etc/device.properties" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="MODEL_NAME=AH212\n")
            elif "cat /version.txt" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="custom version: RDK_V6_AH212_20269999\n")
            return mock.MagicMock(returncode=1, stdout="")

        self.mock_sub_run.side_effect = sub_run_side_effect

        with mock.patch("sys.argv", ["deploy_rdk.py"]):
            with self.assertRaises(SystemExit):
                deploy_rdk.main()

        self.mock_exit.assert_called_once_with(1)

    def test_adb_devices_fails(self):
        self.mock_sub_run.side_effect = subprocess.CalledProcessError(1, "adb devices")

        with mock.patch("sys.argv", ["deploy_rdk.py"]):
            with self.assertRaises(SystemExit):
                deploy_rdk.main()

        self.mock_exit.assert_called_once_with(1)




    @mock.patch("tempfile.TemporaryDirectory")
    def test_setup_toolchain_uses_temporary_directory(self, mock_temp_dir):
        mock_temp_dir.return_value.__enter__.return_value = "/tmp/mock_dir"
        with mock.patch.dict(os.environ, {"RDK_HOME": "/mock/rdk/home"}):
            with mock.patch("os.path.exists", return_value=False):
                with mock.patch("sys.argv", ["deploy_rdk.py", "--setup-toolchain"]):
                    deploy_rdk.main()
        mock_temp_dir.assert_called_once()

    def test_logs_streaming_handles_keyboard_interrupt(self):
        self.mock_run.side_effect = KeyboardInterrupt
        def sub_run_side_effect(cmd, *args, **kwargs):
            cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
            if "adb devices" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="List of devices attached\nonly_device\tdevice\n")
            elif "cat /etc/device.properties" in cmd_str:
                return mock.MagicMock(returncode=0, stdout="MODEL_NAME=AH212\n")
            return mock.MagicMock(returncode=0, stdout="")
        self.mock_sub_run.side_effect = sub_run_side_effect

        with mock.patch("sys.argv", ["deploy_rdk.py", "--logs"]):
            try:
                deploy_rdk.main()
            except KeyboardInterrupt:
                self.fail("KeyboardInterrupt was not caught by deploy_rdk --logs")


if __name__ == "__main__":
    unittest.main()
