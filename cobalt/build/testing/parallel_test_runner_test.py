#!/usr/bin/env python3

import unittest
import collections
import os
import io
import json
import shutil
import tempfile
import sys
from unittest.mock import MagicMock, patch, ANY

# Ensure we can import parallel_test_runner
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

import parallel_test_runner
from parallel_test_runner import condense_failures, find_last_running_test, construct_test_command, parse_gtest_results, DockerWorkerPool, split_batch_smart, chunk_tests, load_filters, update_filter_file, detect_crashes, TestScheduler

class TestCondenseFailures(unittest.TestCase):
    def test_condense_simple(self):
        failures = ["Suite.TestA", "Suite.TestB"]
        all_tests = {"Suite.": ["TestA", "TestB", "TestC"]}
        expected = ["Suite.TestA", "Suite.TestB"]
        self.assertEqual(condense_failures(failures, all_tests), sorted(expected))

    def test_condense_wildcard(self):
        failures = ["Suite.TestA", "Suite.TestB"]
        all_tests = {"Suite.": ["TestA", "TestB"]}
        expected = ["Suite.*"]
        self.assertEqual(condense_failures(failures, all_tests), expected)

    def test_condense_with_existing_filters(self):
        # Failures: [S.B], Existing: [S.A], All: [S.A, S.B] -> Result: [S.*]
        failures = ["Suite.TestB"]
        all_tests = {"Suite.": ["TestA", "TestB"]}
        existing = {"Suite.TestA"}
        expected = ["Suite.*"]
        self.assertEqual(condense_failures(failures, all_tests, existing), expected)

    def test_condense_with_existing_filters_keep_distinct(self):
        # Failures: [S.B], Existing: [S.A], All: [S.A, S.B, S.C] -> Result: [S.TestA, S.TestB] (C is pass)
        failures = ["Suite.TestB"]
        all_tests = {"Suite.": ["TestA", "TestB", "TestC"]}
        existing = {"Suite.TestA"}
        expected = ["Suite.TestA", "Suite.TestB"]
        self.assertEqual(condense_failures(failures, all_tests, existing), sorted(expected))

    def test_condense_unused_filters_preserved(self):
        # Failures: [S.A]. Existing: [S.B, Other.C]. All: [S.A, S.B].
        # Other.C is not in All, so it is unused. It should be preserved.
        failures = ["Suite.TestA"]
        all_tests = {"Suite.": ["TestA", "TestB"]}
        existing = {"Suite.TestB", "Other.C"}
        # Expectation: Other.C preserved. TestA + TestB (from existing) -> Suite.*
        expected = ["Other.C", "Suite.*"]
        self.assertEqual(condense_failures(failures, all_tests, existing), sorted(expected))

class TestSplitBatchSmart(unittest.TestCase):
    def test_split_smart_complex(self):
        # Batch: [S1.A, S1.B(Fail), S1.C, S2.D]
        batch = [("S1.", ["A", "B", "C"]), ("S2.", ["D"])]

        batches = split_batch_smart(batch, "S1.B")

        # Expected:
        # 1. [S1.A] (Pre - Batch)
        # 2. [S1.B] (Fail - Singleton)
        # 3. [S1.C] (Post Same Suite - Singleton)
        # 4. [S2.D] (Post Diff Suite - Batch)

        self.assertEqual(len(batches), 4)
        self.assertEqual(batches[0], [("S1.", ["A"])])
        self.assertEqual(batches[1], [("S1.", ["B"])])
        self.assertEqual(batches[2], [("S1.", ["C"])])
        self.assertEqual(batches[3], [("S2.", ["D"])])

    def test_split_smart_pre_batching(self):
        # Verify pre-failure tests are batched together
        # Batch: [S1.A, S1.B, S1.C(Fail)]
        batch = [("S1.", ["A", "B", "C"])]

        batches = split_batch_smart(batch, "S1.C")

        # Expected:
        # 1. [S1.A, S1.B] (Pre - Batch)
        # 2. [S1.C] (Fail - Singleton)

        self.assertEqual(len(batches), 2)
        # Check content of first batch
        self.assertEqual(len(batches[0]), 1)
        self.assertEqual(batches[0][0][1], ["A", "B"])

        self.assertEqual(batches[1], [("S1.", ["C"])])

    def test_split_smart_post_diff_batching(self):
        # Verify post-failure diff suite tests are batched
        # Batch: [S1.A(Fail), S2.B, S2.C]
        batch = [("S1.", ["A"]), ("S2.", ["B", "C"])]

        batches = split_batch_smart(batch, "S1.A")

        # Expected:
        # 1. [S1.A] (Fail - Singleton)
        # 2. [S2.B, S2.C] (Post Diff - Batch)

        self.assertEqual(len(batches), 2)
        self.assertEqual(batches[0], [("S1.", ["A"])])
        self.assertEqual(batches[1], [("S2.", ["B", "C"])])

    def test_split_smart_mixed_suites(self):
        # [S1.A, S1.B(Fail), S1.C, S2.D, S2.E]
        batch = [("S1.", ["A", "B", "C"]), ("S2.", ["D", "E"])]
        batches = split_batch_smart(batch, "S1.B")

        # 1. S1.A
        # 2. S1.B
        # 3. S1.C
        # 4. S2.D, S2.E
        self.assertEqual(len(batches), 4)
        self.assertEqual(batches[0], [("S1.", ["A"])])
        self.assertEqual(batches[1], [("S1.", ["B"])])
        self.assertEqual(batches[2], [("S1.", ["C"])])
        self.assertEqual(batches[3], [("S2.", ["D", "E"])])

class TestFilterOperations(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.filter_file = os.path.join(self.temp_dir, "test_filter.json")

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def test_load_filters_nonexistent(self):
        self.assertEqual(load_filters(self.filter_file), set())

    def test_load_filters_existing(self):
        data = {"failing_tests": ["Suite.TestA"]}
        with open(self.filter_file, 'w') as f:
            json.dump(data, f)
        self.assertEqual(load_filters(self.filter_file), {"Suite.TestA"})

    def test_update_filter_file_create_new(self):
        new_failures = ["Suite.NewFail"]
        update_filter_file(self.filter_file, new_failures)

        with open(self.filter_file, 'r') as f:
            data = json.load(f)

        self.assertEqual(data["failing_tests"], ["Suite.NewFail"])
        self.assertIn("comment", data)

    def test_update_filter_file_append_and_dedup(self):
        # Create initial
        data = {"failing_tests": ["Suite.OldFail"], "comment": "Keep me"}
        with open(self.filter_file, 'w') as f:
            json.dump(data, f)

        new_failures = ["Suite.NewFail", "Suite.OldFail"] # Overlap
        update_filter_file(self.filter_file, new_failures)

        with open(self.filter_file, 'r') as f:
            data = json.load(f)

        self.assertEqual(data["failing_tests"], ["Suite.NewFail", "Suite.OldFail"])
        self.assertEqual(data["comment"], "Keep me")

    def test_update_filter_file_replace(self):
        # Create initial
        data = {"failing_tests": ["Suite.OldFail"], "comment": "Keep me"}
        with open(self.filter_file, 'w') as f:
            json.dump(data, f)

        new_failures = ["Suite.NewFail"]
        update_filter_file(self.filter_file, new_failures, replace=True)

        with open(self.filter_file, 'r') as f:
            data = json.load(f)

        self.assertEqual(data["failing_tests"], ["Suite.NewFail"])

class TestExecutionHelpers(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.cwd = "/tmp"
        self.binary = "/bin/true"
        self.batch = [("Suite.", ["TestA", "TestB"])]

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def test_construct_test_command(self):
        cmd, xml_path, flag_path, expected = construct_test_command(self.batch, self.cwd, self.temp_dir, self.binary)

        self.assertEqual(expected, {"Suite.TestA", "Suite.TestB"})
        self.assertTrue(os.path.exists(flag_path))
        # XML is just a path, doesn't exist yet
        self.assertTrue(xml_path.startswith(self.temp_dir))

        # Check flag content
        with open(flag_path, 'r') as f:
            content = f.read()
        self.assertIn("--gtest_filter=Suite.TestA:Suite.TestB", content)

        # Check command
        self.assertEqual(cmd[0], self.binary)
        self.assertTrue(any(x.startswith("--gtest_flagfile=") for x in cmd))
        self.assertTrue(any(x.startswith("--gtest_output=xml:") for x in cmd))

    @patch("parallel_test_runner.parse_gtest_xml")
    @patch("parallel_test_runner.os.path.exists")
    def test_parse_gtest_results_pass(self, mock_exists, mock_parse_xml):
        mock_exists.return_value = True # XML exists
        mock_parse_xml.return_value = ([], [], {"Suite.TestA"}) # No failures, 1 executed

        expected = {"Suite.TestA"}
        failures, logs, last_test, executed, passed, _, _, _ = parse_gtest_results(0, "", "", "/tmp/xml", "/tmp/flag", expected)

        self.assertTrue(passed)
        self.assertEqual(failures, {})
        self.assertEqual(executed, {"Suite.TestA"})

    @patch("parallel_test_runner.parse_gtest_xml")
    @patch("parallel_test_runner.os.path.exists")
    def test_parse_gtest_results_fail(self, mock_exists, mock_parse_xml):
        mock_exists.return_value = True
        mock_parse_xml.return_value = (["Suite.TestA"], ["LogA"], {"Suite.TestA", "Suite.TestB"})

        expected = {"Suite.TestA", "Suite.TestB"}
        failures, logs, last_test, executed, passed, _, _, _ = parse_gtest_results(1, "", "", "/tmp/xml", "/tmp/flag", expected)

        self.assertFalse(passed)
        self.assertEqual(failures["FAIL"], ["Suite.TestA"])
        self.assertEqual(logs["Suite.TestA"], "LogA")

    @patch("parallel_test_runner.os.path.exists")
    def test_parse_gtest_results_crash(self, mock_exists):
        mock_exists.return_value = False # No XML

        expected = {"Suite.TestA"}
        failures, logs, last_test, executed, passed, _, _, _ = parse_gtest_results(139, "Crash", "Err", "/tmp/xml", "/tmp/flag", expected)

        self.assertFalse(passed)
        self.assertEqual(failures["CRASH"], ["Suite.TestA"])

class TestEndToEnd(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.cwd = os.getcwd()
        self.binary_path = os.path.join(self.cwd, "fake_binary")
        # Create dummy binary so os.path.exists works
        with open(self.binary_path, 'w') as f:
            f.write("I am a binary")

        self.filter_file = os.path.join(self.temp_dir, "fake_binary_filter.json")

        # Patch DockerWorkerPool
        self.pool_patcher = patch("parallel_test_runner.DockerWorkerPool")
        self.MockPool = self.pool_patcher.start()

        # Create a mock pool instance
        self.mock_pool = MagicMock()
        self.MockPool.return_value.__enter__.return_value = self.mock_pool
        self.MockPool.return_value.__exit__.return_value = None

    def tearDown(self):
        if os.path.exists(self.binary_path):
             os.remove(self.binary_path)
        shutil.rmtree(self.temp_dir)
        self.pool_patcher.stop()

    @patch("parallel_test_runner.parse_gtest_xml")
    @patch("parallel_test_runner.time.sleep") # speed up retry loops if any
    @patch("parallel_test_runner.wait")
    def test_full_fail_flow(self, mock_wait, mock_sleep, mock_parse_xml):
        # Configure wait to return all futures as done immediately
        def side_effect_wait(futures, return_when=None):
            return list(futures), []
        mock_wait.side_effect = side_effect_wait

        # Scenario:
        # 1. List tests -> Returns 2 tests: Suite.Pass, Suite.Fail
        # 2. Run batch -> Fails "Suite.Fail"
        # 3. Update filter file -> Check content

        # Mock invalid pool.run for list_tests to return success
        def pool_run_side_effect(cmd):
            # Check for gtest_list_tests
            if "--gtest_list_tests" in cmd:
                return (True, "Suite.\n  Pass\n  Fail", "", 0)
            return (True, "", "", 0)

        self.mock_pool.run.side_effect = pool_run_side_effect

        # Mock run_cmd_async to handle execution
        call_count = {"val": 0}

        def run_cmd_async_dynamic(*args, **kwargs):
            future = MagicMock()
            count = call_count["val"]
            call_count["val"] += 1

            if count == 0:
                # First Run: Fail Suite.Fail
                 failures = {"FAIL": ["Suite.Fail"]}
                 logs = {"Suite.Fail": "Failed"}
                 expected = {"Suite.Pass", "Suite.Fail"} # Expected
                 executed = {"Suite.Pass", "Suite.Fail"} # Actual execution of both
                 passed = False
                 future.result.return_value = (failures, logs, None, executed, passed, "xml", "flag", expected)

            else:
                # Second Run: Return All
                 failures = {}
                 logs = {}
                 expected = {"Suite.Pass"}
                 executed = {"Suite.Pass"}
                 passed = True
                 future.result.return_value = (failures, logs, None, executed, passed, "xml", "flag", expected)
            return future

        self.mock_pool.run_cmd_async.side_effect = run_cmd_async_dynamic
        self.mock_pool.num_workers = 1

        # We also need to mock get_tests/pull.run for the SECOND listing (after filter update)
        def pool_run_dynamic(cmd):
             if "--gtest_list_tests" in cmd:
                 # Check filter arg
                 if call_count["val"] > 0:
                     # Second listing
                     return (True, "Suite.\n  Pass", "", 0)
                 else:
                     return (True, "Suite.\n  Pass\n  Fail", "", 0)
             return (True, "", "", 0)

        self.mock_pool.run.side_effect = pool_run_dynamic

        # Run Main
        test_args = [
            "parallel_test_runner.py",
            self.binary_path,
            "--filter-file", self.filter_file,
            "--jobs", "1",
            "--consecutive-flake-free", "1"
        ]

        with patch.object(sys, 'argv', test_args):
            try:
                parallel_test_runner.main()
            except SystemExit as e:
                self.assertEqual(e.code, 0)

        # Verify Filter File
        self.assertTrue(os.path.exists(self.filter_file))
        with open(self.filter_file, "r") as f:
            data = json.load(f)
            self.assertIn("Suite.Fail", data["failing_tests"])
            self.assertNotIn("Suite.Pass", data["failing_tests"])

        # Run again to ensure filter works and it passes
        with patch.object(sys, 'argv', test_args):
            try:
                parallel_test_runner.main()
            except SystemExit as e:
                self.assertEqual(e.code, 0)

        self.assertNotIn("Suite.Pass", data["failing_tests"])

    @patch("parallel_test_runner.parse_gtest_xml")
    @patch("parallel_test_runner.time.sleep")
    @patch("parallel_test_runner.wait")
    def test_missing_test_accountability(self, mock_wait, mock_sleep, mock_parse_xml):
        # Scenario: List 2 tests, but execution only reports 1. Script should fail/retry.

        # Configure wait to return all futures as done immediately
        def side_effect_wait(futures, return_when=None):
            return list(futures), []
        mock_wait.side_effect = side_effect_wait

        def pool_run_side_effect(cmd):
            # Always return 2 tests list
            if "--gtest_list_tests" in cmd:
                 return (True, "Suite.\n  TestA\n  TestB", "", 0)
            return (True, "", "", 0)
        self.mock_pool.run.side_effect = pool_run_side_effect

        call_count = {"val": 0}
        def run_cmd_async_dynamic(*args, **kwargs):
            future = MagicMock()
            count = call_count["val"]
            call_count["val"] += 1

            if count == 0:
                 # Run 1: Return TestA passed, but TestB missing entirely
                 failures = {}
                 logs = {}
                 executed = {"Suite.TestA"} # Parser reports only TestA executed
                 passed = True
                 # We need to construct expected set for the mock return
                 expected_set = {"Suite.TestA", "Suite.TestB"}
                 future.result.return_value = (failures, logs, None, executed, passed, "xml", "flag", expected_set)
            else:
                 # Run 2: Return All
                 failures = {}
                 logs = {}
                 executed = {"Suite.TestA", "Suite.TestB"}
                 passed = True
                 expected_set = {"Suite.TestA", "Suite.TestB"}
                 future.result.return_value = (failures, logs, None, executed, passed, "xml", "flag", expected_set)
            return future

        self.mock_pool.run_cmd_async.side_effect = run_cmd_async_dynamic
        self.mock_pool.num_workers = 1

        test_args = [
            "parallel_test_runner.py",
            self.binary_path,
            "--filter-file", self.filter_file,
            "--jobs", "1",
            "--consecutive-flake-free", "1"
        ]

        with patch.object(sys, 'argv', test_args):
            with patch('sys.stdout', new_callable=io.StringIO) as fake_out:
                 try:
                    parallel_test_runner.main()
                 except SystemExit as e:
                    self.assertEqual(e.code, 0)

                 output = fake_out.getvalue()
                 # New behavior: Captures missing test as crash and resubmits
                 self.assertIn("[BATCH CRASH]", output)
                 self.assertIn("Splitting into", output)
                 self.assertIn("Run #1 PASSED", output)
                 print("\nAccountability test passed: Missing test detected and recovered (smart split).")

    @patch("parallel_test_runner.parse_gtest_xml")
    @patch("parallel_test_runner.time.sleep")
    @patch("parallel_test_runner.wait")
    def test_runtime_exclusion_flow(self, mock_wait, mock_sleep, mock_parse_xml):
        # Scenario:
        # Run 1: TestA crashes. Deep verification passes.
        # Run 2: TestA should be excluded (not run). TestB passes.
        # Filter file should NOT be updated.

        def side_effect_wait(futures, return_when=None):
            return list(futures), []
        mock_wait.side_effect = side_effect_wait

        # Mock pool
        def pool_run_side_effect(cmd):
            if "--gtest_list_tests" in cmd:
                 # Initial listing
                 return (True, "Suite.\n  TestA\n  TestB", "", 0)

            # Deep verification call for TestA
            if "--gtest_repeat" in cmd and "--gtest_filter=Suite.TestA" in cmd:
                return (True, "", "", 0) # Pass deep verify

            return (True, "", "", 0)

        self.mock_pool.run.side_effect = pool_run_side_effect

        call_count = {"val": 0}
        def run_cmd_async_dynamic(*args, **kwargs):
            future = MagicMock()
            count = call_count["val"]
            call_count["val"] += 1

            if count == 0:
                # Run 1 Batch: TestA crashes (executed but no XML? or just return code failure)
                # Let's say TestA "crashes" by having return code failure and no XML
                 failures = {"CRASH": ["Suite.TestA"]}
                 logs = {"Suite.TestA": "Crash"}
                 executed = {"Suite.TestA"}
                 passed = False
                 expected = {"Suite.TestA", "Suite.TestB"}
                 future.result.return_value = (failures, logs, None, executed, passed, "xml", "flag", expected)
            else:
                # Run 2 Batch: Should ONLY contain TestB
                # If the logic works, TestA is filtered out.
                 passed = True
                 executed = {"Suite.TestB"}
                 expected = {"Suite.TestB"}
                 # We assert that ONLY TestB is expected
                 future.result.return_value = ({}, {}, None, executed, passed, "xml", "flag", expected)
            return future

        self.mock_pool.run_cmd_async.side_effect = run_cmd_async_dynamic
        self.mock_pool.num_workers = 1

        test_args = [
            "parallel_test_runner.py",
            self.binary_path,
            "--filter-file", self.filter_file,
            "--jobs", "1",
            "--consecutive-flake-free", "1",
            "--crash-retry-count", "2"
        ]

        # Ensure filter file is empty/exists
        with open(self.filter_file, 'w') as f:
            json.dump({"failing_tests": []}, f)

        with patch.object(sys, 'argv', test_args):
            with patch('sys.stdout', new_callable=io.StringIO) as fake_out:
                 try:
                    parallel_test_runner.main()
                 except SystemExit as e:
                    self.assertEqual(e.code, 0)

                 output = fake_out.getvalue()
                 self.assertIn("Treating as FLAKE (Runtime Exclusion)", output)
                 self.assertIn("Run #1 FAILED", output) # Failed because found runtime flake
                 self.assertIn("Run #2 PASSED", output)

        # CHECK: Filter file should NOT contain Suite.TestA
        with open(self.filter_file, 'r') as f:
            data = json.load(f)
            self.assertNotIn("Suite.TestA", data["failing_tests"])


class TestDetectCrashes(unittest.TestCase):
    def test_no_split(self):
        # Normal execution
        expected = {"S.A", "S.B"}
        executed = {"S.A", "S.B"}
        passed = True
        failures = {}
        last_test = "S.B"

        should_split, reason, culprit = detect_crashes(expected, executed, passed, failures, last_test)
        self.assertFalse(should_split)

    def test_failures_no_crash(self):
        # Failures present, XML parsed ok
        expected = {"S.A", "S.B"}
        executed = {"S.A", "S.B"}
        passed = False
        failures = {"FAIL": ["S.A"]}
        last_test = "S.B"

        should_split, reason, culprit = detect_crashes(expected, executed, passed, failures, last_test)
        self.assertFalse(should_split)

    def test_missing_executed(self):
        # Crash mid-batch
        expected = {"S.A", "S.B", "S.C"}
        executed = {"S.A"}
        passed = False
        failures = {}
        last_test = "S.B" # Crashed at B

        should_split, reason, culprit = detect_crashes(expected, executed, passed, failures, last_test)
        self.assertTrue(should_split)
        self.assertIn("tests missing", reason)
        self.assertEqual(culprit, "S.B")

    def test_fail_no_xml_failures(self):
        # Return code non-zero, no failures in XML (maybe segfault with no output?)
        expected = {"S.A", "S.B"}
        executed = {"S.A", "S.B"}
        passed = False
        failures = {}
        last_test = "S.B"

        should_split, reason, culprit = detect_crashes(expected, executed, passed, failures, last_test)
        self.assertTrue(should_split)
        self.assertIn("Batch failed with no recorded failures", reason)

class TestTestScheduler(unittest.TestCase):
    def setUp(self):
        self.suites = {
            "S1.": [f"T{i}" for i in range(100)],
            "S2.": [f"T{i}" for i in range(50)],
        }
        self.scheduler = TestScheduler(self.suites, num_workers=10)

    def test_initial_batching(self):
        # 150 tests, 10 workers.
        # Load balance: 150 / (10 * 4) = 3.75 -> 3
        # Crash rate 0 -> target_crash_opt huge.
        # Logic: if crash_rate < 0.05: calculated_size = max(10, calculated_size)
        # So it should be 10.
        batch = self.scheduler.get_next_batch()
        self.assertTrue(batch)

        count = sum(len(tests) for s, tests in batch)
        self.assertEqual(count, 10)

    def test_adaptive_sizing_high_crash_rate(self):
        # Simulate high crash rate
        # 50 crashes in 100 tests = 50% crash rate
        self.scheduler.report_result(crash_count=50, test_count=100)
        self.assertEqual(self.scheduler.get_crash_rate(), 0.5)

        # Target: 0.5 / 0.5 = 1 test per batch
        batch = self.scheduler.get_next_batch()
        count = sum(len(tests) for s, tests in batch)
        self.assertEqual(count, 1)

    def test_priority_batches(self):
        p_batch = [("S9.", ["CriticalTest"])]
        self.scheduler.add_priority_batches([p_batch])

        next_b = self.scheduler.get_next_batch()
        self.assertEqual(next_b, p_batch)

if __name__ == '__main__':
    unittest.main()
