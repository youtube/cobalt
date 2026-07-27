# Copyright 2026 The Cobalt Authors. All rights reserved.
"""Tests for kokoro_download.py."""

import os
import shutil
import tempfile
import unittest
from unittest import mock
import subprocess

import kokoro_download


class TestKokoroDownload(unittest.TestCase):
  """Tests for Kokoro download and triage utilities."""

  def setUp(self):
    super().setUp()
    self.test_dir = tempfile.mkdtemp()

    # Override cache dir for test isolation
    self.cache_dir = os.path.join(self.test_dir, "cache")
    self.cache_patcher = mock.patch("kokoro_download.CACHE_DIR", self.cache_dir)
    self.cache_patcher.start()

  def tearDown(self):
    self.cache_patcher.stop()
    shutil.rmtree(self.test_dir)
    super().tearDown()

  @mock.patch("kokoro_download.fetch_sponge_log")
  @mock.patch("kokoro_download.get_child_actions")
  def test_process_child_jobs(self, mock_get_actions, mock_fetch_log):
    # Create a temp parent log file containing a child link
    parent_log_path = os.path.join(self.test_dir, "parent_build.log")
    with open(parent_log_path, "w", encoding="utf-8") as f:
      f.write(
          "Link to child: https://sponge.corp.google.com/invocation?id=c99999\n"
      )

    # Mock child actions
    mock_get_actions.return_value = [{
        "target_id":
            "on_device_test_target",
        "status":
            "FAILED",
        "files": [{
            "uid": "webDriverTestLog.ERROR",
            "uri": "googlefile:/cns/path/webDriverTestLog.ERROR",
        }],
    }]
    # Mock fetch_sponge_log to return a dummy path
    mock_fetch_log.return_value = "/dummy/path/webDriverTestLog.ERROR"

    child_jobs = kokoro_download.process_child_jobs(parent_log_path, "main")

    self.assertEqual(len(child_jobs), 1)
    self.assertEqual(child_jobs[0]["job_name"], "on_device_test_target")
    self.assertEqual(child_jobs[0]["conclusion"], "failure")
    self.assertEqual(child_jobs[0]["local_log_path"],
                     "/dummy/path/webDriverTestLog.ERROR")

    # Verify calls
    mock_get_actions.assert_called_once_with("c99999")
    mock_fetch_log.assert_called_once_with("c99999", "webDriverTestLog.ERROR")

  @mock.patch("kokoro_download.process_child_jobs")
  @mock.patch("kokoro_download.fetch_sponge_log")
  @mock.patch("kokoro_download.get_latest_build_status")
  def test_triage_job(self, mock_get_status, mock_fetch_log,
                      mock_process_child):
    job = {"job_name": "cobalt/main/build/linux/nightly", "branch": "main"}

    # Mock status to be failed
    mock_get_status.return_value = {
        "status": "FAILED",
        "build_id": "b12345",
        "createdAt": "2026-07-22T08:00:00Z"
    }
    # Mock fetch_sponge_log to return a dummy path
    mock_fetch_log.return_value = "/dummy/path/build.log"
    # Mock process_child_jobs to return some child jobs
    mock_process_child.return_value = [{
        "job_name": "on_device_test_target",
        "conclusion": "failure",
        "url": "https://sponge.corp.google.com/invocation?id=c99999",
        "local_log_path": "/dummy/path/webDriverTestLog.ERROR",
        "branch": "main"
    }]

    info = kokoro_download.triage_job(job)

    self.assertIsNotNone(info)
    self.assertEqual(info["job_name"], "cobalt/main/build/linux/nightly")
    self.assertEqual(info["run_id"], "b12345")
    self.assertEqual(info["local_log_path"], "/dummy/path/build.log")
    self.assertEqual(len(info["child_jobs"]), 1)
    self.assertEqual(info["child_jobs"][0]["job_name"], "on_device_test_target")

    # Verify calls
    mock_get_status.assert_called_once_with("cobalt/main/build/linux/nightly")
    mock_fetch_log.assert_called_once_with("b12345", "build.log")
    mock_process_child.assert_called_once_with("/dummy/path/build.log", "main")

  # pylint: disable=protected-access
  def test_validation_invocation_id(self):
    self.assertTrue(kokoro_download._validate_invocation_id("b12345"))
    self.assertTrue(kokoro_download._validate_invocation_id("some-id_123"))
    self.assertFalse(kokoro_download._validate_invocation_id("id; rm -rf /"))
    self.assertFalse(kokoro_download._validate_invocation_id("id with spaces"))

  # pylint: disable=protected-access
  def test_validation_filename(self):
    self.assertTrue(kokoro_download._validate_filename("build.log"))
    self.assertTrue(kokoro_download._validate_filename("logs/test.log"))
    self.assertFalse(kokoro_download._validate_filename("../build.log"))
    self.assertFalse(kokoro_download._validate_filename("/abs/path/build.log"))
    self.assertFalse(
        kokoro_download._validate_filename("build.log; drop table"))

  @mock.patch("kokoro_download.run_command")
  def test_discover_jobs_cli_success(self, mock_run):
    mock_run.return_value = mock.Mock(
        stdout=("devtools/kokoro/config/prod/cobalt/main/build/linux/"
                "nightly.gcl\n"
                "devtools/kokoro/config/prod/cobalt/27_lts/build/linux/"
                "nightly.gcl\n"))
    jobs = kokoro_download.discover_jobs()
    self.assertEqual(len(jobs), 2)
    self.assertEqual(jobs[0]["job_name"], "cobalt/main/build/linux/nightly")
    self.assertEqual(jobs[0]["branch"], "main")
    self.assertEqual(jobs[1]["job_name"], "cobalt/27_lts/build/linux/nightly")
    self.assertEqual(jobs[1]["branch"], "27_lts")

  @mock.patch("kokoro_download.run_command")
  @mock.patch("os.path.exists")
  @mock.patch("glob.glob")
  def test_discover_jobs_cli_fallback(self, mock_glob, mock_exists, mock_run):
    mock_run.side_effect = subprocess.CalledProcessError(1, "cs")
    mock_exists.return_value = True
    mock_glob.return_value = [
        "/google/src/files/head/depot/google3/devtools/kokoro/"
        "config/prod/cobalt/main/build/linux/nightly.gcl"
    ]
    jobs = kokoro_download.discover_jobs()
    self.assertEqual(len(jobs), 1)
    self.assertEqual(jobs[0]["job_name"], "cobalt/main/build/linux/nightly")
    self.assertEqual(jobs[0]["branch"], "main")

  @mock.patch("kokoro_download.run_command")
  def test_get_latest_build_status_cli(self, mock_run):
    mock_run.return_value = mock.Mock(
        stdout=('{"status": "FAILURE", "build_id": "b12345", '
                '"createdAt": "2026-07-22T08:00:00Z"}'))
    status = kokoro_download.get_latest_build_status(
        "cobalt/main/build/linux/nightly")
    self.assertIsNotNone(status)
    self.assertEqual(status["status"], "FAILED")
    self.assertEqual(status["build_id"], "b12345")

  @mock.patch("kokoro_download.run_command")
  def test_fetch_sponge_log_cli(self, mock_run):

    def side_effect(cmd, timeout=None):  # pylint: disable=unused-argument
      if "stubby" in cmd:
        return mock.Mock(
            stdout=('{"files": [{"uid": "build.log", '
                    '"uri": "googlefile:/cns/vq-d/home/kokoro-dedicated/'
                    "jenkins/spongeV2/prod/cobalt/main/build/123/456/"
                    'b12345/build.log"}]}'))
      elif "fileutil" in cmd and "cp" in cmd:
        temp_path = cmd[-1]
        os.makedirs(os.path.dirname(temp_path), exist_ok=True)
        with open(temp_path, "w", encoding="utf-8") as f:
          f.write("mock log content")
        return mock.Mock(stdout="")
      return mock.Mock(stdout="")

    mock_run.side_effect = side_effect
    path = kokoro_download.fetch_sponge_log("b12345", "build.log")
    self.assertIsNotNone(path)
    self.assertTrue(os.path.exists(path))
    with open(path, "r", encoding="utf-8") as f:
      self.assertEqual(f.read(), "mock log content")

  @mock.patch("kokoro_download.run_command")
  def test_get_child_actions_cli_json_lines(self, mock_run):
    mock_run.return_value = mock.Mock(
        stdout=('{"actions": [{"targetId": "t1", '
                '"statusAttributes": {"status": "FAILED"}, '
                '"files": [{"uid": "test.log", "uri": "googlefile:/path"}]}]}'
                "\n"
                '{"actions": [{"targetId": "t2", '
                '"statusAttributes": {"status": "SUCCESS"}, "files": []}]}'))
    actions = kokoro_download.get_child_actions("b12345")
    self.assertEqual(len(actions), 2)
    self.assertEqual(actions[0]["target_id"], "t1")
    self.assertEqual(actions[0]["status"], "FAILED")
    self.assertEqual(actions[1]["target_id"], "t2")
    self.assertEqual(actions[1]["status"], "SUCCESS")

  def test_get_cache_path(self):
    path = kokoro_download.get_cache_path("test.log")
    self.assertEqual(path, os.path.join(self.cache_dir, "test.log"))

  def test_get_cache_path_traversal_prevention(self):
    # Path traversal attempt should be stripped to basename
    path = kokoro_download.get_cache_path("../../../etc/passwd")
    self.assertEqual(path, os.path.join(self.cache_dir, "passwd"))

  def test_ensure_cache_dir(self):
    if os.path.exists(self.cache_dir):
      shutil.rmtree(self.cache_dir)
    self.assertFalse(os.path.exists(self.cache_dir))
    self.assertTrue(kokoro_download.ensure_cache_dir())
    self.assertTrue(os.path.exists(self.cache_dir))

  @mock.patch("os.makedirs")
  def test_ensure_cache_dir_failure(self, mock_makedirs):
    mock_makedirs.side_effect = OSError("Permission denied")
    self.assertFalse(kokoro_download.ensure_cache_dir())

  @mock.patch("kokoro_download.run_command")
  def test_get_latest_build_status_cli_error(self, mock_run):
    mock_run.side_effect = subprocess.CalledProcessError(1, "stubby")
    status = kokoro_download.get_latest_build_status(
        "cobalt/main/build/linux/nightly")
    self.assertIsNone(status)

  @mock.patch("kokoro_download.run_command")
  def test_get_latest_build_status_invalid_json(self, mock_run):
    mock_run.return_value = mock.Mock(stdout="invalid json")
    status = kokoro_download.get_latest_build_status(
        "cobalt/main/build/linux/nightly")
    self.assertIsNone(status)

  @mock.patch("kokoro_download.run_command")
  def test_fetch_sponge_log_cli_fallback(self, mock_run):

    def side_effect(cmd, timeout=None):  # pylint: disable=unused-argument
      if "stubby" in cmd:
        raise subprocess.CalledProcessError(1, "stubby")
      elif "fileutil" in cmd and "ls" in cmd:
        return mock.Mock(
            stdout=("/cns/vq-d/home/kokoro-dedicated/jenkins/spongeV2/prod/"
                    "cobalt/main/build/123/456/b12345/build.log\n"))
      elif "fileutil" in cmd and "cp" in cmd:
        temp_path = cmd[-1]
        os.makedirs(os.path.dirname(temp_path), exist_ok=True)
        with open(temp_path, "w", encoding="utf-8") as f:
          f.write("mock log content from legacy")
        return mock.Mock(stdout="")
      return mock.Mock(stdout="")

    mock_run.side_effect = side_effect
    path = kokoro_download.fetch_sponge_log("b12345", "build.log")
    self.assertIsNotNone(path)
    self.assertTrue(os.path.exists(path))
    with open(path, "r", encoding="utf-8") as f:
      self.assertEqual(f.read(), "mock log content from legacy")

  @mock.patch("kokoro_download.run_command")
  def test_fetch_sponge_log_cli_fallback_not_found_in_resultstore(
      self, mock_run):

    def side_effect(cmd, timeout=None):  # pylint: disable=unused-argument
      if "stubby" in cmd:
        return mock.Mock(stdout='{"files": []}')
      elif "fileutil" in cmd and "ls" in cmd:
        return mock.Mock(
            stdout=("/cns/vq-d/home/kokoro-dedicated/jenkins/spongeV2/prod/"
                    "cobalt/main/build/123/456/b12345/build.log\n"))
      elif "fileutil" in cmd and "cp" in cmd:
        temp_path = cmd[-1]
        os.makedirs(os.path.dirname(temp_path), exist_ok=True)
        with open(temp_path, "w", encoding="utf-8") as f:
          f.write("mock log content from legacy")
        return mock.Mock(stdout="")
      return mock.Mock(stdout="")

    mock_run.side_effect = side_effect
    path = kokoro_download.fetch_sponge_log("b12345", "build.log")
    self.assertIsNotNone(path)
    self.assertTrue(os.path.exists(path))
    with open(path, "r", encoding="utf-8") as f:
      self.assertEqual(f.read(), "mock log content from legacy")


if __name__ == "__main__":
  unittest.main()
