# Copyright 2026 The Cobalt Authors. All rights reserved.
# TAG=agy
"""Tests for unified_analyzer.py."""

import datetime
import shutil
import tempfile
import unittest

import unified_analyzer


class TestUnifiedAnalyzer(unittest.TestCase):
  """Tests for the unified analyzer."""

  def setUp(self):
    super().setUp()
    self.test_dir = tempfile.mkdtemp()

    # Create mock log files
    self.compile_log_path = self.create_log_file(
        "Starting build...\n"
        "src/main.cc:42: error: expected ';' before '}' token\n"
        "Build failed!\n")
    self.child_log_path = self.create_log_file(
        "Running tests...\n"
        "[  FAILED  ] MyClassTest.ComputeValue (10 ms)\n"
        "Tests failed!\n")
    self.infra_log_path = self.create_log_file("Runner lost communication\n")

    # Fixed time for testing: 2026-07-22 12:00:00 UTC
    self.mock_now = datetime.datetime.fromisoformat("2026-07-22T12:00:00+00:00")

  def tearDown(self):
    shutil.rmtree(self.test_dir)
    super().tearDown()

  def create_log_file(self, content):
    with tempfile.NamedTemporaryFile(
        mode="w+", suffix=".log", dir=self.test_dir, delete=False) as log_file:
      log_file.write(content)
      return log_file.name

  # ==========================================
  # Age Check Tests
  # ==========================================

  def test_check_run_age_nightly(self):
    recent_time = "2026-07-22T08:00:00Z"  # 4h ago
    outdated_time = "2026-07-21T10:00:00Z"  # 26h ago

    is_outdated, age_str = unified_analyzer.check_run_age(
        recent_time, "nightly", self.mock_now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, "4 hour(s) ago")

    is_outdated, age_str = unified_analyzer.check_run_age(
        outdated_time, "nightly", self.mock_now)
    self.assertTrue(is_outdated)
    self.assertEqual(age_str, "1 day(s) ago")

  def test_check_run_age_postsubmit(self):
    recent_time = "2026-07-20T12:00:00Z"  # 2 days ago
    outdated_time = "2026-07-14T12:00:00Z"  # 8 days ago

    is_outdated, age_str = unified_analyzer.check_run_age(
        recent_time, "postsubmit", self.mock_now)
    self.assertFalse(is_outdated)
    self.assertEqual(age_str, "2 day(s) ago")

    is_outdated, age_str = unified_analyzer.check_run_age(
        outdated_time, "postsubmit", self.mock_now)
    self.assertTrue(is_outdated)
    self.assertEqual(age_str, "8 day(s) ago")

  # ==========================================
  # Log Analysis Tests
  # ==========================================

  def test_analyze_log_compilation(self):
    matches = unified_analyzer.analyze_log(
        self.compile_log_path, tag="test_tag")
    self.assertEqual(len(matches), 1)
    self.assertEqual(matches[0]["line_num"], 2)
    self.assertEqual(matches[0]["category"], "compilation_error")
    self.assertEqual(matches[0]["tag"], "test_tag")
    self.assertIn("src/main.cc:42: error", matches[0]["line"])

  def test_analyze_log_test_failure(self):
    matches = unified_analyzer.analyze_log(self.child_log_path, tag="gha_log")
    self.assertEqual(len(matches), 1)
    self.assertEqual(matches[0]["line_num"], 2)
    self.assertEqual(matches[0]["category"], "test_failure")
    self.assertEqual(matches[0]["tag"], "gha_log")
    self.assertIn("[  FAILED  ]", matches[0]["line"])

  def test_analyze_log_infra_error(self):
    matches = unified_analyzer.analyze_log(self.infra_log_path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(matches[0]["line_num"], 1)
    self.assertEqual(matches[0]["category"], "infra_error")

  def test_analyze_log_infra_error_timeout_waiting_for_build(self):
    content = "Some log\nTimeout waiting for Kokoro build\nAnother log"
    path = self.create_log_file(content)
    matches = unified_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(matches[0]["line_num"], 2)
    self.assertEqual(matches[0]["category"], "infra_error")
    self.assertEqual(matches[0]["line"], "Timeout waiting for Kokoro build")

    content_other = "Some log\nTimeout waiting for GHA build\nAnother log"
    path_other = self.create_log_file(content_other)
    matches_other = unified_analyzer.analyze_log(path_other)
    self.assertEqual(len(matches_other), 1)
    self.assertEqual(matches_other[0]["line_num"], 2)
    self.assertEqual(matches_other[0]["category"], "infra_error")
    self.assertEqual(matches_other[0]["line"], "Timeout waiting for GHA build")

  def test_analyze_log_no_match(self):
    clean_log = self.create_log_file("All tests passed.\n")
    matches = unified_analyzer.analyze_log(clean_log)
    self.assertEqual(len(matches), 1)
    self.assertIsNone(matches[0]["line_num"])
    self.assertEqual(matches[0]["line"], "No matching error signature found.")
    self.assertIsNone(matches[0]["category"])

  def test_analyze_log_compilation_error_msvc(self):
    content = ("cl : Command line warning D9002 : ignoring unknown option\n"
               "..\\..\\foo.cc(12) : fatal error C1083: "
               "Cannot open include file: 'bar.h': No such file or directory")
    path = self.create_log_file(content)
    matches = unified_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(
        matches[0]["line"],
        "..\\..\\foo.cc(12) : fatal error C1083: "
        "Cannot open include file: 'bar.h': No such file or directory",
    )
    self.assertEqual(matches[0]["line_num"], 2)
    self.assertEqual(matches[0]["category"], "compilation_error")

  def test_analyze_log_compilation_error_java(self):
    content = ("Some compile task\n"
               "src/com/example/Foo.java:23: error: cannot find symbol\n"
               "Another error line")
    path = self.create_log_file(content)
    matches = unified_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(
        matches[0]["line"],
        "src/com/example/Foo.java:23: error: cannot find symbol",
    )
    self.assertEqual(matches[0]["line_num"], 2)
    self.assertEqual(matches[0]["category"], "compilation_error")

    # False positive check (GTest failure on Windows should NOT match
    # compilation_error)
    content = ("Some output\n"
               "..\\..\\starboard\\nplb\\"
               "condition_variable_wait_timed_test.cc(50): "
               "error: Expected: (1) != (0)\n"
               "FAILED")
    path = self.create_log_file(content)
    matches = unified_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertIsNone(matches[0]["line_num"])
    self.assertEqual(matches[0]["line"], "No matching error signature found.")

  def test_analyze_log_infra_error_gcs_no_url(self):
    content = (
        "Download start\n"
        "CommandException: No URLs matched: gs://cobalt-xmls/builds.zip\n"
        "Download failed")
    path = self.create_log_file(content)
    matches = unified_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(
        matches[0]["line"],
        "CommandException: No URLs matched: gs://cobalt-xmls/builds.zip",
    )
    self.assertEqual(matches[0]["line_num"], 2)
    self.assertEqual(matches[0]["category"], "infra_error")

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
    matches = unified_analyzer.analyze_log(path)
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
    matches = unified_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 1)
    self.assertEqual(matches[0]["line_num"], 5)
    self.assertEqual(matches[0]["line"], "ZeroDivisionError: division by zero")
    self.assertEqual(matches[0]["category"], "crash_signature")

  def test_analyze_log_device_crash_signatures(self):
    content = (
        "Line 1\n"
        "Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), "
        "fault addr 0x0 in tid 123\n"
        "Line 3\n"
        "*** *** *** *** *** *** *** *** *** *** *** *** *** *** *** ***\n"
        "Line 5\n"
        "FATAL EXCEPTION: main\n"
        "Line 7\n"
        "Caused by: java.lang.NullPointerException\n"
        "Line 9\n"
        "Process com.google.android.youtube.cobalt (pid 456) has died\n")
    path = self.create_log_file(content)
    matches = unified_analyzer.analyze_log(path)
    self.assertEqual(len(matches), 5)
    for m in matches:
      self.assertEqual(m["category"], "crash_signature")
    self.assertEqual(matches[0]["line_num"], 2)
    self.assertEqual(matches[1]["line_num"], 4)
    self.assertEqual(matches[2]["line_num"], 6)
    self.assertEqual(matches[3]["line_num"], 8)
    self.assertEqual(matches[4]["line_num"], 10)

  # ==========================================
  # Results Processing & Recency Tests
  # ==========================================

  def test_process_results_data_timezone_offsets(self):
    # Test with timezone offsets
    # recent: 2 hours ago in local time (+02:00)
    local_time_recent = ((self.mock_now -
                          datetime.timedelta(hours=2)).astimezone(
                              datetime.timezone(
                                  datetime.timedelta(hours=2))).isoformat())

    # outdated: 8 days ago in local time (-05:00)
    local_time_outdated = ((self.mock_now -
                            datetime.timedelta(days=8)).astimezone(
                                datetime.timezone(
                                    datetime.timedelta(hours=-5))).isoformat())

    mock_data = {
        "source":
            "github",
        "total_jobs_fetched":
            2,
        "runs": [
            {
                "run_id":
                    "101",
                "job_name":
                    "test-tz-recent",
                "branch":
                    "main",
                "event":
                    "push",
                "createdAt":
                    local_time_recent,
                "failed_jobs": [{
                    "name": "job-recent",
                    "url": "http://gha/101/job/1",
                    "local_log_path": self.infra_log_path,
                }],
            },
            {
                "run_id":
                    "102",
                "job_name":
                    "test-tz-outdated",
                "branch":
                    "main",
                "event":
                    "push",
                "createdAt":
                    local_time_outdated,
                "failed_jobs": [{
                    "name": "job-outdated",
                    "url": "http://gha/102/job/1",
                    "local_log_path": self.infra_log_path,
                }],
            },
        ],
    }

    failed, outdated = unified_analyzer.process_results_data(
        mock_data, self.mock_now)
    self.assertEqual(len(failed), 1)
    self.assertEqual(failed[0]["run_id"], "101")
    self.assertEqual(len(outdated), 1)
    self.assertEqual(outdated[0]["run_id"], "102")

  def test_process_results_data_naive_and_invalid_datetimes(self):
    naive_time = "2026-07-20T12:00:00"
    invalid_time = "invalid-date-string"

    fixed_now = datetime.datetime(
        2026, 7, 20, 13, 0, 0, tzinfo=datetime.timezone.utc)

    mock_data = {
        "source":
            "github",
        "total_jobs_fetched":
            2,
        "runs": [
            {
                "run_id":
                    "201",
                "job_name":
                    "test-naive",
                "branch":
                    "main",
                "event":
                    "push",
                "createdAt":
                    naive_time,
                "failed_jobs": [{
                    "name": "job-naive",
                    "url": "http://gha/201/job/1",
                    "local_log_path": self.infra_log_path,
                }],
            },
            {
                "run_id":
                    "202",
                "job_name":
                    "test-invalid",
                "branch":
                    "main",
                "event":
                    "push",
                "createdAt":
                    invalid_time,
                "failed_jobs": [{
                    "name": "job-invalid",
                    "url": "http://gha/202/job/1",
                    "local_log_path": self.infra_log_path,
                }],
            },
        ],
    }

    # Should execute without throwing TypeError/ValueError for naive/invalid
    # dates
    failed, outdated = unified_analyzer.process_results_data(
        mock_data, fixed_now)
    # Naive time "2026-07-20T12:00:00" is 1 hour before fixed_now (recent)
    # -> failed
    # Invalid time -> check_run_age returns (False, "unknown age") -> failed
    # (not outdated)
    self.assertEqual(len(failed), 2)
    self.assertEqual(len(outdated), 0)

  def test_process_results_data_boundary_conditions(self):
    fixed_now = datetime.datetime(
        2026, 7, 20, 12, 0, 0, tzinfo=datetime.timezone.utc)

    # 1. Exactly on limit (nightly limit is 24h) -> 24 hours ago
    nightly_limit = ((fixed_now -
                      datetime.timedelta(hours=24)).isoformat().replace(
                          "+00:00", "Z"))
    # 2. Just below limit (recent) -> 23 hours 59 minutes ago
    nightly_recent = ((fixed_now - datetime.timedelta(
        hours=23, minutes=59)).isoformat().replace("+00:00", "Z"))
    # 3. Just above limit (outdated) -> 24 hours 1 minute ago
    nightly_outdated = ((fixed_now - datetime.timedelta(
        hours=24, minutes=1)).isoformat().replace("+00:00", "Z"))

    mock_data = {
        "source":
            "github",
        "total_jobs_fetched":
            3,
        "runs": [
            {
                "run_id":
                    "301",
                "job_name":
                    "nightly-exact",
                "branch":
                    "main",
                "event":
                    "schedule",
                "createdAt":
                    nightly_limit,
                "failed_jobs": [{
                    "name": "j1",
                    "url": "http://gha/301/job/1",
                    "local_log_path": self.infra_log_path
                }],
            },
            {
                "run_id":
                    "302",
                "job_name":
                    "nightly-recent",
                "branch":
                    "main",
                "event":
                    "schedule",
                "createdAt":
                    nightly_recent,
                "failed_jobs": [{
                    "name": "j2",
                    "url": "http://gha/302/job/1",
                    "local_log_path": self.infra_log_path
                }],
            },
            {
                "run_id":
                    "303",
                "job_name":
                    "nightly-outdated",
                "branch":
                    "main",
                "event":
                    "schedule",
                "createdAt":
                    nightly_outdated,
                "failed_jobs": [{
                    "name": "j3",
                    "url": "http://gha/303/job/1",
                    "local_log_path": self.infra_log_path
                }],
            },
        ],
    }

    failed, outdated = unified_analyzer.process_results_data(
        mock_data, fixed_now)
    # 301 (exactly 24h) -> age is exactly 24h, not > 24h, so recent -> failed
    # 302 (23h 59m) -> recent -> failed
    # 303 (24h 1m) -> outdated -> outdated
    self.assertEqual(len(failed), 2)
    self.assertEqual({r["run_id"] for r in failed}, {"301", "302"})
    self.assertEqual(len(outdated), 1)
    self.assertEqual(outdated[0]["run_id"], "303")

  # ==========================================
  # Report Formatting Tests
  # ==========================================

  def test_generate_report_with_device_system_log(self):
    failed_jobs = [{
        "source":
            "github",
        "branch":
            "main",
        "run_id":
            "301",
        "run_name":
            "test-device",
        "run_url":
            "http://gha/301",
        "job_name":
            "job-device",
        "job_url":
            "http://gha/301/job/1",
        "local_log_path":
            self.child_log_path,
        "device_system_log_path":
            self.infra_log_path,
        "matches": [{
            "line_num": 2,
            "line": "[  FAILED  ] MyClassTest",
            "category": "test_failure",
            "log_path": self.child_log_path,
            "tag": "test_log",
        }, {
            "line_num": 1,
            "line": "Runner lost communication",
            "category": "infra_error",
            "log_path": self.infra_log_path,
            "tag": "device_system_log",
        }]
    }]

    report = unified_analyzer.generate_report(failed_jobs, [], total_fetched=1)

    self.assertIn("#### Run: test-device (ID: 301) [GITHUB]", report)
    self.assertIn("**Job**: job-device", report)
    self.assertIn(f"**Cached Log**: `{self.child_log_path}`", report)
    self.assertIn(f"**Device System Log**: `{self.infra_log_path}`", report)
    self.assertIn(
        "[test_log] [test_failure] Line 2: `[  FAILED  ] MyClassTest`", report)
    self.assertIn(
        "[device_system_log] [infra_error] Line 1: `Runner lost communication`",
        report)

  def test_generate_report_with_missing_device_logs(self):
    failed_jobs = [{
        "source":
            "github",
        "branch":
            "main",
        "run_id":
            "302",
        "run_name":
            "test-device-missing",
        "run_url":
            "http://gha/302",
        "job_name":
            "job-device-missing",
        "job_url":
            "http://gha/302/job/1",
        "local_log_path":
            self.child_log_path,
        "device_logs_status":
            "MISSING",
        "matches": [{
            "line_num": 2,
            "line": "[  FAILED  ] MyClassTest",
            "category": "test_failure",
            "log_path": self.child_log_path,
            "tag": "test_log",
        }]
    }]

    report = unified_analyzer.generate_report(failed_jobs, [], total_fetched=1)

    self.assertIn("#### Run: test-device-missing (ID: 302) [GITHUB]", report)
    self.assertIn("**Job**: job-device-missing", report)
    self.assertIn("**Device System Log**: MISSING", report)

  def test_generate_report_branch_sorting(self):
    failed_jobs = [
        {
            "source": "github",
            "branch": "9.lts.1.master",
            "run_id": "1",
            "run_name": "run1",
            "run_url": "url",
            "job_name": "job1",
            "job_url": "url",
            "local_log_path": self.child_log_path,
            "matches": []
        },
        {
            "source": "github",
            "branch": "main",
            "run_id": "2",
            "run_name": "run2",
            "run_url": "url",
            "job_name": "job2",
            "job_url": "url",
            "local_log_path": self.child_log_path,
            "matches": []
        },
        {
            "source": "github",
            "branch": "25.lts.2.master",
            "run_id": "3",
            "run_name": "run3",
            "run_url": "url",
            "job_name": "job3",
            "job_url": "url",
            "local_log_path": self.child_log_path,
            "matches": []
        },
        {
            "source": "github",
            "branch": "25.lts.10.master",
            "run_id": "4",
            "run_name": "run4",
            "run_url": "url",
            "job_name": "job4",
            "job_url": "url",
            "local_log_path": self.child_log_path,
            "matches": []
        },
        {
            "source": "github",
            "branch": "25.lts.1.master",
            "run_id": "5",
            "run_name": "run5",
            "run_url": "url",
            "job_name": "job5",
            "job_url": "url",
            "local_log_path": self.child_log_path,
            "matches": []
        },
        {
            "source": "github",
            "branch": "COBALT_9",
            "run_id": "6",
            "run_name": "run6",
            "run_url": "url",
            "job_name": "job6",
            "job_url": "url",
            "local_log_path": self.child_log_path,
            "matches": []
        },
        {
            "source": "github",
            "branch": "custom-feature",
            "run_id": "7",
            "run_name": "run7",
            "run_url": "url",
            "job_name": "job7",
            "job_url": "url",
            "local_log_path": self.child_log_path,
            "matches": []
        },
        {
            "source": "github",
            "branch": "26.android",
            "run_id": "8",
            "run_name": "run8",
            "run_url": "url",
            "job_name": "job8",
            "job_url": "url",
            "local_log_path": self.child_log_path,
            "matches": []
        },
        {
            "source": "github",
            "branch": "26.eap",
            "run_id": "9",
            "run_name": "run9",
            "run_url": "url",
            "job_name": "job9",
            "job_url": "url",
            "local_log_path": self.child_log_path,
            "matches": []
        },
    ]

    report = unified_analyzer.generate_report(failed_jobs, [], total_fetched=9)

    # Expected order (version descending):
    # 1. main
    # 2. 26.eap
    # 3. 26.android
    # 4. 25.lts.10.master
    # 5. 25.lts.2.master
    # 6. 25.lts.1.master
    # 7. 9.lts.1.master
    # 8. COBALT_9
    # 9. custom-feature

    health_report_idx = report.find("## Branch Health Report")
    detailed_failures_idx = report.find("## Detailed Branch Failures")

    self.assertNotEqual(health_report_idx, -1)
    self.assertNotEqual(detailed_failures_idx, -1)

    # Check Health Report Section
    health_section = report[health_report_idx:detailed_failures_idx]
    idx_main = health_section.find("Branch: main")
    idx_26_eap = health_section.find("Branch: 26.eap")
    idx_26_android = health_section.find("Branch: 26.android")
    idx_25_10 = health_section.find("Branch: 25.lts.10.master")
    idx_25_2 = health_section.find("Branch: 25.lts.2.master")
    idx_25_1 = health_section.find("Branch: 25.lts.1.master")
    idx_9 = health_section.find("Branch: 9.lts.1.master")
    idx_cobalt = health_section.find("Branch: COBALT_9")
    idx_custom = health_section.find("Branch: custom-feature")

    self.assertNotEqual(idx_main, -1)
    self.assertNotEqual(idx_26_eap, -1)
    self.assertNotEqual(idx_26_android, -1)
    self.assertNotEqual(idx_25_10, -1)
    self.assertNotEqual(idx_25_2, -1)
    self.assertNotEqual(idx_25_1, -1)
    self.assertNotEqual(idx_9, -1)
    self.assertNotEqual(idx_cobalt, -1)
    self.assertNotEqual(idx_custom, -1)

    self.assertTrue(
        idx_main < idx_26_eap < idx_26_android < idx_25_10 < idx_25_2 < idx_25_1 < idx_9 < idx_cobalt < idx_custom,
        "Branches not sorted in version descending order in Health Report"
    )

    # Check Detailed Branch Failures Section
    detailed_section = report[detailed_failures_idx:]
    idx_main_det = detailed_section.find("Branch: main")
    idx_26_eap_det = detailed_section.find("Branch: 26.eap")
    idx_26_android_det = detailed_section.find("Branch: 26.android")
    idx_25_10_det = detailed_section.find("Branch: 25.lts.10.master")
    idx_25_2_det = detailed_section.find("Branch: 25.lts.2.master")
    idx_25_1_det = detailed_section.find("Branch: 25.lts.1.master")
    idx_9_det = detailed_section.find("Branch: 9.lts.1.master")
    idx_cobalt_det = detailed_section.find("Branch: COBALT_9")
    idx_custom_det = detailed_section.find("Branch: custom-feature")

    self.assertNotEqual(idx_main_det, -1)
    self.assertNotEqual(idx_26_eap_det, -1)
    self.assertNotEqual(idx_26_android_det, -1)
    self.assertNotEqual(idx_25_10_det, -1)
    self.assertNotEqual(idx_25_2_det, -1)
    self.assertNotEqual(idx_25_1_det, -1)
    self.assertNotEqual(idx_9_det, -1)
    self.assertNotEqual(idx_cobalt_det, -1)
    self.assertNotEqual(idx_custom_det, -1)

    self.assertTrue(
        idx_main_det < idx_26_eap_det < idx_26_android_det < idx_25_10_det < idx_25_2_det < idx_25_1_det < idx_9_det < idx_cobalt_det < idx_custom_det,
        "Branches not sorted in version descending order in Detailed Failures"
    )


if __name__ == "__main__":
  unittest.main()
