# Copyright 2026 The Cobalt Authors. All rights reserved.
"""Discovers failed Kokoro jobs and downloads logs in parallel."""

import argparse
import concurrent.futures
import datetime
import glob
import json
import logging
import os
import re
import stat
import subprocess
import sys
import tempfile
from typing import Any, Dict, List, Optional

import getpass

# Get logger for this module
logger = logging.getLogger(__name__)

# Caching Configuration
CACHE_DIR = os.path.join(tempfile.gettempdir(),
                         f"kokoro_triage_{getpass.getuser()}")


def get_cache_path(filename: str) -> str:
  """Returns the absolute path to a file inside the cache directory."""
  # Prevent path traversal
  safe_filename = os.path.basename(filename)
  return os.path.join(CACHE_DIR, safe_filename)


def ensure_cache_dir() -> bool:
  """Ensures the cache directory exists safely with 0700 permissions."""
  try:
    os.makedirs(CACHE_DIR, mode=0o700, exist_ok=True)

    # Secure verification of the directory
    stat_info = os.lstat(CACHE_DIR)
    if stat.S_ISLNK(stat_info.st_mode):
      logger.error("Security violation: Cache directory %s is a symbolic link",
                   CACHE_DIR)
      return False

    if stat_info.st_uid != os.getuid():
      logger.error(
          "Security violation: Cache directory %s is owned by a different user",
          CACHE_DIR)
      return False

    os.chmod(CACHE_DIR, 0o700)
    return True
  except OSError as e:
    logger.error("Error creating cache directory %s: %s", CACHE_DIR, e)
    return False


# Pre-compiled regex for validation and log parsing
INVOCATION_ID_PATTERN = re.compile(r"^[a-zA-Z0-9_-]+$")
FILENAME_PATTERN = re.compile(r"^[a-zA-Z0-9_/.-]+$")
SPONGE_LINK_PATTERN = re.compile(
    r"(?:http://goto\.google\.com/cobalt-on-device-tests/|"
    r"http://sponge2(?:\.corp\.google\.com)?/|"
    r"https://sponge\.corp\.google\.com/invocation\?id=)"
    r"([a-fA-F0-9-]+)")

DEFAULT_TIMEOUT = 30


def _validate_invocation_id(invocation_id: str) -> bool:
  return bool(INVOCATION_ID_PATTERN.match(invocation_id))


def _validate_filename(filename: str) -> bool:
  if ".." in filename or os.path.isabs(filename):
    return False
  return bool(FILENAME_PATTERN.match(filename))


def run_command(cmd: List[str],
                timeout: int = DEFAULT_TIMEOUT) -> subprocess.CompletedProcess:
  """Runs a shell command and returns the CompletedProcess."""
  logger.debug("Running command: %s", " ".join(cmd))
  return subprocess.run(
      cmd, capture_output=True, text=True, check=True, timeout=timeout)


def discover_jobs() -> List[Dict[str, str]]:
  """Discovers Kokoro jobs by searching for GCL files."""

  # Find Kokoro GCL files
  logger.info("Searching for Kokoro jobs via cs...")
  cmd = ["cs", "-l", "-f=nightly.gcl$", "devtools/kokoro/config/prod/cobalt"]
  jobs = []

  try:
    result = run_command(cmd)
    jobs = [
        line.strip()
        for line in result.stdout.splitlines()
        if line.strip() and line.strip().endswith(".gcl")
    ]
  except (subprocess.CalledProcessError, subprocess.TimeoutExpired,
          OSError) as e:
    logger.warning("cs search failed, trying fallback directory: %s", e)
    fallback_dir = ("/google/src/files/head/depot/google3/"
                    "devtools/kokoro/config/prod/cobalt")
    if os.path.exists(fallback_dir):
      pattern = os.path.join(fallback_dir, "**/nightly.gcl")
      jobs = glob.glob(pattern, recursive=True)
    else:
      logger.warning("Fallback directory not found: %s", fallback_dir)

  discovered = []
  for job_path in jobs:
    # Convert path to unique job name and extract branch
    # Example path:
    # .../devtools/kokoro/config/prod/cobalt/main/build/linux/nightly.gcl
    # Unique name: cobalt/main/build/linux/nightly
    # Branch: main (derived from segment after 'cobalt')
    match = re.search(
        r"devtools/kokoro/config/prod/"
        r"(?P<name>cobalt/(?P<branch>[^/]+)/.+)\.gcl", job_path)
    if match:
      discovered.append({
          "job_name": match.group("name"),
          "branch": match.group("branch"),
          "gcl_path": job_path,
      })
    else:
      # Simple fallback
      base = os.path.basename(job_path).replace(".gcl", "")
      discovered.append({
          "job_name": f"cobalt/{base}",
          "branch": "unknown",
          "gcl_path": job_path,
      })
  return discovered


def get_latest_build_status(job_name: str) -> Optional[Dict[str, Any]]:
  """Queries stubby for the latest build status of a job."""

  cmd = [
      "stubby",
      "call",
      "--output_json",
      "blade:kokoro-api",
      "KokoroApi.GetLatestBuild",
      f'full_job_name: "{job_name}"',
  ]
  try:
    result = run_command(cmd)
    raw_status = json.loads(result.stdout)
    if not raw_status:
      return None

    # Translate to format expected by triage_job
    status_val = raw_status.get("status")
    # Mapping BuildStatusResponse.Status enums to FAILURE/SUCCESS/etc.
    # 2 is FAILED, 4 is NOT_BUILT, 5 is ABORTED
    if status_val in (2, 4, 5, "FAILED", "NOT_BUILT", "ABORTED", "FAILURE"):
      translated_status = "FAILED"
    else:
      translated_status = "SUCCESS"

    build_id = raw_status.get("build_id")

    # Parse finish time or start time for age check
    created_at_iso = ""
    finish_time = raw_status.get("build_finish_time", {})
    if isinstance(finish_time, dict) and "seconds" in finish_time:
      try:
        seconds = int(finish_time["seconds"])
        created_at_iso = datetime.datetime.fromtimestamp(
            seconds, datetime.timezone.utc).isoformat().replace("+00:00", "Z")
      except (ValueError, TypeError):
        pass

    if not created_at_iso:
      start_time = raw_status.get("build_start_time")
      if start_time:
        try:
          millis = int(start_time)
          created_at_iso = datetime.datetime.fromtimestamp(
              millis / 1000.0,
              datetime.timezone.utc).isoformat().replace("+00:00", "Z")
        except (ValueError, TypeError):
          pass

    return {
        "status": translated_status,
        "build_id": build_id,
        "createdAt": created_at_iso,
    }
  except (subprocess.SubprocessError, json.JSONDecodeError, OSError,
          AttributeError) as e:
    logger.error("Failed to query status for %s via stubby: %s", job_name, e)
    return None


def get_sponge_log_uri_from_resultstore(build_id: str,
                                        filename: str) -> Optional[str]:
  """Queries ResultStore to find the exact Colossus URI of a log file."""
  # Write stubby field mask to a temp file
  request_extensions_content = ("[google.rpc.context.field_mask_context] {\n"
                                "  response_mask {\n"
                                '    paths: "files"\n'
                                "  }\n"
                                "}")
  with tempfile.NamedTemporaryFile("w", delete=False) as temp_file:
    temp_file.write(request_extensions_content)
    temp_file_path = temp_file.name

  cmd = [
      "stubby",
      "call",
      "--globaldb",
      f"--request_extensions_file={temp_file_path}",
      "--output_json",
      "blade:google.devtools.resultstore.v2.corpresultstoredownload-prod",
      "CorpResultStoreDownload.GetInvocation",
      f'name:"invocations/{build_id}"',
  ]
  try:
    result = run_command(cmd)
    raw_response = json.loads(result.stdout)
    files = raw_response.get("files", [])
    for file_info in files:
      uid = file_info.get("uid", "")
      uri = file_info.get("uri", "")
      if uid == filename or uid.endswith(filename):
        # Translate googlefile:/cns/... to /cns/...
        if uri.startswith("googlefile:"):
          return uri.replace("googlefile:", "", 1)
        return uri
  except (subprocess.SubprocessError, json.JSONDecodeError, OSError,
          AttributeError, TypeError) as e:
    logger.warning("Failed to query ResultStore for log URI for build %s: %s",
                   build_id, e)
  finally:
    try:
      os.remove(temp_file_path)
    except OSError:
      pass
  return None


def fetch_sponge_log(build_id: str, filename: str) -> Optional[str]:
  """Fetches a log file from CNS and saves it to the local cache."""
  if not _validate_invocation_id(build_id) or not _validate_filename(filename):
    logger.error("Invalid build ID or filename: %s / %s", build_id, filename)
    return None

  safe_filename = filename.replace("/", "_")
  cache_filename = f"sponge_log_{build_id}_{safe_filename}"
  local_path = get_cache_path(cache_filename)

  if os.path.exists(local_path) and os.path.getsize(local_path) > 0:
    logger.info("Found cached log: %s", local_path)
    return local_path

  logger.info("Fetching log %s for build %s from CNS", filename, build_id)

  # Try ResultStore lookup first as it gives the exact Colossus path
  actual_path = get_sponge_log_uri_from_resultstore(build_id, filename)

  # Fallback to the legacy pattern search if ResultStore lookup fails
  if not actual_path:
    logger.info("ResultStore lookup failed. Trying legacy pattern search...")
    base_pattern = (
        "/cns/vq-d/home/kokoro-dedicated/jenkins/spongeV2/prod/cobalt/*/"
        f"build/*/*/{build_id}/{filename}")
    ls_cmd = ["fileutil", "ls", "--", base_pattern]
    try:
      result = run_command(ls_cmd)
      paths = [
          line.strip()
          for line in result.stdout.splitlines()
          if line.strip().startswith("/cns/")
      ]
      if paths:
        actual_path = paths[0]
    except (subprocess.SubprocessError, OSError) as e:
      logger.warning("Legacy pattern search failed: %s", e)

  if not actual_path:
    logger.error("Could not find CNS path for build %s file %s", build_id,
                 filename)
    return None

  logger.info("Found CNS path: %s", actual_path)

  # Copy the file to local cache
  temp_local_path = local_path + ".tmp"
  try:
    ensure_cache_dir()
    cp_cmd = ["fileutil", "cp", "--", actual_path, temp_local_path]
    run_command(cp_cmd, timeout=60)
    os.replace(temp_local_path, local_path)
    return local_path
  except (subprocess.SubprocessError, OSError) as e:
    logger.error("Error fetching log from CNS for build %s: %s", build_id, e)
    for path in (local_path, temp_local_path):
      if os.path.exists(path):
        try:
          os.remove(path)
        except OSError:
          pass
    return None


def get_child_actions(build_id: str) -> List[Dict[str, Any]]:
  """Queries ResultStore via stubby for child actions of an invocation."""

  # Write stubby field mask to a temp file
  request_extensions_content = (
      "[google.rpc.context.field_mask_context] {\n"
      "  response_mask {\n"
      '    paths: "next_page_token"\n'
      '    paths: "actions.id"\n'
      '    paths: "actions.status_attributes.status"\n'
      '    paths: "actions.files"\n'
      "  }\n"
      "}")
  with tempfile.NamedTemporaryFile("w", delete=False) as temp_file:
    temp_file.write(request_extensions_content)
    temp_file_path = temp_file.name

  cmd = [
      "stubby",
      "call",
      "--globaldb",
      f"--request_extensions_file={temp_file_path}",
      "--output_json",
      "blade:google.devtools.resultstore.v2.corpresultstoredownload-prod",
      "CorpResultStoreDownload.ExportInvocation",
      f'name:"invocations/{build_id}"',
  ]

  actions = []
  try:
    result = run_command(cmd)
    # Stubby JSON output can be JSON Lines or a single JSON object
    try:
      messages = [json.loads(result.stdout)]
    except json.JSONDecodeError:
      messages = []
      for line in result.stdout.splitlines():
        if line.strip():
          try:
            messages.append(json.loads(line))
          except json.JSONDecodeError:
            continue

    for msg in messages:
      for action in msg.get("actions", []):
        id_msg = action.get("id", {})
        target_id = id_msg.get("target_id") or id_msg.get(
            "targetId") or action.get("targetId") or action.get(
                "target_id") or "unknown"

        status_attr = action.get("status_attributes") or action.get(
            "statusAttributes") or {}
        status = status_attr.get("status", "UNKNOWN")

        files = []
        for file_info in action.get("files", []):
          files.append({
              "uid": file_info.get("uid"),
              "uri": file_info.get("uri")
          })
        actions.append({
            "target_id": target_id,
            "status": status,
            "files": files
        })
  except (subprocess.SubprocessError, OSError, AttributeError, TypeError) as e:
    logger.error("Failed to query ResultStore for invocation %s: %s", build_id,
                 e)
  finally:
    try:
      os.remove(temp_file_path)
    except OSError:
      pass

  return actions


def _extract_log_uris(
    files: List[Dict[str, str]]) -> tuple[Optional[str], Optional[str]]:
  """Extracts webDriverTestLog.ERROR and test_output.txt/test.log URIs."""
  test_log_uri = None
  infra_log_uri = None
  for file_info in files:
    uid = file_info.get("uid", "")
    uri = file_info.get("uri", "")
    if uid.endswith("webDriverTestLog.ERROR"):
      test_log_uri = uri
    elif uid.endswith("test_output.txt") or uid == "test.log":
      infra_log_uri = uri
  return test_log_uri, infra_log_uri


def process_child_jobs(parent_log_path: str,
                       branch: str) -> List[Dict[str, Any]]:
  """Scans the parent log and downloads failed child job logs.

  Scans for child sponge links in the parent log, queries ResultStore
  for child actions, and downloads their corresponding logs.
  """
  child_jobs = []
  if not os.path.exists(parent_log_path):
    return child_jobs

  # Scan parent log for child sponge IDs
  sponge_ids = []
  try:
    with open(parent_log_path, "r", encoding="utf-8", errors="ignore") as f:
      for line in f:
        match = SPONGE_LINK_PATTERN.search(line)
        if match:
          sponge_id = match.group(1)
          if sponge_id not in sponge_ids:
            sponge_ids.append(sponge_id)
  except OSError as e:
    logger.error("Error parsing parent log %s: %s", parent_log_path, e)
    return child_jobs

  if not sponge_ids:
    return child_jobs

  logger.info("Found child Sponge IDs in parent log: %s", sponge_ids)

  for child_id in sponge_ids:
    actions = get_child_actions(child_id)
    for action in actions:
      if action.get("status") in (3, 6, 9, "FAILED", "FAILED_TO_BUILD",
                                  "TOOL_FAILED"):
        target_id = action.get("target_id", "unknown")
        test_log_uri, infra_log_uri = _extract_log_uris(action.get("files", []))

        # Try to download test log first, fallback to infra log
        local_log_path = ""
        log_uri = test_log_uri or infra_log_uri
        if log_uri:
          # Extract file name from URI or use default
          filename = os.path.basename(log_uri.replace("googlefile:", ""))
          if not filename or filename == "/":
            filename = "test.log"

          local_log_path = fetch_sponge_log(child_id, filename) or ""

        child_jobs.append({
            "job_name": f"{target_id}",
            "conclusion": "failure",
            "url": f"https://sponge.corp.google.com/invocation?id={child_id}",
            "local_log_path": local_log_path,
            "branch": branch,
        })
  return child_jobs


def triage_job(job: Dict[str, str]) -> Optional[Dict[str, Any]]:
  """Checks status and downloads logs for a single job."""
  job_name = job["job_name"]
  branch = job["branch"]

  logger.info("Triaging job %s", job_name)
  status_info = get_latest_build_status(job_name)

  if not status_info:
    return None

  status = status_info.get("status")
  # Map 'FAILURE' or 'FAILED' to failure
  if status in ("FAILURE", "FAILED"):
    build_id = status_info.get("build_id")
    if not build_id:
      logger.warning("Failed status but missing build_id for %s", job_name)
      return None

    # Fetch main build log
    local_log_path = fetch_sponge_log(build_id, "build.log") or ""

    # Process child jobs if any are mentioned in the parent build.log
    child_jobs = []
    if local_log_path:
      child_jobs = process_child_jobs(local_log_path, branch)

    return {
        "job_name": job_name,
        "branch": branch,
        "run_id": build_id,
        "createdAt": status_info.get("createdAt", ""),  # optional
        "url": f"https://sponge.corp.google.com/invocation?id={build_id}",
        "local_log_path": local_log_path,
        "child_jobs": child_jobs,
    }
  return None


def main():
  parser = argparse.ArgumentParser(
      description="Discover and download Kokoro build logs.")
  parser.add_argument(
      "--output",
      default="kokoro_results.json",
      help="Path to output results file.")
  parser.add_argument("--job", help="Triage a specific job name directly.")
  parser.add_argument(
      "--build", help="Triage a specific build/invocation ID directly.")
  parser.add_argument(
      "--verbose", action="store_true", help="Enable verbose logging.")
  args = parser.parse_args()

  log_level = logging.DEBUG if args.verbose else logging.INFO
  logging.basicConfig(
      level=log_level,
      format="%(asctime)s - %(levelname)s - %(message)s",
      stream=sys.stderr)

  if args.build:
    # Direct build triage (skip discovery)
    logger.info("Directly triaging build ID: %s", args.build)
    local_log_path = fetch_sponge_log(args.build, "build.log") or ""
    child_jobs = []
    if local_log_path:
      child_jobs = process_child_jobs(local_log_path, "unknown")

    results = {
        "total_jobs_fetched":
            1,
        "failed_runs": [{
            "job_name": "direct_build_query",
            "branch": "unknown",
            "run_id": args.build,
            "url": f"https://sponge.corp.google.com/invocation?id={args.build}",
            "local_log_path": local_log_path,
            "child_jobs": child_jobs,
        }]
    }
  elif args.job:
    # Direct job triage
    logger.info("Directly triaging job: %s", args.job)
    job = {"job_name": args.job, "branch": "unknown"}
    run_info = triage_job(job)
    results = {
        "total_jobs_fetched": 1,
        "failed_runs": [run_info] if run_info else []
    }
  else:
    # Discovery mode
    logger.info("Starting discovery mode...")
    jobs = discover_jobs()
    logger.info("Discovered %d jobs", len(jobs))

    failed_runs = []
    # Query statuses and download logs in parallel
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
      futures = [executor.submit(triage_job, job) for job in jobs]
      for future in concurrent.futures.as_completed(futures):
        try:
          run_info = future.result()
          if run_info:
            failed_runs.append(run_info)
        except Exception as e:  # pylint: disable=broad-exception-caught
          logger.error("Job triage thread threw exception: %s", e)

    results = {
        "total_jobs_fetched": len(jobs),
        "failed_runs": failed_runs,
    }

  # Map to unified schema
  failed_runs = results.get("failed_runs", [])
  total_fetched = results.get("total_jobs_fetched", 0)

  unified_runs = []
  for run in failed_runs:
    if not run:
      continue
    failed_jobs = []
    if run.get("local_log_path"):
      name = run["job_name"]
      if run.get("child_jobs"):
        name += " (parent)"
      failed_jobs.append({
          "name": name,
          "url": run["url"],
          "local_log_path": run["local_log_path"],
          "log_type": "kokoro_log"
      })
    for child in run.get("child_jobs", []):
      failed_jobs.append({
          "name": child["job_name"],
          "url": child["url"],
          "local_log_path": child["local_log_path"],
          "log_type": "test_log"
      })
    unified_runs.append({
        "run_id": str(run["run_id"]),
        "job_name": run["job_name"],
        "branch": run["branch"],
        "event": "nightly",
        "createdAt": run.get("createdAt", ""),
        "url": run["url"],
        "failed_jobs": failed_jobs
    })

  unified_results = {
      "source": "kokoro",
      "total_jobs_fetched": total_fetched,
      "runs": unified_runs
  }

  # Ensure output directory exists
  os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

  with open(args.output, "w", encoding="utf-8") as f:
    json.dump(unified_results, f, indent=2)
  print(f"Successfully wrote results to {args.output}")


if __name__ == "__main__":
  main()
