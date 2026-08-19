#!/usr/bin/env python3
# pylint: disable=duplicate-code
"""Gemini & Vertex AI Connection & Model Diagnostics Tester.

Supports:
  1. Vertex AI (GCP Project + Service Account / gcloud ADC access token)
  2. Google AI Studio (API Key or Bearer Token with target-specific scopes)

Tests connection, latency, and reasoning configurations.
"""

import argparse
import json
import os
import ssl
import sys
import time
import urllib.error
import urllib.request
from typing import Dict, List, Optional

import google.auth
import google.auth.transport.requests


def get_gcp_access_token(scopes: List[str]) -> str:
  """Retrieves GCP OAuth access token natively using google-auth ADC."""
  credentials, _ = google.auth.default(scopes=scopes)
  request = google.auth.transport.requests.Request()
  credentials.refresh(request)
  if not credentials.token:
    raise RuntimeError(
        "Failed to obtain OAuth access token from Google Cloud ADC.")
  return credentials.token


def test_vertex_ai_query(
    project_id: str,
    location: str,
    model: str,
    access_token: str,
    prompt: str = "Return OK",
) -> Dict[str, any]:
  """Tests an API call against Google Cloud Vertex AI endpoint."""
  # Vertex AI beta endpoint is strictly v1beta1 (with a '1')
  api_version = "v1beta1" if "gemini-3." in model else "v1"

  # Resolve Host Location DNS mappings for multi-regions
  if location == "us":
    host_location = "us-central1"
  elif location == "eu":
    host_location = "europe-west3"
  else:
    host_location = location

  url = (f"https://{host_location}-aiplatform.googleapis.com/"
         f"{api_version}/projects/{project_id}/locations/{location}/"
         f"publishers/google/models/{model}:generateContent")

  generation_config = {
      "maxOutputTokens": 2048,
  }

  # Inject thinking configuration and strip temperature for Gemini 3.x models
  if "gemini-3." in model:
    generation_config["thinkingConfig"] = {"thinkingLevel": "medium"}
  else:
    generation_config["temperature"] = 0.1

  payload = {
      "contents": [{
          "role": "user",
          "parts": [{
              "text": prompt
          }],
      }],
      "generationConfig": generation_config,
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
          "thinking_tokens": usage.get("thoughtsTokenCount", 0),
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
    model: str,
    api_key: Optional[str] = None,
    access_token: Optional[str] = None,
    prompt: str = "Return OK",
) -> Dict[str, any]:
  """Tests an API call against Google AI Studio endpoint."""
  if api_key:
    url = (f"https://generativelanguage.googleapis.com/v1beta/models/"
           f"{model}:generateContent?key={api_key}")
    headers = {"Content-Type": "application/json"}
  elif access_token:
    url = (f"https://generativelanguage.googleapis.com/v1beta/models/"
           f"{model}:generateContent")
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {access_token}",
    }
  else:
    return {
        "success": False,
        "error": "No credentials (API key or OAuth token) provided.",
    }

  generation_config = {
      "maxOutputTokens": 2048,
  }

  # Inject thinking configuration and strip temperature for Gemini 3.x models
  if "gemini-3." in model:
    generation_config["thinkingConfig"] = {"thinkingLevel": "medium"}
  else:
    generation_config["temperature"] = 0.1

  payload = {
      "contents": [{
          "parts": [{
              "text": prompt
          }]
      }],
      "generationConfig": generation_config,
  }
  data = json.dumps(payload).encode("utf-8")
  req = urllib.request.Request(
      url,
      data=data,
      headers=headers,
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
          "thinking_tokens": usage.get("thoughtsTokenCount", 0),
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
      "--target",
      choices=["studio", "vertex"],
      default="studio",
      help="API target: 'studio' or 'vertex'. Default: 'studio'.",
  )
  parser.add_argument(
      "--project-id",
      default=os.environ.get("GCP_PROJECT") or
      os.environ.get("GOOGLE_CLOUD_PROJECT"),
      help="GCP Project ID for authentication/Vertex AI.",
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
      "--model",
      nargs="+",
      dest="models",
      default=["gemini-3.7-flash"],
      help="Models to test.",
  )
  args = parser.parse_args()

  print("=" * 70)
  print("[TEST] GEMINI CONNECTION DIAGNOSTIC")
  if args.target == "vertex":
    print("[ENTERPRISE] Mode:      Vertex AI (Enterprise Project Quota)")
    print(f"[ENTERPRISE] Project:   {args.project_id}")
    print(f"[LOCATION]   Location:  {args.location}")
  else:
    print("[DEVELOPER]  Mode:      Google AI Studio (generativelanguage)")
    if args.api_key:
      print("[KEY]        Auth Type: API Key")
    else:
      print("[AUTH]       Auth Type: GCP OAuth Access Token")
  print("=" * 70)

  token = None
  if args.target == "vertex" or (args.target == "studio" and not args.api_key):
    print("\n[AUTH] 1. Acquiring Google Cloud OAuth Access Token...")
    scopes = (["https://www.googleapis.com/auth/cloud-platform"]
              if args.target == "vertex" else
              ["https://www.googleapis.com/auth/generative-language"])
    try:
      token = get_gcp_access_token(scopes=scopes)
      masked = token[:6] + "..." + token[-4:]
      print(f"[OK] Acquired Access Token: {masked}")
    except (RuntimeError, OSError, Exception) as e:  # pylint: disable=broad-exception-caught
      print(f"[FAIL] Failed to obtain GCP credentials: {e}", file=sys.stderr)
      sys.exit(1)

  print("\n[FAST] 2. Benchmarking Model Endpoints & Latency...")
  for model_name in args.models:
    print(f"\n  Testing endpoint: {model_name}...")
    if args.target == "vertex":
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
          access_token=token,
      )

    if res.get("success"):
      res_text = repr(res.get("text"))
      res_lat = res.get("latency_sec", 0.0)
      p_tok = res.get("prompt_tokens", 0)
      c_tok = res.get("candidates_tokens", 0)
      th_tok = res.get("thinking_tokens", 0)
      print(f"    [OK] Response: {res_text}")
      print(f"    Latency:       {res_lat:.2f}s")
      print(f"    Prompt Tokens: {p_tok}")
      print(f"    Output Tokens: {c_tok}")
      if th_tok > 0:
        print(f"    Thinking Tokens: {th_tok}")
    else:
      err_msg = res.get("error")
      print(f"    [FAIL] Error: {err_msg}", file=sys.stderr)

  print("\n[DONE] DIAGNOSTIC COMPLETE")


if __name__ == "__main__":
  main()
