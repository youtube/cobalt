#!/usr/bin/env python3
"""
Runs GoogleTest suites in parallel with granular retry and filter support,
wrapping execution in a Docker container with Xvfb.
"""

import argparse
import collections
import json
import multiprocessing
import os
import re
import shlex
import signal
import subprocess
import sys
import time
import uuid
import queue
import atexit
import tempfile
import functools
import datetime
import xml.etree.ElementTree as ET
import fnmatch
from concurrent.futures import ThreadPoolExecutor, wait, as_completed
from typing import List, Set, Dict, Tuple, Optional, Deque, Union
import math

# Ensure we can import docker_worker_pool from the same directory
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from docker_worker_pool import DockerWorkerPool

class LocalWorkerPool:
    def __init__(self, num_workers: int, cwd: str):
        self.num_workers = num_workers
        self.cwd = cwd
        self._executor = ThreadPoolExecutor(max_workers=num_workers)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.shutdown()

    def shutdown(self):
        self._executor.shutdown(wait=False)

    def run(self, cmd: List[str], timeout: Optional[int] = None) -> Tuple[bool, str, str, int]:
        try:
            print(f"Running locally: {' '.join(cmd)}")
            sys.stdout.flush()
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, cwd=self.cwd)
            return True, result.stdout, result.stderr, result.returncode
        except subprocess.TimeoutExpired as e:
            return False, e.stdout if e.stdout else "", e.stderr if e.stderr else "", -2
        except Exception as e:
            return False, "", str(e), -1

    def run_cmd_async(self, cmd, result_parser=None, timeout=None):
        def _task():
            success, stdout, stderr, retcode = self.run(cmd, timeout=timeout)
            if result_parser:
                return result_parser(retcode, stdout, stderr)
            return success, stdout, stderr, retcode
        return self._executor.submit(_task)

def condense_failures(failures: List[str], all_tests_map: Dict[str, List[str]], existing_filters: Optional[Set[str]] = None) -> List[str]:
    """
    Condenses a list of failed tests into wildcard patterns.
    """
    if not failures and not existing_filters:
        return []

    failures_set = set(failures)
    suite_to_failures = collections.defaultdict(list)
    used_filters = set()

    if existing_filters:
        # Expand existing filters to specific tests
        for suite, tests in all_tests_map.items():
            for t in tests:
                full_name = f"{suite}{t}"
                for pattern in existing_filters:
                    # fnmatch is designed for filenames, but works well for wildcard matching here.
                    # We need to handle the suite prefix carefully.
                    # existing_filters usually have 'Suite.Name' or 'Suite.*'.

                    # Pattern might be 'Suite.*', full_name is 'Suite.TestA'
                    if fnmatch.fnmatch(full_name, pattern):
                        failures_set.add(full_name)
                        used_filters.add(pattern)

    # Re-list from set
    failures = list(failures_set)

    suite_to_failures = collections.defaultdict(list)
    for f in failures:
        if '.' in f:
            suite, name = f.split('.', 1)
            suite_to_failures[suite + '.'].append(name)
        else:
            suite_to_failures[''].append(f)

    condensed = []

    for suite, failed_names in suite_to_failures.items():
        all_names = all_tests_map.get(suite, [])
        if not all_names:
            condensed.extend([f"{suite}{n}" for n in failed_names])
            continue

        failed_names_set = set(failed_names)
        non_failed_names = [n for n in all_names if n not in failed_names_set]

        if not non_failed_names:
            condensed.append(f"{suite}*")
            continue

        remaining_failures = set(failed_names)

        while remaining_failures:
            best_pattern = None
            best_score = (-1, -float('inf'))
            best_covered_set = set()

            candidates = set()
            for name in remaining_failures:
                candidates.add(name)
                for i in range(1, len(name) + 1):
                    is_boundary = False
                    if i < len(name):
                        char = name[i]
                        prev = name[i-1]
                        if char.isupper() or char == '_' or (char.isdigit() and not prev.isdigit()):
                            is_boundary = True
                        if i > 0 and prev == '_':
                            is_boundary = True

                    if is_boundary:
                        candidates.add(name[:i] + '*')

            for pat in candidates:
                if pat.endswith('*'):
                    prefix = pat[:-1]
                    matches_forbidden = any(nf.startswith(prefix) for nf in non_failed_names)
                else:
                    matches_forbidden = pat in non_failed_names

                if matches_forbidden:
                    continue

                covered = set()
                if pat.endswith('*'):
                    prefix = pat[:-1]
                    for f in remaining_failures:
                        if f.startswith(prefix):
                            covered.add(f)
                else:
                    if pat in remaining_failures:
                        covered.add(pat)

                count = len(covered)
                if count == 0:
                    continue

                score = (count, -len(pat))
                if score > best_score:
                    best_score = score
                    best_pattern = pat
                    best_covered_set = covered

            if best_pattern:
                condensed.append(f"{suite}{best_pattern}")
                remaining_failures -= best_covered_set
            else:
                for f in list(remaining_failures):
                    condensed.append(f"{suite}{f}")
                break

                for f in list(remaining_failures):
                    condensed.append(f"{suite}{f}")
                break

    # Add back any filters that weren't applicable to the current set of tests
    if existing_filters:
        unused_filters = existing_filters - used_filters
        condensed.extend(unused_filters)

    return sorted(condensed)


def load_filters(filter_file: str) -> Set[str]:
    """Loads failing tests from a JSON filter file."""
    if not os.path.exists(filter_file):
        return set()

    try:
        with open(filter_file, 'r') as f:
            data = json.load(f)
            filters = set(data.get('failing_tests', []))
            return filters
    except Exception as e:
        print(f"Warning: Failed to load filter file {filter_file}: {e}")
        return set()

def update_filter_file(filter_file: str, new_failures: List[str], replace: bool = False):
    """Updates the filter file with new failures. If replace is True, overwrites existing failures."""
    # Consolidate filters so no filter matches existing ones.

    if not os.path.exists(filter_file):
        # print(f"Filter file {filter_file} does not exist. creating it.")
        data = {"comment": "List of failing unit tests.", "failing_tests": []}
    else:
        try:
            with open(filter_file, 'r') as f:
                data = json.load(f)
        except Exception as e:
            print(f"Error reading filter file {filter_file}: {e}")
            return

    # Ensure keys exist
    if 'failing_tests' not in data: data['failing_tests'] = []

    if replace:
        current_items = set(new_failures)
        updated = True
    else:
        current_items = set(data.get('failing_tests', []))
        updated = False

        for f in new_failures:
            if f not in current_items:
                current_items.add(f)
                updated = True

    if updated:
        data['failing_tests'] = sorted(list(current_items))
        try:
            with open(filter_file, 'w') as f:
                json.dump(data, f, indent=2)
                f.write('\n')
            # print(f"\nUpdated filter file: {filter_file}")
        except Exception as e:
            print(f"Error writing filter file {filter_file}: {e}")
    else:
        # print(f"Filter file already up to date.")
        pass

def get_tests(pool: DockerWorkerPool, binary_path: str, cwd: str, gtest_filter: Optional[str] = None, extra_args: List[str] = None) -> Dict[str, List[str]]:
    """
    Lists all tests from the binary using gtest_list_tests.
    Returns a dict mapping suite names to test lists.
    """
    args = [binary_path, '--gtest_list_tests']
    if extra_args:
        args.extend(extra_args)

    flag_file_path = None
    if gtest_filter:
        # Use a flagfile to avoid command line length limits
        flag_filename = f".gtest_filter_{uuid.uuid4().hex}.flags"
        flag_file_path = os.path.join(cwd, flag_filename)
        try:
            with open(flag_file_path, 'w') as f:
                f.write(f"--gtest_filter={gtest_filter}")
            args.append(f'--gtest_flagfile={flag_filename}')
        except IOError as e:
            print(f"Error creating flagfile: {e}")
            sys.exit(1)

    try:
        success, stdout, stderr, returncode = pool.run(args)
        if not success:
             raise subprocess.CalledProcessError(returncode, args, output=stdout, stderr=stderr)
    except subprocess.CalledProcessError as e:
        print(f"Error listing tests: {e}")
        print(e.stderr)
        pool.shutdown()
        sys.exit(1)
    finally:
        if flag_file_path and os.path.exists(flag_file_path):
            try:
                os.remove(flag_file_path)
            except OSError:
                pass

    suites = {}
    current_suite = None

    for line in stdout.splitlines():
        # Clean line of comments
        clean_line = line.split('#')[0].strip()
        if not clean_line:
            continue

        if line.startswith('  '):
            if current_suite:
                suites[current_suite].append(clean_line)
        else:
            if clean_line.endswith('.'):
                current_suite = clean_line
                suites[current_suite] = []
            else:
                # Should not happen in standard GTest output, but just in case
                current_suite = None

    return suites


def chunk_tests(suites_to_run: Dict[str, List[str]], batch_size_tests: int = 100) -> List[List[Tuple[str, List[str]]]]:
    """
    Chunks tests into batches.
    Returns list of batches, where each batch is a list of (suite_name, tests).
    """
    all_flat = []
    for suite, tests in suites_to_run.items():
        for t in tests:
            all_flat.append((suite, t))

    batches = []
    for i in range(0, len(all_flat), batch_size_tests):
        chunk = all_flat[i:i + batch_size_tests]
        suite_map = collections.defaultdict(list)
        for s, t in chunk:
            suite_map[s].append(t)
        batches.append(list(suite_map.items()))

    return batches

def split_batch(batch: List[Tuple[str, List[str]]], n_splits: int = 2) -> List[List[Tuple[str, List[str]]]]:
    """
    Splits a batch into n_splits smaller batches.
    Flatten structure -> split -> reconstruct.
    """
    all_tests = []
    for suite, tests in batch:
        for t in tests:
            all_tests.append((suite, t))

    total = len(all_tests)
    if total <= 1:
        return [batch]

    n_splits = min(n_splits, total)
    if n_splits < 2:
        n_splits = 2

    chunks = []
    k, m = divmod(total, n_splits)

    start = 0
    for i in range(n_splits):
        end = start + k + (1 if i < m else 0)
        chunks.append(all_tests[start:end])
        start = end

    def reconstruct(flat_tests):
        grouped = collections.defaultdict(list)
        for s, t in flat_tests:
            grouped[s].append(t)
        return list(grouped.items())

    return [reconstruct(chunk) for chunk in chunks if chunk]

def split_batch_smart(batch: List[Tuple[str, List[str]]], failed_test: Optional[str] = None, n_splits_fallback: int = 4) -> List[List[Tuple[str, List[str]]]]:
    """
    Splits a batch based on a failure point.
    - Tests BEFORE failure: Kept as one batch.
    - Test AT failure: Singleton batch.
    - Tests AFTER failure (SAME suite): Singleton batches.
    - Tests AFTER failure (DIFF suite): Kept as one batch.
    """
    if not failed_test:
        return split_batch(batch, n_splits=n_splits_fallback)

    new_batches = []

    def reconstruct_batch(flat_tests: List[Tuple[str, str]]) -> List[Tuple[str, List[str]]]:
        grouped = collections.defaultdict(list)
        for s, t in flat_tests:
            grouped[s].append(t)
        return list(grouped.items())

    # Flatten first
    all_tests = []
    for suite, tests in batch:
        for t in tests:
            all_tests.append((suite, t))

    pre_batch = []
    post_diff_suite_batch = []

    found = False
    failed_suite = None

    for suite, name in all_tests:
        full_name = f"{suite}{name}"

        if not found:
            if full_name == failed_test:
                found = True
                failed_suite = suite

                # Push pre_batch if exists
                if pre_batch:
                    new_batches.append(reconstruct_batch(pre_batch))

                # Push failed test as singleton
                new_batches.append(reconstruct_batch([(suite, name)]))
            else:
                pre_batch.append((suite, name))
        else:
            # We are after failure
            if suite == failed_suite:
                 # Same suite -> Singleton
                 new_batches.append(reconstruct_batch([(suite, name)]))
            else:
                 # Diff suite -> Accumulate
                 post_diff_suite_batch.append((suite, name))

    if post_diff_suite_batch:
        new_batches.append(reconstruct_batch(post_diff_suite_batch))

    if not new_batches:
        # Should not happen if logic is correct, but safe fallback
        return split_batch(batch, n_splits=n_splits_fallback)

    return new_batches


def detect_crashes(expected_tests: Set[str], executed_tests: Set[str], passed: bool, failures: Dict[str, List[str]], last_test: Optional[str]) -> Tuple[bool, str, Optional[str]]:
    """
    Analyzes execution results to detect crashes that require splitting.
    Returns (should_split, reason, likely_culprit).
    """
    missing_in_batch = expected_tests - executed_tests

    if len(expected_tests) > 1:
        if missing_in_batch:
             # Some tests didn't run -> Crash mid-batch
             return True, f"{len(missing_in_batch)} tests missing (Crashed mid-batch)", last_test
        elif not passed and not failures:
             # Batch failed (non-zero return) but no XML failures -> Crash/Timeout
             return True, "Batch failed with no recorded failures (Crash/Timeout?)", last_test

    return False, "", None


class TestScheduler:
    """
    Dynamically schedules tests into batches based on worker availability and crash rates.
    """
    def __init__(self, suites_map: Dict[str, List[str]], num_workers: int):
        self.all_tests = []
        for suite, tests in suites_map.items():
            for t in tests:
                self.all_tests.append((suite, t))

        self.queue = collections.deque(self.all_tests)
        self.priority_batches: Deque[List[Tuple[str, List[str]]]] = collections.deque()
        self.num_workers = num_workers

        # Statistics for adaptive sizing
        self.total_tests_finished = 0
        self.total_crashes = 0

    def report_result(self, crash_count: int, test_count: int):
        self.total_crashes += crash_count
        self.total_tests_finished += test_count

    def get_crash_rate(self) -> float:
        if self.total_tests_finished == 0:
            return 0.0
        return self.total_crashes / self.total_tests_finished

    def add_priority_batches(self, batches: List[List[Tuple[str, List[str]]]]):
        """Add specific batched tests (e.g. splits) to the front of the line."""
        for b in reversed(batches):
             self.priority_batches.appendleft(b)

    def get_next_batch(self) -> Optional[List[Tuple[str, List[str]]]]:
        if self.priority_batches:
            return self.priority_batches.popleft()

        if not self.queue:
            return None

        remaining = len(self.queue)

        # 1. Load Optimization: Keep all workers busy for ~4 rounds
        target_load_balance = max(1, remaining // (self.num_workers * 4))

        # 2. Crash Optimization: Reduce size if crashes are frequent
        # Heuristic: Aim for < 0.5 crashes per batch to maximize probability of crash-free runs
        crash_rate = self.get_crash_rate()
        if crash_rate > 0:
            target_crash_opt = int(0.5 / crash_rate)
            target_crash_opt = max(1, target_crash_opt)
        else:
            target_crash_opt = 1000000

        # Combine
        calculated_size = min(target_load_balance, target_crash_opt)

        # Clamp settings
        # If crash rate is low (< 5%), enforce a minimum efficient batch size (10)
        # If crash rate is high, allow dropping to 1 to isolate crashes execution
        if crash_rate < 0.05:
            calculated_size = max(10, calculated_size)

        batch_size = max(1, min(calculated_size, remaining))

        # Construct batch
        batch_flat = []
        for _ in range(batch_size):
            batch_flat.append(self.queue.popleft())

        # Reconstruct into format
        suite_map = collections.defaultdict(list)
        for s, t in batch_flat:
            suite_map[s].append(t)

        return list(suite_map.items())


def parse_gtest_xml(xml_path: str) -> Tuple[List[str], List[str], Set[str]]:
    """
    Parses GTest XML to find failed tests.
    Returns (failed_tests, logs, executed_tests).
    """
    failed_tests = []
    logs = []
    executed_tests = set()
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()
        for testsuite in root.findall('testsuite'):
            suite_name = testsuite.get('name')
            for testcase in testsuite.findall('testcase'):
                name = testcase.get('name')
                full_name = f"{suite_name}.{name}"
                executed_tests.add(full_name) # Track execution

                failure = testcase.find('failure')
                if failure is not None:
                    failed_tests.append(full_name)
                    msg = failure.get('message', 'Unknown failure')
                    logs.append(f"[{full_name}] FAILED\n{msg}")
    except ET.ParseError as e:
        pass # XML might be empty or malformed on crash
    except Exception as e:
        print(f"Unexpected error reading XML {xml_path}: {e}")

    return failed_tests, logs, executed_tests

def find_last_running_test(stdout: Union[str, bytes]) -> Optional[str]:
    """
    Finds the last test that started running based on stdout.
    """
    if isinstance(stdout, bytes):
        stdout = stdout.decode('utf-8', errors='replace')
    matches = re.findall(r"\[\s+RUN\s+\]\s+(.+)", stdout)
    if matches:
        # Capture the whole line after RUN, then strip comment and spaces
        raw_name = matches[-1]
        return raw_name.split('#')[0].strip()
    return None

def construct_test_command(batch: List[Tuple[str, List[str]]], cwd: str, temp_dir: str, binary_path: str, extra_args: List[str] = None) -> Tuple[List[str], str, str, Set[str]]:
    """
    Constructs the GTest command and necessary flag files.
    Returns (cmd_list, xml_path, flag_path, expected_tests_set).
    """
    xml_fd, xml_path = tempfile.mkstemp(suffix=".xml", prefix="test_results_", dir=temp_dir)
    os.close(xml_fd)

    flag_fd, flag_file_path = tempfile.mkstemp(suffix=".flags", prefix=".gtest_batch_", dir=temp_dir)

    expected_tests_in_batch = set()
    filter_parts = []

    for suite, tests in batch:
        for t in tests:
            full_name = f"{suite}{t}"
            expected_tests_in_batch.add(full_name)
            filter_parts.append(full_name)

    filter_pattern = ":".join(filter_parts)
    with os.fdopen(flag_fd, 'w') as f:
        f.write(f"--gtest_filter={filter_pattern}")

    # Use relative paths for Docker transparency if possible, but absolute inside container is safer
    # if we are sure about mounts.
    # The existing code used os.path.relpath(xml_path, cwd).
    # We will stick to that to minimize regression during refactor.
    rel_xml_path = os.path.relpath(xml_path, cwd)
    rel_flag_path = os.path.relpath(flag_file_path, cwd)

    cmd = [
        binary_path,
        f'--gtest_flagfile={rel_flag_path}',
        '--single-process-tests',
        f'--gtest_output=xml:{rel_xml_path}'
    ]
    if extra_args:
        cmd.extend(extra_args)

    return cmd, xml_path, flag_file_path, expected_tests_in_batch

def parse_gtest_results(returncode: int, stdout: str, stderr: str, xml_path: str, flag_path: str, expected_tests: Set[str]) -> Tuple[Dict[str, List[str]], Dict[str, str], Optional[str], Set[str], bool, str, str, Set[str]]:
    """
    Parses execution results.
    Returns (failures_by_type, log_by_test, last_running_test, executed_tests, passed, xml_path, flag_path, expected_tests).
    """
    failures = collections.defaultdict(list)
    logs = {}
    last_running_test = find_last_running_test(stdout)
    passed = (returncode == 0)
    executed_tests = set()

    # Check for Timeout
    if returncode == -2:
        passed = False
        if len(expected_tests) == 1:
                failed_test = list(expected_tests)[0]
                failures["TIMEOUT"].append(failed_test)
                logs[failed_test] = f"[TIMEOUT] Test timed out.\n{stderr[-1000:]}"
                executed_tests.add(failed_test)

        # If last_running_test is found, strictly it started execution
        if last_running_test:
            executed_tests.add(last_running_test)

        return failures, logs, last_running_test, executed_tests, passed, xml_path, flag_path, expected_tests

    # Check XML
    if os.path.exists(xml_path):
        xml_failures, xml_logs, xml_executed = parse_gtest_xml(xml_path)
        executed_tests.update(xml_executed)

        if xml_failures:
            failures["FAIL"] = xml_failures
            for f, l in zip(xml_failures, xml_logs):
                logs[f] = l
            # If XML has failures, passed is explicitly False even if returncode was 0 (unlikely)
            passed = False

        # If XML exists and no failures, passed should be True if returncode is 0.
        # GTest usually returns non-zero if tests fail, but we rely on XML.
        return failures, logs, last_running_test, executed_tests, passed, xml_path, flag_path, expected_tests

    # No XML or Crash
    if not executed_tests or returncode != 0:
        passed = False
        if len(expected_tests) == 1:
            failed_test = list(expected_tests)[0]
            if failed_test not in failures["CRASH"] and failed_test not in failures["FAIL"]:
                failures["CRASH"].append(failed_test)
                logs[failed_test] = f"[CRASH] Return code {returncode}. No tests executed.\n{stdout[-1000:]}\n{stderr[-1000:]}"
            executed_tests.add(failed_test)
        elif last_running_test:
             # We crashed, but we know at least this one started
             if last_running_test not in executed_tests:
                 executed_tests.add(last_running_test)

    return failures, logs, last_running_test, executed_tests, passed, xml_path, flag_path, expected_tests


def main():
    parser = argparse.ArgumentParser(description="Parallel GTest Runner (Docker wrapper)")
    parser.add_argument("binary", help="Path to gtest binary")
    parser.add_argument("--filter-file", help="Path to filter JSON", default=None)
    parser.add_argument("--jobs", "-j", type=int, default=8, help="Parallel jobs")
    parser.add_argument("--timeout", type=int, default=5, help="Timeout per test batch in seconds")
    parser.add_argument("--dump-tests", help="Dump all tests to JSON file and exit")
    parser.add_argument("--batch-size", type=int, default=200, help="Target number of tests per batch")
    parser.add_argument("--docker-image", default="ghcr.io/youtube/cobalt/unittest:main", help="Docker image to use")
    parser.add_argument("--gtest-filter", help="Gtest filter to select tests")
    parser.add_argument("--consecutive-flake-free", type=int, default=1, help="Number of consecutive flake-free runs required")
    parser.add_argument("--flake-retries", type=int, default=5, help="Number of times to retry a suspected flake concurrently")
    parser.add_argument("--crash-retry-count", type=int, default=100, help="Number of retries for crashing tests in isolation")
    parser.add_argument("--no-docker", action="store_true", help="Run tests locally instead of in Docker")

    args, extra_args = parser.parse_known_args()

    cwd = os.getcwd()
    log_dir = os.path.join(cwd, "test_logs")
    os.makedirs(log_dir, exist_ok=True)
    binary_path = args.binary
    abs_binary_path = os.path.abspath(binary_path)
    if not os.path.exists(abs_binary_path):
        print(f"Error: Binary {abs_binary_path} not found.")
        sys.exit(1)

    # Create base temp dir that is mounted in Docker
    base_temp_dir = os.path.join(cwd, "src/cobalt/build/testing/active_test_runners")
    os.makedirs(base_temp_dir, exist_ok=True)

    pool_class = LocalWorkerPool if args.no_docker else lambda n, c, i: DockerWorkerPool(n, c, i)
    pool_args = (args.jobs, cwd) if args.no_docker else (args.jobs, cwd, args.docker_image)

    with pool_class(*pool_args) as pool, tempfile.TemporaryDirectory(prefix="gtest_runner_", dir=base_temp_dir) as temp_dir:
        filter_file = args.filter_file
        if not filter_file:
            base_name = os.path.basename(binary_path)
            script_dir = os.path.dirname(os.path.abspath(__file__))
            filters_base_dir = os.path.join(script_dir, "../../testing/filters")

            # Try to detect platform from binary path
            detected_platform = "linux-x64x11" # Default
            if os.path.exists(filters_base_dir):
                available_platforms = [d for d in os.listdir(filters_base_dir) if os.path.isdir(os.path.join(filters_base_dir, d))]
                # Sort by length descending to match longer specific names first (e.g. android-arm64 before android-arm if applicable)
                available_platforms.sort(key=len, reverse=True)

                # Check if binary path contains the platform name
                # We normalize/lower to be safe, though paths usually match case on Linux
                full_binary_path_str = os.path.abspath(binary_path)
                for plat in available_platforms:
                    # Heuristic: check if platform name is in the path elements
                    # e.g. out/android-arm_devel/... contains android-arm
                    if plat in full_binary_path_str:
                        detected_platform = plat
                        break

            potential_path = os.path.join(filters_base_dir, detected_platform, f"{base_name}_filter.json")
            if os.path.exists(potential_path):
                filter_file = potential_path
                print(f"Auto-detected filter file: {filter_file}")

        skipped_tests_patterns = load_filters(filter_file) if filter_file else set()

        gtest_filter_arg = None
        if skipped_tests_patterns:
            print(f"Loaded {len(skipped_tests_patterns)} skip patterns.")
            negative_filter = ":".join(skipped_tests_patterns)
            gtest_filter_arg = f"-{negative_filter}"

        print(f"Listing tests from {binary_path} ({'locally' if args.no_docker else 'in Docker'}, filtered)...")
        sys.stdout.flush()
        suites_map = get_tests(pool, binary_path, cwd, gtest_filter=args.gtest_filter if args.gtest_filter else gtest_filter_arg, extra_args=extra_args)
        print(f"Done listing tests.")
        sys.stdout.flush()

        if args.dump_tests:
            with open(args.dump_tests, 'w') as f:
                json.dump(suites_map, f, indent=2)
            print(f"Tests dumped to {args.dump_tests}")
            sys.exit(0)

        suites_to_run = {}
        total_tests = 0
        for suite, tests in suites_map.items():
            if tests:
                suites_to_run[suite] = tests
                total_tests += len(tests)

        print(f"Found {len(suites_to_run)} suites with {total_tests} tests to run.")
        sys.stdout.flush()

        consecutive_successes = 0
        target_successes = args.consecutive_flake_free
        run_number = 0
        runtime_excluded_tests = set()

        while consecutive_successes < target_successes:
            run_number += 1
            print(f"\n{'='*60}")
            print(f"RUN #{run_number} (Consecutive Successes: {consecutive_successes}/{target_successes})")
            print(f"{'='*60}")

            all_failures = collections.defaultdict(list) # "FAIL": [], "CRASH": [], "TIMEOUT": []
            all_executed_tests = set()
            deep_verify_tests = set()
            found_runtime_flake = False

            # --- DYNAMIC EXECUTION LOOP ---
            scheduler = TestScheduler(suites_to_run, args.jobs)

            futures = set()
            future_to_batch = {}

            def submit_next():
                # Keep pipe full: aim for 2 * jobs active futures
                while len(futures) < args.jobs * 2:
                    next_batch = scheduler.get_next_batch()
                    if not next_batch:
                        break

                    cmd, xml_path, flag_path, expected = construct_test_command(next_batch, cwd, temp_dir, binary_path, extra_args=extra_args)
                    parser = functools.partial(parse_gtest_results, xml_path=xml_path, flag_path=flag_path, expected_tests=expected)
                    # Use slightly higher timeout for larger batches if we wanted, but static timeout is per user request
                    f = pool.run_cmd_async(cmd, result_parser=parser, timeout=args.timeout)
                    futures.add(f)
                    future_to_batch[f] = next_batch

            # Initial seed
            submit_next()

            completed_count = 0
            # Total batches is dynamic now, but we can track processed chunks

            start_time = time.time()

            try:
                while futures:
                    done, _ = wait(futures, return_when="FIRST_COMPLETED")
                    for future in done:
                        futures.remove(future)
                        completed_count += 1
                        batch = future_to_batch.pop(future)

                        try:
                            result = future.result()
                        except Exception as e:
                            print(f"Worker exception: {e}")
                            import traceback
                            traceback.print_exc()
                            print(f" Resubmitting lost batch...")
                            scheduler.add_priority_batches([batch]) # Requeue whole batch
                            continue

                        failures, logs, last_test, executed, passed, xml_to_clean, flag_to_clean, expected = result

                        # Cleanup
                        for f_path in [xml_to_clean, flag_to_clean]:
                            if os.path.exists(f_path):
                                try: os.remove(f_path)
                                except: pass

                        all_executed_tests.update(executed)

                        # Check for Crashes needing Split
                        should_split, split_reason, culprit = detect_crashes(expected, executed, passed, failures, last_test)

                        # Update Stats
                        scheduler.report_result(1 if should_split else 0, len(executed))

                        # Handle Failures
                        for cat, tests in failures.items():
                             if cat in ["CRASH", "TIMEOUT"] and args.crash_retry_count > 0:
                                 # Divert singleton crashes to deep verification
                                 for t in tests:
                                     print(f" [CRASH DETECTED] {t}. Queueing for deep verification.")
                                     deep_verify_tests.add(t)
                                 continue
                             all_failures[cat].extend(tests)

                             if filter_file: # Lazy consolidated update
                                 current_filters = load_filters(filter_file)
                                 condensed = condense_failures(all_failures[cat], suites_map, current_filters)
                                 update_filter_file(filter_file, condensed, replace=True)

                        for test_name, log in logs.items():
                            # Save log to file instead of printing
                            safe_name = test_name.replace('/', '_').replace('.', '_')
                            log_path = os.path.join(log_dir, f"{safe_name}.log")
                            timestamp = datetime.datetime.now().isoformat()
                            try:
                                with open(log_path, "a") as lf:
                                    lf.write(f"\n{'='*40}\n")
                                    lf.write(f"Run: {timestamp}\n")
                                    lf.write(f"Test: {test_name}\n")
                                    lf.write(f"{'-'*40}\n")
                                    lf.write(log)
                                    lf.write(f"\n{'='*40}\n")
                                print(f" [FAILURE] {test_name} (Log: {os.path.relpath(log_path, cwd)})")
                            except Exception as e:
                                print(f" [FAILURE] {test_name} (Failed to save log: {e})")

                        if should_split:
                             # Try to blame logic
                             if culprit and culprit not in failures.get("CRASH", []) and culprit not in failures.get("FAIL", []):
                                 all_failures["CRASH"].append(culprit)

                             print(f"\n[BATCH CRASH] {split_reason}. Last started: {culprit}")

                             sub_batches = split_batch_smart(batch, culprit, n_splits_fallback=max(pool.num_workers // 4, 2))
                             if sub_batches and (len(sub_batches) > 1 or sub_batches[0] != batch):
                                 print(f" Splitting into {len(sub_batches)} sub-batches (Priority Queue)...")
                                 scheduler.add_priority_batches(sub_batches)
                             else:
                                 print(f" [WARNING] Could not split batch further. Recording all tests/remainder as CRASH.")
                                 for s, tests in batch:
                                     for t in tests:
                                         all_failures["CRASH"].append(f"{s}{t}")

                    # Refill queue
                    submit_next()

                    count_failures = sum(len(v) for v in all_failures.values())
                    if completed_count % 10 == 0:
                        sys.stdout.write(f"\rBatch Processing... Completed: {completed_count}. Failures: {count_failures}. Crash Rate: {scheduler.get_crash_rate():.1%}")
                        sys.stdout.flush()

            except KeyboardInterrupt:
                print("\nInterrupted by user.")
                sys.exit(1)

            print(f"\nBatch processing finished. Total Crashes: {scheduler.total_crashes}")

            # Deep Verification Phase
            if deep_verify_tests:
                print(f"\n{'='*60}")
                print(f"DEEP VERIFICATION: Retrying {len(deep_verify_tests)} crashed tests {args.crash_retry_count}x sequentially...")
                print(f"{'='*60}")

                for t in sorted(list(deep_verify_tests)):
                    print(f" Verifying {t}...", end='', flush=True)

                    retry_args = [
                        binary_path,
                        f"--gtest_filter={t}",
                        f"--gtest_repeat={args.crash_retry_count}",
                        "--gtest_break_on_failure"
                    ]
                    if extra_args:
                        retry_args.extend(extra_args)

                    # Run synchronously to ensure low parallelism
                    success, stdout, stderr, retcode = pool.run(retry_args)

                    if success and retcode == 0:
                        print(f" PASSED (survived {args.crash_retry_count} retries). Treating as FLAKE (Runtime Exclusion).")
                        runtime_excluded_tests.add(t)
                        found_runtime_flake = True
                    else:
                        print(f" FAILED/CRASHED. Confirmed failure.")
                        all_failures["CRASH"].append(t)

            duration = time.time() - start_time
            print(f"\nRun #{run_number} completed in {duration:.2f} seconds.")

            if any(all_failures.values()) or found_runtime_flake:
                print(f"\nRun #{run_number} FAILED with {sum(len(v) for v in all_failures.values())} failures and {int(found_runtime_flake)} runtime flakes. Resetting consecutive counter.")
                consecutive_successes = 0

                for category, tests in all_failures.items():
                    if tests:
                        if filter_file:
                            current_filters = load_filters(filter_file)
                            condensed = condense_failures(tests, suites_map, current_filters)
                            update_filter_file(filter_file, condensed, replace=True)

                if filter_file:
                    print("Refreshing test list with updated filters...")
                    skipped_tests_patterns = load_filters(filter_file)
                    negative_filter = ":".join(skipped_tests_patterns)
                    gtest_filter_arg = f"-{negative_filter}"
                    suites_map = get_tests(pool, binary_path, cwd, gtest_filter=gtest_filter_arg)

                    # Filter out runtime excluded tests
                    suites_to_run = {}
                    for s, tests in suites_map.items():
                        filtered_tests = [t for t in tests if f"{s}{t}" not in runtime_excluded_tests]
                        if filtered_tests:
                            suites_to_run[s] = filtered_tests

            else:
                consecutive_successes += 1
                if consecutive_successes < target_successes:
                    print(f"\nRun #{run_number} PASSED! ({consecutive_successes}/{target_successes}). Rerunning to verify stability...")
                else:
                    print(f"\nRun #{run_number} PASSED! ({consecutive_successes}/{target_successes})")

    if consecutive_successes >= target_successes:
        print(f"\nCompleted {target_successes} consecutive flake-free runs. SUCCESS.")
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()