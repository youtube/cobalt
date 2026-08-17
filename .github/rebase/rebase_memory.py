#!/usr/bin/env python3
# pylint: disable=duplicate-code
"""Persistent Knowledge Memory Bank for Cobalt Rebase.

Stores and retrieves successful conflict resolutions, GN fixes, and
compiler repairs with full concrete code solutions under out/memory/.
Supports bi-directional synchronization with Google Cloud Storage (GCS)
to persist long-term learning across ephemeral rebase sessions.
"""

import json
import os
import subprocess
import sys
from typing import Optional

REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DEFAULT_MEMORY_FILE = os.path.join(
    REPO_ROOT,
    "out",
    "memory",
    "knowledge_bank.json",
)


def pull_memory_from_gcs(
    memory_path: Optional[str] = None,
    gcs_uri: Optional[str] = None,
) -> bool:
  """Pulls existing knowledge memory bank from GCS bucket if configured."""
  target_path = memory_path or DEFAULT_MEMORY_FILE
  remote_uri = gcs_uri or os.environ.get("GCS_MEMORY_URI")
  if not remote_uri:
    return False

  os.makedirs(os.path.dirname(target_path), exist_ok=True)
  print(
      f"[rebase_memory] Pulling knowledge memory from {remote_uri}...",
      file=sys.stderr,
  )
  cmd = ["gcloud", "storage", "cp", remote_uri, target_path]
  try:
    res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if res.returncode == 0 and os.path.isfile(target_path):
      print(
          "[rebase_memory] [OK] Pulled memory bank from GCS.",
          file=sys.stderr,
      )
      return True
  except (OSError, subprocess.SubprocessError) as e:
    print(
        f"[rebase_memory] [WARNING] Could not pull from GCS: {e}",
        file=sys.stderr,
    )
  return False


def sync_memory_to_gcs(
    memory_path: Optional[str] = None,
    gcs_uri: Optional[str] = None,
) -> bool:
  """Uploads local knowledge memory bank to GCS bucket on rebase complete."""
  target_path = memory_path or DEFAULT_MEMORY_FILE
  remote_uri = gcs_uri or os.environ.get("GCS_MEMORY_URI")
  if not remote_uri or not os.path.isfile(target_path):
    return False

  print(
      f"[rebase_memory] Syncing knowledge memory to {remote_uri}...",
      file=sys.stderr,
  )
  cmd = ["gcloud", "storage", "cp", target_path, remote_uri]
  try:
    res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if res.returncode == 0:
      print(
          f"[rebase_memory] [OK] Uploaded memory to {remote_uri}.",
          file=sys.stderr,
      )
      return True
    print(
        f"[rebase_memory] [FAIL] gcloud storage cp failed: {res.stderr}",
        file=sys.stderr,
    )
    return False
  except (OSError, subprocess.SubprocessError) as e:
    print(
        f"[rebase_memory] [WARNING] Could not upload to GCS: {e}",
        file=sys.stderr,
    )
  return False


def load_past_experience(memory_path: Optional[str] = None,
                         max_entries: int = 10) -> str:
  """Loads previous successful fixes with code solutions for Gemini."""
  path = memory_path or DEFAULT_MEMORY_FILE
  if not os.path.isfile(path):
    return "No prior rebase lessons recorded yet."

  try:
    with open(path, "r", encoding="utf-8") as f:
      data = json.load(f)
    if not data:
      return "No prior rebase lessons recorded yet."

    entries = []
    for item in data[-max_entries:]:
      category = item.get("category", "Fix")
      error_sig = item.get("error_signature", "").strip()
      target_file = item.get("target_file", "").strip()
      solution = item.get("solution_snippet", "").strip()
      summary = item.get("fix_summary", "").strip()

      entry_text = (f"- [{category}] File: `{target_file}`\n"
                    f"  Problem: {error_sig}\n")
      if solution:
        entry_text += f"  Working Solution:\n```\n{solution}\n```"
      elif summary:
        entry_text += f"  Resolution Pattern: {summary}"

      entries.append(entry_text)
    return "\n\n".join(entries)
  except (OSError, json.JSONDecodeError):
    return "No prior rebase lessons recorded yet."


def record_successful_fix(
    category: str,
    target_file: str,
    error_signature: str,
    *,
    fix_summary: str = "",
    solution_snippet: str = "",
    memory_path: Optional[str] = None,
):
  """Persists a successful fix with solution code to knowledge bank."""
  path = memory_path or DEFAULT_MEMORY_FILE
  os.makedirs(os.path.dirname(path), exist_ok=True)

  data = []
  if os.path.isfile(path):
    try:
      with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    except (OSError, json.JSONDecodeError):
      data = []

  clean_solution = solution_snippet.strip()[:1000]

  for item in data:
    if (item.get("target_file") == target_file and
        item.get("error_signature") == error_signature):
      if clean_solution and not item.get("solution_snippet"):
        item["solution_snippet"] = clean_solution
        if fix_summary:
          item["fix_summary"] = fix_summary
        try:
          with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
        except OSError:
          pass
      return

  data.append({
      "category": category,
      "target_file": target_file,
      "error_signature": error_signature[:300],
      "fix_summary": fix_summary[:500],
      "solution_snippet": clean_solution,
  })

  try:
    with open(path, "w", encoding="utf-8") as f:
      json.dump(data, f, indent=2)
  except OSError as e:
    print(
        f"[rebase_memory] [WARNING] Could not save to memory bank: {e}",
        file=sys.stderr,
    )
