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
import unittest
from unittest.mock import patch, MagicMock
import os
import sys
import importlib.util

# Dynamically import the script to test
script_path = os.path.join(os.path.dirname(__file__), 'deploy_rdk.py')
spec = importlib.util.spec_from_file_location("deploy_rdk", script_path)
deploy_rdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deploy_rdk)

class TestDeployRdk(unittest.TestCase):

    def setUp(self):
        # Setup common patches
        self.mock_run = patch.object(deploy_rdk, 'run_command').start()
        self.mock_run.return_value = "Everything is up-to-date"
        
        self.mock_sub_run = patch('subprocess.run').start()
        self.mock_sub_run.return_value = MagicMock(returncode=0)
        
        patch('os.path.exists', return_value=True).start()
        patch('os.remove').start()
        patch('os.makedirs').start()
        patch.dict(os.environ, {"RDK_DEVICE_ID": "test_device_123"}).start()
        patch('sys.exit').start()

    def tearDown(self):
        patch.stopall()

    def test_reset_only(self):
        """Verify --reset flag runs rdkDisplay remove and exits early."""
        with patch('sys.argv', ['deploy_rdk.py', '--reset']):
            deploy_rdk.main()
            
        found_reset = any("rdkDisplay remove" in str(call) for call in self.mock_run.call_args_list)
        self.assertTrue(found_reset, "rdkDisplay remove was not called for --reset")

    def test_plugin_mode_launch(self):
        """Verify plugin mode uses deactivate -> sleep -> activate sequence."""
        # Force deployment logic
        self.mock_run.side_effect = ["", "", "linking...", "", "", "", "", ""] 

        with patch('sys.argv', ['deploy_rdk.py', '--mode', 'plugin', '--run', '--force-deploy']):
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
        """Verify executable mode uses the correct remote directory."""
        self.mock_run.side_effect = ["", "", "linking...", "", "", "", "", ""]

        with patch('sys.argv', ['deploy_rdk.py', '--mode', 'executable', '--run', '--force-deploy']):
            deploy_rdk.main()

        found_executable_dir = False
        for call_args in self.mock_run.call_args_list:
            cmd_arg = str(call_args[0][0])
            if "/data/out_loader_app_executable" in cmd_arg:
                found_executable_dir = True
        
        self.assertTrue(found_executable_dir, "Executable mode did not use /data/out_loader_app_executable")

if __name__ == '__main__':
    unittest.main()
