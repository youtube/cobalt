#!/usr/bin/env python3
# pylint: disable=duplicate-code
"""Gemini & Vertex AI Connection & Model Diagnostics Tester.

Natively uses Google's unified `google.genai` SDK (`genai.Client`)
supporting:
  1. Vertex AI (GCP Project + OAuth ADC with regional or multi-region routing)
  2. Google AI Studio (API Key)

Tests connection, latency, and model availability.
"""

import argparse
import os
import sys
import time
from typing import Dict
import warnings

from google import genai
from google.genai import types

# Suppress google.genai informational warnings about AFC on direct model calls
warnings.filterwarnings("ignore", message=".*automatic function calling.*")
warnings.filterwarnings("ignore", category=UserWarning, module="google.genai")


def test_genai_query(
    client: genai.Client,
    model: str,
    prompt: str = "Return OK",
) -> Dict[str, any]:
  """Executes a generate_content query using the official genai.Client."""
  start = time.time()
  try:
    config = types.GenerateContentConfig(max_output_tokens=2048,)
    # Configure temperature or thinking budget
    if "gemini-3." in model or "thinking" in model.lower():
      config.thinking_config = types.ThinkingConfig(thinking_budget=1024)
    else:
      config.temperature = 0.1

    resp = client.models.generate_content(
        model=model,
        contents=prompt,
        config=config,
    )
    elapsed = time.time() - start
    text = resp.text.strip() if resp and resp.text else ""
    usage = getattr(resp, "usage_metadata", None)
    prompt_tokens = (getattr(usage, "prompt_token_count", 0) or 0)
    cand_tokens = (getattr(usage, "candidates_token_count", 0) or 0)
    total_tokens = (getattr(usage, "total_token_count", 0) or 0)
    thoughts_tokens = (getattr(usage, "thoughts_token_count", 0) or 0)

    return {
        "success": True,
        "latency_sec": elapsed,
        "text": text,
        "prompt_tokens": prompt_tokens,
        "candidates_tokens": cand_tokens,
        "total_tokens": total_tokens,
        "thinking_tokens": thoughts_tokens,
    }
  except Exception as e:  # pylint: disable=broad-exception-caught
    return {"success": False, "error": str(e)}


def main():
  """Main CLI entry point for genai.Client connection diagnostic."""
  parser = argparse.ArgumentParser(
      description="Test Vertex AI and Google AI Studio via google.genai Client."
  )
  parser.add_argument(
      "--target",
      choices=["vertex", "studio"],
      default="vertex",
      help="API mode: 'vertex' (Enterprise/ADC) or 'studio' (API Key).",
  )
  parser.add_argument(
      "--project-id",
      default=os.environ.get("GCP_PROJECT") or
      os.environ.get("GOOGLE_CLOUD_PROJECT") or "lxn-test",
      help="GCP Project ID for Vertex AI.",
  )
  parser.add_argument(
      "--location",
      default=os.environ.get("GCP_LOCATION", "us"),
      help="Location: 'us' / 'global' (for Gemini 3.x) or 'us-central1'.",
  )
  parser.add_argument(
      "--api-key",
      default=os.environ.get("GEMINI_API_KEY"),
      help="Google AI Studio API key (optional for studio mode).",
  )
  parser.add_argument(
      "--models",
      "--model",
      nargs="+",
      dest="models",
      default=["gemini-3.7-flash", "gemini-2.5-pro", "gemini-2.5-flash"],
      help="Models to benchmark.",
  )
  args = parser.parse_args()

  print("=" * 70)
  print("[TEST] GEMINI CLIENT CONNECTION DIAGNOSTIC")
  if args.target == "vertex":
    print("[ENTERPRISE] Mode:      Vertex AI (google.genai with vertexai=True)")
    print(f"[ENTERPRISE] Project:   {args.project_id}")
    print(f"[LOCATION]   Location:  {args.location}")
    # Disable mTLS endpoint auto-selection for multi-region endpoints
    os.environ["GOOGLE_API_USE_CLIENT_CERTIFICATE"] = "false"
    os.environ["GOOGLE_API_USE_MTLS_ENDPOINT"] = "never"
    client = genai.Client(
        vertexai=True,
        project=args.project_id,
        location=args.location,
    )
  else:
    print("[DEVELOPER]  Mode:      Google AI Studio (API Key)")
    if args.api_key:
      client = genai.Client(api_key=args.api_key)
    else:
      print("[FAIL] Missing GEMINI_API_KEY for studio mode.", file=sys.stderr)
      sys.exit(1)
  print("=" * 70)

  print("\n[FAST] Benchmarking Models via genai.Client...")
  for model_name in args.models:
    print(f"\n  Testing model: {model_name}...")
    res = test_genai_query(client, model=model_name)
    if res.get("success"):
      res_text = repr(res.get("text"))
      res_lat = res.get("latency_sec", 0.0)
      p_tok = res.get("prompt_tokens", 0)
      c_tok = res.get("candidates_tokens", 0)
      th_tok = res.get("thinking_tokens", 0)
      print(f"    [OK] Response:        {res_text}")
      print(f"    Latency:              {res_lat:.2f}s")
      print(f"    Prompt Tokens:        {p_tok}")
      print(f"    Output Tokens:        {c_tok}")
      if th_tok > 0:
        print(f"    Thinking Tokens:      {th_tok}")
    else:
      err_msg = res.get("error")
      print(f"    [FAIL] Error: {err_msg}", file=sys.stderr)

  print("\n[DONE] DIAGNOSTIC COMPLETE")


if __name__ == "__main__":
  main()
