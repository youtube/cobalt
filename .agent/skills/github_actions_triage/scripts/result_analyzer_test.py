#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All rights reserved.
"""Tests for result_analyzer.py."""

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
import result_analyzer


class TestResultAnalyzer(unittest.TestCase):
  """Unit tests for result_analyzer functions."""

  def setUp(self):
    super().setUp()
    self.test_dir = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with

  def tearDown(self):
    self.test_dir.cleanup()
    super().tearDown()

  def create_log_file(self, content):
    log_file = tempfile.NamedTemporaryFile(  # pylint: disable=consider-using-with
        mode="w+",
        suffix=".log",
        dir=self.test_dir.name,
        delete=False)
    log_file.write(content)
    log_file.close()
    return log_file.name

  def test_analyze_log_infra_error(self):
    content = "Some log line\nRunner lost communication\nAnother line"
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(matches[0]["line"], "Runner lost communication")
    self.assertEqual(matches[0]["line_num"], 2)

  def test_analyze_log_compilation_error(self):
    content = (
        "ninja: Entering directory `out/linux_devel'\n[1/1] CXX"
        " obj/test.o\ntest.cc:12: error: expected ';' before '}' token\nFAILED:"
        " obj/test.o")
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    # Both the compilation error (line 3) and siso failure (line 4) match rules
    self.assertEqual(len(matches), 2)
    self.assertEqual(matches[0]["line"],
                     "test.cc:12: error: expected ';' before '}' token")
    self.assertEqual(matches[0]["line_num"], 3)
    self.assertEqual(matches[1]["line"], "FAILED: obj/test.o")
    self.assertEqual(matches[1]["line_num"], 4)

  def test_analyze_log_ninja_error(self):
    content = (
        "[1/1] ACTION //some:action\nFAILED: obj/action_out\nerr: remote-exec"
        " failed")
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 2)
    self.assertEqual(matches[0]["line"], "FAILED: obj/action_out")
    self.assertEqual(matches[0]["line_num"], 2)
    self.assertEqual(matches[1]["line"], "err: remote-exec failed")
    self.assertEqual(matches[1]["line_num"], 3)

  def test_analyze_log_test_failure(self):
    content = "[ RUN      ] MyTest.TestOne\n[  FAILED  ] MyTest.TestOne (5 ms)"
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(matches[0]["line"], "[  FAILED  ] MyTest.TestOne (5 ms)")
    self.assertEqual(matches[0]["line_num"], 2)

  def test_analyze_log_undetermined(self):
    content = "Build finished successfully.\nAll tests passed."
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertIsNone(matches[0]["line_num"])
    self.assertEqual(matches[0]["line"], "No matching error signature found.")

  def test_analyze_log_new_infra_errors(self):
    infra_errors = [
        "Failed to fetch file gs://cobalt-static-storage/foo",
        "Unable to load AWS_CREDENTIAL_FILE",
        "E: The repository '...' buster Release' does not have a Release file.",
        ("##[error]This request has been automatically failed because it"
         " uses a deprecated version of `actions/upload-artifact: v3`."),
        "Failed to allocate any device due to your job config",
        "2026-07-09T17:26:34.3168791Z      Result: ERROR",
        "unexpected disconnect",
        "early EOF",
        "invalid index-pack output",
    ]
    for error_line in infra_errors:
      content = f"Some log line\n{error_line}\nAnother line"
      path = self.create_log_file(content)
      matches = result_analyzer.analyze_log(path)
      self.assertEqual(len(matches), 1, f"Failed to classify: {error_line}")
      self.assertEqual(matches[0]["line"], error_line.strip())
      self.assertEqual(matches[0]["line_num"], 2)

  def test_analyze_log_new_ninja_error(self):
    content = ("Some line\n2026-07-15T09:44:14.5582109Z FAILED:"
               " /__w/cobalt/cobalt/starboard/...\nAnother line")
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(
        matches[0]["line"],
        "2026-07-15T09:44:14.5582109Z FAILED: /__w/cobalt/cobalt/starboard/...",
    )
    self.assertEqual(matches[0]["line_num"], 2)

  def test_analyze_log_new_test_failure(self):
    content = (
        "Some line\n2026-07-15T18:09:06.0703651Z      Result: FAIL\nAnother"
        " line")
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(matches[0]["line"],
                     "2026-07-15T18:09:06.0703651Z      Result: FAIL")
    self.assertEqual(matches[0]["line_num"], 2)

  def test_generate_report(self):
    log_infra = self.create_log_file("Runner lost communication")
    log_comp = self.create_log_file("main.cc:10: error: compilation failed")

    now = datetime.datetime.now(datetime.timezone.utc)
    recent_time = ((now - datetime.timedelta(hours=2)).isoformat().replace(
        "+00:00", "Z"))

    mock_results = {
        "total_jobs_fetched":
            10,
        "runs": [
            {
                "run_id":
                    111,
                "workflow_name":
                    "android",
                "branch":
                    "main",
                "event":
                    "push",
                "createdAt":
                    recent_time,
                "failed_jobs": [{
                    "name": "android-arm",
                    "url": "https://github.com/run/111/job/1",
                    "local_log_path": log_infra,
                }],
            },
            {
                "run_id":
                    222,
                "workflow_name":
                    "linux",
                "branch":
                    "27.lts",
                "event":
                    "schedule",
                "createdAt":
                    recent_time,
                "failed_jobs": [{
                    "name": "linux-x64",
                    "url": "https://github.com/run/222/job/2",
                    "local_log_path": log_comp,
                }],
            },
        ],
    }

    report = result_analyzer.generate_report(mock_results)

    # Verify elements are present in the report
    self.assertIn("# GitHub Build Status Triage Report", report)
    self.assertIn("**Total Jobs Fetched**: 10", report)
    self.assertIn("**Failed Jobs**: 2", report)
    self.assertIn("### Branch: main (Unhealthy)", report)
    self.assertIn("### Branch: 27.lts (Unhealthy)", report)
    self.assertIn("#### Job: android-arm", report)
    self.assertIn("#### Job: linux-x64", report)
    self.assertIn("Line 1: `Runner lost communication`", report)
    self.assertIn("Line 1: `main.cc:10: error: compilation failed`", report)

  def test_analyze_log_file_not_found(self):
    matches = result_analyzer.analyze_log(
        os.path.join(self.test_dir.name, "does_not_exist.log"))
    self.assertEqual(len(matches), 1)
    self.assertIsNone(matches[0]["line_num"])
    self.assertIn("Log file not found", matches[0]["line"])

  def test_analyze_log_read_error(self):
    path = os.path.join(self.test_dir.name, "read_error.log")
    # Create the file before patching open
    with open(path, "w", encoding="utf-8") as f:
      f.write("Some log content")

    with mock.patch("builtins.open") as mock_open:
      mock_open.side_effect = PermissionError("Permission denied")
      matches = result_analyzer.analyze_log(path)
      self.assertEqual(len(matches), 1)
      self.assertIsNone(matches[0]["line_num"])
      self.assertIn("Error reading log file", matches[0]["line"])

  def test_generate_report_empty_log_path(self):
    mock_results = {
        "total_jobs_fetched":
            5,
        "runs": [{
            "run_id":
                333,
            "workflow_name":
                "evergreen",
            "branch":
                "main",
            "event":
                "push",
            "failed_jobs": [{
                "name": "evergreen-job",
                "url": "https://github.com/run/333/job/1",
                "local_log_path": "",
            }],
        }],
    }
    report = result_analyzer.generate_report(mock_results)
    self.assertIn("**Failed Jobs**: 1", report)
    self.assertIn("#### Job: evergreen-job", report)
    self.assertIn("Log file not found", report)

  def test_analyze_log_compilation_error_msvc(self):
    content = (
        "cl : Command line warning D9002 : ignoring unknown"
        " option\n..\\..\\foo.cc(12) : fatal error C1083: Cannot open include"
        " file: 'bar.h': No such file or directory")
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(
        matches[0]["line"],
        "..\\..\\foo.cc(12) : fatal error C1083: Cannot open include file:"
        " 'bar.h': No such file or directory",
    )
    self.assertEqual(matches[0]["line_num"], 2)

  def test_analyze_log_compilation_error_java(self):
    content = (
        "Some compile task\nsrc/com/example/Foo.java:23: error: cannot find"
        " symbol\nAnother error line")
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(
        matches[0]["line"],
        "src/com/example/Foo.java:23: error: cannot find symbol",
    )
    self.assertEqual(matches[0]["line_num"], 2)

    # This matches the pattern of a GTest failure on Windows
    # which should NOT match compilation_error
    content = ("Some output\n..\\..\\starboard\\nplb\\"
               "condition_variable_wait_timed_test.cc(50):"
               " error: Expected: (1) != (0)\nFAILED")
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    # It should not match any rules (since it's a false positive check for
    # compilation_error, and GTest Windows error is not currently in the
    # RULES regex list).
    self.assertEqual(len(matches), 1)
    self.assertIsNone(matches[0]["line_num"])
    self.assertEqual(matches[0]["line"], "No matching error signature found.")

  def test_analyze_log_infra_error_gcs_no_url(self):
    content = ("Download start\nCommandException: No URLs matched:"
               " gs://cobalt-xmls/builds.zip\nDownload failed")
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(
        matches[0]["line"],
        "CommandException: No URLs matched: gs://cobalt-xmls/builds.zip",
    )
    self.assertEqual(matches[0]["line_num"], 2)

  def test_generate_report_with_healthy_branch(self):
    log_infra = self.create_log_file("Runner lost communication")
    now = datetime.datetime.now(datetime.timezone.utc)
    recent_time = ((now - datetime.timedelta(hours=2)).isoformat().replace(
        "+00:00", "Z"))

    mock_results = {
        "total_jobs_fetched":
            10,
        "runs": [
            {
                "run_id":
                    111,
                "workflow_name":
                    "android",
                "branch":
                    "main",
                "event":
                    "push",
                "createdAt":
                    recent_time,
                "failed_jobs": [{
                    "name": "android-arm",
                    "url": "https://github.com/run/111/job/1",
                    "local_log_path": log_infra,
                }],
            },
            {
                "run_id": 333,
                "workflow_name": "linux",
                "branch": "master",
                "event": "push",
                "createdAt": recent_time,
                "failed_jobs": [],
            },
        ],
    }

    report = result_analyzer.generate_report(mock_results)

    self.assertIn("### Branch: master (Healthy)", report)
    self.assertIn("*   All runs completed successfully.", report)
    self.assertIn("### Branch: main (Unhealthy)", report)
    self.assertIn("## Detailed Branch Failures", report)
    self.assertIn("### Branch: main", report)
    self.assertNotIn("### Branch: master",
                     report.split("## Detailed Branch Failures")[1])

  def test_generate_report_empty_results(self):
    mock_results = {}
    report = result_analyzer.generate_report(mock_results)
    self.assertIn("# GitHub Build Status Triage Report", report)
    self.assertIn("**Total Jobs Fetched**: 0", report)
    self.assertIn("**Failed Jobs**: 0", report)
    self.assertIn("## Branch Health Report", report)
    self.assertIn("## Detailed Branch Failures", report)
    self.assertIn("*   None", report)

  def test_generate_report_outdated_nightly(self):
    log_infra = self.create_log_file("Runner lost communication")
    now = datetime.datetime.now(datetime.timezone.utc)
    # 2 days ago is > 24 hours (nightly limit)
    outdated_time = ((now - datetime.timedelta(days=2)).isoformat().replace(
        "+00:00", "Z"))

    mock_results = {
        "total_jobs_fetched":
            5,
        "runs": [{
            "run_id":
                444,
            "workflow_name":
                "nightly-tests",
            "branch":
                "main",
            "event":
                "schedule",
            "createdAt":
                outdated_time,
            "failed_jobs": [{
                "name": "android-arm",
                "url": "https://github.com/run/444/job/1",
                "local_log_path": log_infra,
            }],
        }],
    }

    report = result_analyzer.generate_report(mock_results)

    # Should report as outdated failures
    self.assertIn("### Branch: main (Outdated Failures)", report)
    self.assertIn("outdated failed run(s) exist", report)
    self.assertIn("#### Outdated Failed Runs (Retrigger Suggested)", report)
    self.assertIn("Run ID: 444", report)
    # Outdated job shouldn't be counted under standard failed jobs count
    self.assertIn("**Failed Jobs**: 0", report)

  def test_generate_report_outdated_postsubmit(self):
    log_infra = self.create_log_file("Runner lost communication")
    now = datetime.datetime.now(datetime.timezone.utc)
    # 8 days ago is > 7 days (postsubmit limit)
    outdated_time = ((now - datetime.timedelta(days=8)).isoformat().replace(
        "+00:00", "Z"))

    mock_results = {
        "total_jobs_fetched":
            5,
        "runs": [{
            "run_id":
                555,
            "workflow_name":
                "postsubmit-tests",
            "branch":
                "main",
            "event":
                "push",
            "createdAt":
                outdated_time,
            "failed_jobs": [{
                "name": "android-arm",
                "url": "https://github.com/run/555/job/1",
                "local_log_path": log_infra,
            }],
        }],
    }

    report = result_analyzer.generate_report(mock_results)

    # Should report as outdated failures
    self.assertIn("### Branch: main (Outdated Failures)", report)
    self.assertIn("outdated failed run(s) exist", report)
    self.assertIn("#### Outdated Failed Runs (Retrigger Suggested)", report)
    self.assertIn("Run ID: 555", report)
    self.assertIn("**Failed Jobs**: 0", report)

  def test_generate_report_timezone_offsets(self):
    log_infra = self.create_log_file("Runner lost communication")
    now = datetime.datetime.now(datetime.timezone.utc)
    # Test with +02:00 timezone offset (recent: 2 hours ago in local time)
    local_time_recent = ((now - datetime.timedelta(hours=2)).astimezone(
        datetime.timezone(datetime.timedelta(hours=2))).isoformat())

    # Test with -05:00 timezone offset (outdated: 8 days ago in local time)
    local_time_outdated = ((now - datetime.timedelta(days=8)).astimezone(
        datetime.timezone(datetime.timedelta(hours=-5))).isoformat())

    mock_results = {
        "total_jobs_fetched":
            2,
        "runs": [
            {
                "run_id":
                    101,
                "workflow_name":
                    "test-tz-recent",
                "branch":
                    "main",
                "event":
                    "push",
                "createdAt":
                    local_time_recent,
                "failed_jobs": [{
                    "name": "job-recent",
                    "url": "https://github.com/run/101/job/1",
                    "local_log_path": log_infra,
                }],
            },
            {
                "run_id":
                    102,
                "workflow_name":
                    "test-tz-outdated",
                "branch":
                    "main",
                "event":
                    "push",
                "createdAt":
                    local_time_outdated,
                "failed_jobs": [{
                    "name": "job-outdated",
                    "url": "https://github.com/run/102/job/1",
                    "local_log_path": log_infra,
                }],
            },
        ],
    }

    report = result_analyzer.generate_report(mock_results)

    self.assertIn("#### Job: job-recent", report)
    self.assertIn("**Failed Jobs**: 1", report)

    self.assertIn("Run ID: 102", report)
    self.assertIn("test-tz-outdated", report)

  def test_generate_report_naive_and_invalid_datetimes(self):
    log_infra = self.create_log_file("Runner lost communication")
    # Naive datetime format (without Z or offset)
    naive_time = "2026-07-20T12:00:00"
    # Invalid date format
    invalid_time = "invalid-date-string"

    mock_results = {
        "total_jobs_fetched":
            2,
        "runs": [
            {
                "run_id":
                    201,
                "workflow_name":
                    "test-naive",
                "branch":
                    "main",
                "event":
                    "push",
                "createdAt":
                    naive_time,
                "failed_jobs": [{
                    "name": "job-naive",
                    "url": "https://github.com/run/201/job/1",
                    "local_log_path": log_infra,
                }],
            },
            {
                "run_id":
                    202,
                "workflow_name":
                    "test-invalid",
                "branch":
                    "main",
                "event":
                    "push",
                "createdAt":
                    invalid_time,
                "failed_jobs": [{
                    "name": "job-invalid",
                    "url": "https://github.com/run/202/job/1",
                    "local_log_path": log_infra,
                }],
            },
        ],
    }

    # This should execute without throwing offset-naive vs offset-aware
    # TypeError or ValueError
    report = result_analyzer.generate_report(mock_results)

    self.assertIn("#### Job: job-naive", report)
    self.assertIn("#### Job: job-invalid", report)
    self.assertIn("**Failed Jobs**: 2", report)

  def test_generate_report_boundary_conditions(self):
    log_infra = self.create_log_file("Runner lost communication")
    fixed_now = datetime.datetime(
        2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)

    class MockDateTime(datetime.datetime):

      @classmethod
      def now(cls, tz=None):
        return fixed_now

    with mock.patch.object(
        result_analyzer.datetime,
        "datetime",
        MockDateTime,
    ):
      # 1. Exactly on limit (nightly) -> 24 hours ago
      nightly_limit = ((fixed_now -
                        datetime.timedelta(hours=24)).isoformat().replace(
                            "+00:00", "Z"))
      # 2. Just below limit (nightly) -> 23 hours 59 minutes ago
      # (should be recent)
      nightly_recent = ((fixed_now - datetime.timedelta(
          hours=23, minutes=59)).isoformat().replace("+00:00", "Z"))
      # 3. Just above limit (nightly) -> 24 hours 1 minute ago
      # (should be outdated)
      nightly_outdated = ((fixed_now - datetime.timedelta(
          hours=24, minutes=1)).isoformat().replace("+00:00", "Z"))

      mock_results = {
          "total_jobs_fetched":
              3,
          "runs": [
              {
                  "run_id":
                      301,
                  "workflow_name":
                      "nightly-exact",
                  "branch":
                      "main",
                  "event":
                      "schedule",
                  "createdAt":
                      nightly_limit,
                  "failed_jobs": [{
                      "name": "j1",
                      "url": "https://github.com/run/301/job/1",
                      "local_log_path": log_infra,
                  }],
              },
              {
                  "run_id":
                      302,
                  "workflow_name":
                      "nightly-recent",
                  "branch":
                      "main",
                  "event":
                      "schedule",
                  "createdAt":
                      nightly_recent,
                  "failed_jobs": [{
                      "name": "j2",
                      "url": "https://github.com/run/302/job/1",
                      "local_log_path": log_infra,
                  }],
              },
              {
                  "run_id":
                      303,
                  "workflow_name":
                      "nightly-outdated",
                  "branch":
                      "main",
                  "event":
                      "schedule",
                  "createdAt":
                      nightly_outdated,
                  "failed_jobs": [{
                      "name": "j3",
                      "url": "https://github.com/run/303/job/1",
                      "local_log_path": log_infra,
                  }],
              },
          ],
      }

      report = result_analyzer.generate_report(mock_results)

      # 301 (exactly 24h) -> age is exactly 24h, not > 24h (since age > limit),
      # so it should be RECENT
      self.assertIn("#### Job: j1", report)

      # 302 (23h 59m) -> RECENT
      self.assertIn("#### Job: j2", report)

      # 303 (24h 1m) -> OUTDATED
      self.assertIn("Run ID: 303", report)
      self.assertNotIn("#### Job: j3", report)

  def test_analyze_log_ignored_wrapper_traceback(self):
    content = ("Some log header\n"
               "Traceback (most recent call last):\n"
               '  File "cobalt_test_wrapper.py", line 100, in <module>\n'
               "    sys.exit(main())\n"
               '  File "cobalt_test_wrapper.py", line 85, in main\n'
               "    raise AssertionError('Wrapper test failed: 1 != 0')\n"
               "AssertionError: Wrapper test failed: 1 != 0\n"
               "Another log line")
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    # The traceback should be ignored, returning
    # 'No matching error signature found.'
    self.assertEqual(len(matches), 1)
    self.assertIsNone(matches[0]["line_num"])
    self.assertEqual(matches[0]["line"], "No matching error signature found.")

  def test_analyze_log_real_traceback(self):
    content = ("Some log header\n"
               "Traceback (most recent call last):\n"
               '  File "main.py", line 10, in <module>\n'
               "    crash()\n"
               "ZeroDivisionError: division by zero\n"
               "Another log line")
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    # This is a real traceback, so it should match
    self.assertEqual(len(matches), 1)
    self.assertEqual(matches[0]["line_num"], 5)
    self.assertEqual(matches[0]["line"], "ZeroDivisionError: division by zero")

  def test_analyze_log_all_errors_marked_chronologically(self):
    content = ("Line 1\n"
               "[  FAILED  ]\n"
               "Line 3\n"
               "ERROR at //foo.gn\n"
               "Line 5\n"
               "FAILED: obj/foo.o\n"
               "foo.cc:10: error: bad\n"
               "Line 8\n"
               "Segmentation fault\n"
               "Line 10\n"
               "Runner lost communication\n"
               "Line 12\n"
               "FAIL:\n")
    path = self.create_log_file(content)
    matches = result_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 7)

    self.assertEqual(matches[0]["line_num"], 2)
    self.assertEqual(matches[0]["line"], "[  FAILED  ]")
    self.assertEqual(matches[1]["line_num"], 4)
    self.assertEqual(matches[1]["line"], "ERROR at //foo.gn")
    self.assertEqual(matches[2]["line_num"], 6)
    self.assertEqual(matches[2]["line"], "FAILED: obj/foo.o")
    self.assertEqual(matches[3]["line_num"], 7)
    self.assertEqual(matches[3]["line"], "foo.cc:10: error: bad")
    self.assertEqual(matches[4]["line_num"], 9)
    self.assertEqual(matches[4]["line"], "Segmentation fault")
    self.assertEqual(matches[5]["line_num"], 11)
    self.assertEqual(matches[5]["line"], "Runner lost communication")
    self.assertEqual(matches[6]["line_num"], 13)
    self.assertEqual(matches[6]["line"], "FAIL:")

  def test_generate_report_outdated_successful_run(self):
    now = datetime.datetime.now(datetime.timezone.utc)
    # 10 days ago (outdated)
    outdated_time = ((now - datetime.timedelta(days=10)).isoformat().replace(
        "+00:00", "Z"))

    mock_results = {
        "total_jobs_fetched":
            5,
        "runs": [{
            "run_id": 999,
            "workflow_name": "nightly-tests",
            "branch": "main",
            "event": "schedule",
            "createdAt": outdated_time,
            "failed_jobs": [],  # Completed successfully
        }],
    }

    report = result_analyzer.generate_report(mock_results)

    # Should report as Healthy, NOT Outdated Failures
    self.assertIn("### Branch: main (Healthy)", report)
    self.assertIn("All runs completed successfully.", report)
    self.assertNotIn("Outdated Failures", report)
    self.assertNotIn("Outdated Failed Runs", report)


if __name__ == "__main__":
  unittest.main()
