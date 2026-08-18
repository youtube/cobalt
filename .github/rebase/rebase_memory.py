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
import sys
from typing import Optional, Tuple

from google.cloud import storage

REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DEFAULT_MEMORY_FILE = os.path.join(
    REPO_ROOT,
    "out",
    "memory",
    "knowledge_bank.json",
)


def _parse_gcs_uri(gcs_uri: str) -> Tuple[str, str]:
  """Parses a gs://bucket/blob_path URI into (bucket_name, blob_name)."""
  if not gcs_uri.startswith("gs://"):
    raise ValueError(f"Invalid GCS URI: {gcs_uri}. Must start with 'gs://'.")
  path_part = gcs_uri[5:]
  parts = path_part.split("/", 1)
  if len(parts) < 2 or not parts[0] or not parts[1]:
    raise ValueError(f"Invalid GCS URI: {gcs_uri}. Must be of format "
                     "gs://<bucket_name>/<path_to_blob>.")
  return parts[0], parts[1]


def pull_memory_from_gcs(
    memory_path: Optional[str] = None,
    gcs_uri: Optional[str] = None,
) -> bool:
  """Pulls existing knowledge memory bank from GCS bucket natively."""
  target_path = memory_path or DEFAULT_MEMORY_FILE
  remote_uri = gcs_uri or os.environ.get("GCS_MEMORY_URI")
  if not remote_uri:
    return False

  bucket_name, blob_name = _parse_gcs_uri(remote_uri)
  os.makedirs(os.path.dirname(target_path), exist_ok=True)
  print(
      f"[rebase_memory] Pulling knowledge memory from {remote_uri}...",
      file=sys.stderr,
  )
  client = storage.Client()
  bucket = client.bucket(bucket_name)
  blob = bucket.blob(blob_name)
  if not blob.exists():
    print(
        f"[rebase_memory] Remote blob does not exist at {remote_uri}. "
        "Starting with fresh memory bank.",
        file=sys.stderr,
    )
    return False

  blob.download_to_filename(target_path)
  print(
      f"[rebase_memory] [OK] Pulled memory bank from GCS to {target_path}.",
      file=sys.stderr,
  )
  return True


def sync_memory_to_gcs(
    memory_path: Optional[str] = None,
    gcs_uri: Optional[str] = None,
) -> bool:
  """Uploads local knowledge memory bank to GCS bucket natively on complete."""
  target_path = memory_path or DEFAULT_MEMORY_FILE
  remote_uri = gcs_uri or os.environ.get("GCS_MEMORY_URI")
  if not remote_uri:
    return False

  if not os.path.isfile(target_path):
    print(
        f"[rebase_memory] [WARNING] Local memory file not found: "
        f"{target_path}. Skipping GCS sync.",
        file=sys.stderr,
    )
    return False

  print(
      f"[rebase_memory] Syncing knowledge memory to {remote_uri}...",
      file=sys.stderr,
  )
  try:
    bucket_name, blob_name = _parse_gcs_uri(remote_uri)
    client = storage.Client()
    bucket = client.bucket(bucket_name)
    blob = bucket.blob(blob_name)
    blob.upload_from_filename(target_path)
    print(
        f"[rebase_memory] [OK] Uploaded memory to {remote_uri}.",
        file=sys.stderr,
    )
    return True
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(
        f"[rebase_memory] [WARNING] Could not upload memory to GCS: {e}",
        file=sys.stderr,
    )
  return False


def load_past_experience(memory_path: Optional[str] = None,
                         max_entries: int = 10) -> str:
  """Loads previous successful fixes with code solutions for Gemini."""
  path = memory_path or DEFAULT_MEMORY_FILE
  if not os.path.isfile(path) or os.path.getsize(path) == 0:
    return "No prior rebase lessons recorded yet."

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
  if os.path.isfile(path) and os.path.getsize(path) > 0:
    with open(path, "r", encoding="utf-8") as f:
      data = json.load(f)

  clean_solution = solution_snippet.strip()[:1000]

  updated = False
  for item in data:
    if (item.get("target_file") == target_file and
        item.get("error_signature") == error_signature):
      if clean_solution and not item.get("solution_snippet"):
        item["solution_snippet"] = clean_solution
        if fix_summary:
          item["fix_summary"] = fix_summary
      updated = True
      break

  if not updated:
    data.append({
        "category": category,
        "target_file": target_file,
        "error_signature": error_signature[:300],
        "fix_summary": fix_summary[:500],
        "solution_snippet": clean_solution,
    })

  with open(path, "w", encoding="utf-8") as f:
    json.dump(data, f, indent=2)
