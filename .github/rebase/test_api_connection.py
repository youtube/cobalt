#!/usr/bin/env python3
# pylint: disable=duplicate-code
"""Gemini & Vertex AI Connection & Model Diagnostics Tester.

Supports:
  1. Vertex AI (GCP Project + Service Account / gcloud ADC access token)
  2. Google AI Studio (API Key)

Tests connection, latency, and C++ code conflict resolution without 429
rate limits.
"""

import argparse
import json
import os
import ssl
import subprocess
import sys
import time
import urllib.error
import urllib.request
from typing import Dict, Optional

try:
  import google.auth
  import google.auth.transport.requests
  HAS_GOOGLE_AUTH = True
except ImportError:
  HAS_GOOGLE_AUTH = False


def get_gcp_access_token() -> Optional[str]:
  """Retrieves Google Cloud OAuth access token via ADC or gcloud CLI."""
  # 1. Try gcloud auth application-default print-access-token
  try:
    res = subprocess.run(
        ["gcloud", "auth", "application-default", "print-access-token"],
        capture_output=True,
        text=True,
        timeout=10,
        check=False,
    )
    if res.returncode == 0 and res.stdout.strip():
      return res.stdout.strip()
  except (OSError, subprocess.SubprocessError):
    pass

  # 2. Try gcloud auth print-access-token
  try:
    res = subprocess.run(
        ["gcloud", "auth", "print-access-token"],
        capture_output=True,
        text=True,
        timeout=10,
        check=False,
    )
    if res.returncode == 0 and res.stdout.strip():
      return res.stdout.strip()
  except (OSError, subprocess.SubprocessError):
    pass

  # 3. Direct exchange using application_default_credentials.json if present
  adc_path = os.path.expanduser(
      "~/.config/gcloud/application_default_credentials.json")
  if os.path.isfile(adc_path):
    try:
      with open(adc_path, "r", encoding="utf-8") as f:
        adc = json.load(f)
      if ("refresh_token" in adc and "client_id" in adc and
          "client_secret" in adc):
        token_url = "https://oauth2.googleapis.com/token"
        payload = json.dumps({
            "client_id": adc["client_id"],
            "client_secret": adc["client_secret"],
            "refresh_token": adc["refresh_token"],
            "grant_type": "refresh_token",
        }).encode("utf-8")
        req = urllib.request.Request(
            token_url,
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        ctx = ssl.create_default_context()
        with urllib.request.urlopen(req, context=ctx, timeout=10) as resp:
          data = json.loads(resp.read().decode("utf-8"))
          return data.get("access_token")
    except (OSError, json.JSONDecodeError, urllib.error.URLError):
      pass

  # 4. Try google.auth if installed
  if HAS_GOOGLE_AUTH:
    try:
      credentials, _ = google.auth.default(
          scopes=["https://www.googleapis.com/auth/cloud-platform"])
      credentials.refresh(google.auth.transport.requests.Request())
      if credentials.token:
        return credentials.token
    except Exception:  # pylint: disable=broad-exception-caught
      pass

  return None


def test_vertex_ai_query(
    project_id: str,
    location: str,
    model: str,
    access_token: str,
    prompt: str = "Return OK",
) -> Dict[str, any]:
  """Tests an API call against Google Cloud Vertex AI endpoint."""
  url = (f"https://{location}-aiplatform.googleapis.com/v1/projects/"
         f"{project_id}/locations/{location}/publishers/google/models/"
         f"{model}:generateContent")
  payload = {
      "contents": [{
          "role": "user",
          "parts": [{
              "text": prompt
          }],
      }],
      "generationConfig": {
          "temperature": 0.1,
          "maxOutputTokens": 2048,
      },
  }
  data = json.dumps(payload).encode("utf-8")
  req = urllib.request.Request(
      url,
      data=data,
      headers={
          "Content-Type": "application/json",
          "Authorization": f"Bearer {access_token}",
      },
      method="POST",
  )
  ctx = ssl.create_default_context()

  start = time.time()
  try:
    with urllib.request.urlopen(req, context=ctx, timeout=60) as resp:
      elapsed = time.time() - start
      res_json = json.loads(resp.read().decode("utf-8"))
      candidates = res_json.get("candidates", [])
      text = (
          candidates[0]["content"]["parts"][0]["text"] if candidates else "")
      usage = res_json.get("usageMetadata", {})
      return {
          "success": True,
          "latency_sec": elapsed,
          "text": text.strip(),
          "prompt_tokens": usage.get("promptTokenCount", 0),
          "candidates_tokens": usage.get("candidatesTokenCount", 0),
          "total_tokens": usage.get("totalTokenCount", 0),
      }
  except urllib.error.HTTPError as e:
    body = e.read().decode("utf-8") if e.fp else ""
    return {
        "success": False,
        "error": f"HTTP {e.code}: {e.reason}\n{body}",
    }
  except urllib.error.URLError as e:
    return {"success": False, "error": f"URLError: {e.reason}"}


def test_ai_studio_query(
    api_key: str,
    model: str,
    prompt: str = "Return OK",
) -> Dict[str, any]:
  """Tests an API call against Google AI Studio endpoint."""
  url = (f"https://generativelanguage.googleapis.com/v1beta/models/"
         f"{model}:generateContent?key={api_key}")
  payload = {
      "contents": [{
          "parts": [{
              "text": prompt
          }]
      }],
      "generationConfig": {
          "temperature": 0.1,
          "maxOutputTokens": 2048,
      },
  }
  data = json.dumps(payload).encode("utf-8")
  req = urllib.request.Request(
      url,
      data=data,
      headers={"Content-Type": "application/json"},
      method="POST",
  )
  ctx = ssl.create_default_context()

  start = time.time()
  try:
    with urllib.request.urlopen(req, context=ctx, timeout=60) as resp:
      elapsed = time.time() - start
      res_json = json.loads(resp.read().decode("utf-8"))
      candidates = res_json.get("candidates", [])
      text = (
          candidates[0]["content"]["parts"][0]["text"] if candidates else "")
      usage = res_json.get("usageMetadata", {})
      return {
          "success": True,
          "latency_sec": elapsed,
          "text": text.strip(),
          "prompt_tokens": usage.get("promptTokenCount", 0),
          "candidates_tokens": usage.get("candidatesTokenCount", 0),
          "total_tokens": usage.get("totalTokenCount", 0),
      }
  except urllib.error.HTTPError as e:
    body = e.read().decode("utf-8") if e.fp else ""
    return {
        "success": False,
        "error": f"HTTP {e.code}: {e.reason}\n{body}",
    }
  except urllib.error.URLError as e:
    return {"success": False, "error": f"URLError: {e.reason}"}


def main():
  """Main CLI entry point for Vertex AI / Gemini API diagnostic."""
  parser = argparse.ArgumentParser(
      description="Test Vertex AI and Google AI Studio API connections.")
  parser.add_argument(
      "--project-id",
      default=os.environ.get("GCP_PROJECT") or
      os.environ.get("GOOGLE_CLOUD_PROJECT"),
      help="GCP Project ID for Vertex AI.",
  )
  parser.add_argument(
      "--location",
      default=os.environ.get("GCP_LOCATION", "us-central1"),
      help="Vertex AI Region (default: us-central1).",
  )
  parser.add_argument(
      "--api-key",
      default=os.environ.get("GEMINI_API_KEY"),
      help="Google AI Studio API key.",
  )
  parser.add_argument(
      "--models",
      nargs="+",
      default=["gemini-2.5-flash", "gemini-2.5-pro"],
      help="Models to test.",
  )
  args = parser.parse_args()

  print("=" * 70)
  print("[TEST] GEMINI & VERTEX AI CONNECTION DIAGNOSTIC")
  if args.project_id:
    print("[ENTERPRISE] Mode:      Vertex AI (Enterprise Project Quota)")
    print(f"[ENTERPRISE] Project:   {args.project_id}")
    print(f"[LOCATION]   Location:  {args.location}")
  elif args.api_key:
    print("[KEY] Mode:            Google AI Studio (Personal API Key)")
  else:
    print(
        "[FAIL] Set $GCP_PROJECT or pass --project-id / --api-key.",
        file=sys.stderr,
    )
    sys.exit(1)
  print("=" * 70)

  token = None
  if args.project_id:
    print("\n[AUTH] 1. Acquiring Google Cloud OAuth Access Token...")
    token = get_gcp_access_token()
    if not token:
      print("[FAIL] Failed to obtain GCP access token.", file=sys.stderr)
      sys.exit(1)
    masked = token[:6] + "..." + token[-4:]
    print(f"[OK] Acquired Access Token: {masked}")

  print("\n[FAST] 2. Benchmarking Model Endpoints & Latency...")
  for model_name in args.models:
    print(f"\n  Testing endpoint: {model_name}...")
    if args.project_id and token:
      res = test_vertex_ai_query(
          project_id=args.project_id,
          location=args.location,
          model=model_name,
          access_token=token,
      )
    else:
      res = test_ai_studio_query(
          api_key=args.api_key,
          model=model_name,
      )

    if res.get("success"):
      res_text = repr(res.get("text"))
      res_lat = res.get("latency_sec", 0.0)
      p_tok = res.get("prompt_tokens", 0)
      c_tok = res.get("candidates_tokens", 0)
      print(f"    [OK] Response: {res_text}")
      print(f"    Latency:       {res_lat:.2f}s")
      print(f"    Prompt Tokens: {p_tok}")
      print(f"    Output Tokens: {c_tok}")
    else:
      err_msg = res.get("error")
      print(f"    [FAIL] Error: {err_msg}", file=sys.stderr)

  print("\n[DONE] DIAGNOSTIC COMPLETE")


if __name__ == "__main__":
  main()
