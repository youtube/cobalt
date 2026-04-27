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
        self.mock_run.return_value = "Everything is up-to-date"

        self.mock_sub_run = mock.patch("subprocess.run").start()
        self.mock_sub_run.return_value = mock.MagicMock(returncode=0)

        # Mock filesystem and environment
        mock.patch("os.path.exists", return_value=True).start()
        mock.patch("pathlib.Path.exists", return_value=True).start()
        mock.patch("pathlib.Path.mkdir").start()
        mock.patch("pathlib.Path.unlink").start()
        mock.patch("os.remove").start()
        mock.patch("os.makedirs").start()
        mock.patch.dict(os.environ, {"RDK_DEVICE_ID": "test_device_123"}).start()
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
        self.mock_run.side_effect = ["", "", "linking...", "", "", "", "", ""]

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
        self.mock_run.side_effect = ["", "", "linking...", "", "", "", "", ""]

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
        self.mock_run.side_effect = ["", "", "linking...", "", "", "", "", ""]

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

    def test_executable_mode_remote_dir(self):
        """Verifies executable mode uses the correct remote directory."""
        self.mock_run.side_effect = ["", "", "linking...", "", "", "", "", ""]

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
        self.mock_run.side_effect = ["", "", "linking...", "", "", "", "", ""]

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


if __name__ == "__main__":
    unittest.main()
