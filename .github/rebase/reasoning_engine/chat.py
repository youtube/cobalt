#!/usr/bin/env python3
"""Interactive Chat and Verification CLI for Cobalt Rebase Reasoning Engine.

Allows engineers to converse directly with Vertex AI Gemini and Claude Sonnet
models pre-loaded with Cobalt rebase skills, GN build diagnostics, and compiler
healing guides, or run automated verification tests against local/remote engines.

Usage:
  # 1. Interactive chat with default model
  python3 .github/rebase/reasoning_engine/chat.py

  # 2. Interactive chat with Expert Tier (Claude Sonnet 4.6)
  python3 .github/rebase/reasoning_engine/chat.py --model expert

  # 3. Test verification of all models & skills
  python3 .github/rebase/reasoning_engine/chat.py --test

  # 4. Test remote deployed Reasoning Engine instance
  python3 .github/rebase/reasoning_engine/chat.py --remote <RESOURCE_ID> --test
"""

import argparse
import os
import readline  # pylint: disable=unused-import
import sys
from typing import Any, Dict, List, Optional

# Ensure reasoning_engine package can be imported
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
if SCRIPT_DIR not in sys.path:
  sys.path.insert(0, SCRIPT_DIR)
if PARENT_DIR not in sys.path:
  sys.path.insert(0, PARENT_DIR)

# pylint: disable=wrong-import-position
from reasoning_engine.engine import CobaltReasoningEngine
from engine_client import ReasoningEngineClient


def load_latest_failure(repo_root: Optional[str] = None) -> str:
  """Retrieves the latest failure description from logs."""
  root = repo_root or os.path.abspath(
      os.path.join(
          os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
  log_file = os.path.join(root, "out", "rebase_pipeline.log")
  if os.path.isfile(log_file):
    try:
      with open(log_file, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
      err_block = []
      for line in reversed(lines[-300:]):
        sline = line.strip()
        if (sline.startswith(("ERROR at //", "[FAIL]", "FAILED:", "Error:",
                              "Traceback (most recent call last):")) or
            "Can't include this header" in sline):
          err_block.insert(0, sline)
          if len(err_block) >= 20:
            break
        elif err_block:
          err_block.insert(0, sline)
          if len(err_block) >= 20:
            break
      if err_block:
        return "\n".join(err_block)
      tail_lines = [line.strip() for line in lines[-25:] if line.strip()]
      return "\n".join(tail_lines)
    except OSError:
      pass
  return "No recent failure records found."


def print_banner(
    project_id: str,
    location: str,
    model: str,
    mode: str,
    latest_failure: str = "",
    remote_id: Optional[str] = None,
):
  """Prints welcome banner and active configuration."""
  print("=" * 80)
  print(" COBALT REBASE REASONING ENGINE - INTERACTIVE CHAT & VERIFICATION")
  print("=" * 80)
  print(f"  - GCP Project  : {project_id}")
  print(f"  - Vertex Region: {location}")
  print(f"  - Active Tier  : {model}")
  print(f"  - Active Mode  : {mode.upper()} (Skills: {mode}_healing / rebase)")
  if remote_id:
    print(f"  - Remote Engine: {remote_id}")
  if latest_failure and "No recent" not in latest_failure:
    first_line = latest_failure.splitlines()[0][:70]
    print(f"  - Failure Memo : {first_line}")
  print("-" * 80)
  print(
      " Commands: /mode <gn|compiler|conflict|rebase>  /model <flash|pro|expert>"
  )
  print("           /file <path>   /failure   /test   /clear   /help   /exit")
  print("=" * 80)
  print()


def print_help():
  """Prints interactive command guide."""
  print("\n--- Available Commands ---")
  print("  /mode <rebase|gn|compiler|conflict> : Switch active domain skill")
  print("  /model <flash|pro|expert|sonnet>    : Switch model tier")
  print(
      "  /test                                : Run automated tier verification"
  )
  print("  /file <relative_filepath>            : Attach file to prompt")
  print("  /failure                             : Show latest failure memory")
  print("  /clear                               : Clear conversation history")
  print("  /exit, /quit                         : Exit chat session\n")


def run_verification_test(engine_or_client: Any) -> bool:
  """Runs an automated verification test across Flash and Expert tiers."""
  print("\n==================================================")
  print(" RUNNING REASONING ENGINE VERIFICATION SUITE")
  print("==================================================")

  # Test 1: Tier 1 Flash Model
  print("\n[TEST 1] Testing Tier-1 Flash Model (Gemini 2.5 Flash)...")
  try:
    if hasattr(engine_or_client, "resolve_conflict"):
      res = engine_or_client.resolve_conflict(
          file_path="cobalt/browser/web_contents.cc",
          language="C++",
          raw_conflict="<<<<<<< HEAD\nint a = 1;\n=======\nint a = 2;\n>>>>>>> upstream",
          use_pro=False,
          use_expert=False,
      )
    else:
      res = engine_or_client.query(
          action="resolve_conflict",
          file_path="cobalt/browser/web_contents.cc",
          language="C++",
          raw_conflict="<<<<<<< HEAD\nint a = 1;\n=======\nint a = 2;\n>>>>>>> upstream",
          use_pro=False,
          use_expert=False,
      )
    status = res.get("status")
    model = res.get("model_used", "flash")
    print(f"  [RESULT] Status: {status} | Model Used: {model}")
    if status != "SUCCESS":
      print(f"  [FAIL] Error: {res.get('replacement')}")
      return False
    print("  [PASS] Tier-1 Flash operational!")
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f"  [FAIL] Test 1 encountered exception: {e}")
    return False

  # Test 2: Tier-2 Senior Architect Guidance (Claude Sonnet 4.6)
  print(
      "\n[TEST 2] Testing Tier-2 Senior Architect (Claude Sonnet 4.6 Guidance)..."
  )
  guidance_text = ""
  try:
    if hasattr(engine_or_client, "generate_expert_guidance"):
      res = engine_or_client.generate_expert_guidance(
          target="cobalt",
          diagnostics="v8/src/wasm/wrappers.cc:20:10: fatal error: 'src/compiler/turboshaft/wasm-graph-builder-base.h' file not found",
          source_contexts="20: #include \"src/compiler/turboshaft/wasm-graph-builder-base.h\"",
          trajectory_history="- Iteration 1: Modified v8/src/wasm/wrappers.cc -> failed\n- Iteration 2: Replaced header with dummy -> failed",
          working_diff="--- a/v8/src/wasm/wrappers.cc\n+++ b/v8/src/wasm/wrappers.cc\n@@ -20,1 +20,1 @@\n-#include \"src/wasm/old.h\"\n+#include \"src/compiler/turboshaft/wasm-graph-builder-base.h\"",
          mode="compiler",
      )
    else:
      res = engine_or_client.query(
          action="generate_expert_guidance",
          target="cobalt",
          diagnostics="v8/src/wasm/wrappers.cc:20:10: fatal error: 'src/compiler/turboshaft/wasm-graph-builder-base.h' file not found",
          source_contexts="20: #include \"src/compiler/turboshaft/wasm-graph-builder-base.h\"",
          trajectory_history="- Iteration 1: Modified v8/src/wasm/wrappers.cc -> failed\n- Iteration 2: Replaced header with dummy -> failed",
          working_diff="--- a/v8/src/wasm/wrappers.cc\n+++ b/v8/src/wasm/wrappers.cc\n@@ -20,1 +20,1 @@\n-#include \"src/wasm/old.h\"\n+#include \"src/compiler/turboshaft/wasm-graph-builder-base.h\"",
          mode="compiler",
      )
    status = res.get("status")
    model = res.get("model_used", "claude-sonnet-4-6")
    guidance_text = res.get("guidance", "")
    print(f"  [RESULT] Status: {status} | Architect Model: {model}")
    print(f"  [ARCHITECT DIRECTIVE]:\n{guidance_text[:250]}...")
    if status != "SUCCESS":
      print("  [FAIL] Expert Guidance query returned non-success.")
      return False
    print("  [PASS] Tier-2 Senior Architect guidance operational!")
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f"  [FAIL] Test 2 encountered exception: {e}")
    return False

  # Test 3: Tier-1 Flash implementing Senior Architect's Guidance
  print(
      "\n[TEST 3] Testing Tier-1 Flash worker implementing Architect Guidance..."
  )
  try:
    if hasattr(engine_or_client, "heal_compiler_error"):
      res = engine_or_client.heal_compiler_error(
          target="cobalt",
          diagnostics="v8/src/wasm/wrappers.cc:20:10: fatal error: 'src/compiler/turboshaft/wasm-graph-builder-base.h' file not found",
          source_contexts="20: #include \"src/compiler/turboshaft/wasm-graph-builder-base.h\"",
          expert_guidance=guidance_text,
          use_pro=False,
      )
    else:
      res = engine_or_client.query(
          action="heal_compiler_error",
          target="cobalt",
          diagnostics="v8/src/wasm/wrappers.cc:20:10: fatal error: 'src/compiler/turboshaft/wasm-graph-builder-base.h' file not found",
          source_contexts="20: #include \"src/compiler/turboshaft/wasm-graph-builder-base.h\"",
          expert_guidance=guidance_text,
          use_pro=False,
      )
    status = res.get("status")
    model = res.get("model_used", "flash")
    print(f"  [RESULT] Status: {status} | Worker Model: {model}")
    print(f"  [WORKER PATCH OUTPUT]:\n{res.get('patch', '')[:200]}...")
    if status != "SUCCESS":
      print("  [FAIL] Tier-1 Flash guided healing returned non-success.")
      return False
    print("  [PASS] Tier-1 Flash successfully executed Architect guidance!")
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f"  [FAIL] Test 3 encountered exception: {e}")
    return False

  print("\n==================================================")
  print(" [SUCCESS] ALL REASONING ENGINE TIERS VERIFIED!")
  print("==================================================\n")
  return True


def main():
  """Main entry point for interactive chat and verification CLI."""
  parser = argparse.ArgumentParser(
      description="Interactive Chat and Verification for Cobalt Reasoning Engine"
  )
  parser.add_argument(
      "--project-id",
      default=os.environ.get("GCP_PROJECT") or
      os.environ.get("GOOGLE_CLOUD_PROJECT") or "lxn-test",
      help="GCP Project ID for Vertex AI (default: lxn-test)",
  )
  parser.add_argument(
      "--location",
      default=os.environ.get("GCP_LOCATION", "us-central1"),
      help="Vertex AI region (default: us-central1)",
  )
  parser.add_argument(
      "--remote",
      "--resource-id",
      dest="resource_id",
      default=os.environ.get("REASONING_ENGINE_ID"),
      help="Optional Remote Reasoning Engine Resource ID to test hosted engine",
  )
  parser.add_argument(
      "--model",
      default=os.environ.get("GEMINI_MODEL", "gemini-2.5-flash"),
      help="Default model tier: 'flash', 'pro', 'expert', or 'claude-sonnet-4-6'",
  )
  parser.add_argument(
      "--mode",
      choices=["rebase", "gn", "compiler", "conflict"],
      default="rebase",
      help="Domain skill mode (default: rebase)",
  )
  parser.add_argument(
      "--test",
      action="store_true",
      help="Run automated end-to-end verification suite across models",
  )
  parser.add_argument(
      "--prompt",
      default="",
      help="One-shot prompt to evaluate without entering interactive REPL",
  )
  args = parser.parse_args()

  if args.resource_id:
    print(
        f"[chat] Connecting to Remote Reasoning Engine: {args.resource_id}...")
    engine = ReasoningEngineClient(
        resource_id=args.resource_id,
        project_id=args.project_id,
        location=args.location,
    )
  else:
    engine = CobaltReasoningEngine(
        project_id=args.project_id,
        location=args.location,
        flash_model="gemini-2.5-flash",
        pro_model="gemini-2.5-pro",
        expert_model="claude-sonnet-4-6",
    )

  if args.test:
    ok = run_verification_test(engine)
    sys.exit(0 if ok else 1)

  current_mode = args.mode
  current_model = args.model
  history: List[Dict[str, str]] = []
  latest_failure = load_latest_failure()

  # One-shot mode
  if args.prompt:
    use_expert = current_model in ("expert", "claude", "sonnet",
                                   "claude-sonnet-4-6")
    use_pro = "pro" in current_model.lower() or use_expert
    prompt_with_context = args.prompt
    if latest_failure and "No recent" not in latest_failure:
      prompt_with_context = (
          f"[Latest Pipeline Failure Memory]\n{latest_failure}\n\n"
          f"[User Query]\n{args.prompt}")
    res = engine.chat(
        prompt_with_context,
        history=[],
        mode=current_mode,
        use_pro=use_pro,
    )
    if res.get("status") == "SUCCESS":
      print(res.get("response", ""))
      sys.exit(0)
    else:
      err_msg = res.get("response")
      print(f"Error: {err_msg}", file=sys.stderr)
      sys.exit(1)

  # Interactive REPL mode
  print_banner(
      args.project_id,
      args.location,
      current_model,
      current_mode,
      latest_failure,
      remote_id=args.resource_id,
  )

  while True:
    try:
      user_input = input(f"[{current_mode}|{current_model}] >>> ").strip()
      if not user_input:
        continue

      if user_input in ("/exit", "/quit", "exit", "quit"):
        print("Exiting Cobalt Reasoning Engine. Goodbye!")
        break

      if user_input == "/help":
        print_help()
        continue

      if user_input == "/test":
        run_verification_test(engine)
        continue

      if user_input == "/clear":
        history.clear()
        print("[OK] Conversation history cleared.\n")
        continue

      if user_input == "/failure":
        print(f"\n--- Latest Failure Memory ---\n{latest_failure}\n")
        user_input = (
            "Analyze the latest pipeline failure above. Explain why it "
            "occurred and suggest how to resolve it.")

      elif user_input.startswith("/model"):
        parts = user_input.split(maxsplit=1)
        if len(parts) > 1:
          target_model = parts[1].strip()
          if target_model in ("flash", "gemini-2.5-flash", "2.5-flash"):
            current_model = "gemini-2.5-flash"
          elif target_model in ("pro", "gemini-2.5-pro"):
            current_model = "gemini-2.5-pro"
          elif target_model in ("expert", "claude", "sonnet",
                                "claude-sonnet-4-6"):
            current_model = "claude-sonnet-4-6"
          else:
            current_model = target_model
          print(f"[OK] Model tier switched to: {current_model}\n")
        else:
          print(f"Current model: {current_model}")
        continue

      elif user_input.startswith("/mode"):
        parts = user_input.split(maxsplit=1)
        if len(parts) > 1 and parts[1].strip() in (
            "rebase",
            "gn",
            "compiler",
            "conflict",
        ):
          current_mode = parts[1].strip()
          print(f"[OK] Domain mode switched to: {current_mode.upper()}\n")
        else:
          print("Valid modes: rebase, gn, compiler, conflict")
        continue

      elif user_input.startswith("/file"):
        parts = user_input.split(maxsplit=1)
        if len(parts) > 1:
          file_rel = parts[1].strip()
          if os.path.isfile(file_rel):
            with open(file_rel, "r", encoding="utf-8", errors="replace") as f:
              content = f.read()
            user_input = (
                f"File: {file_rel}\n```\n{content}\n```\n\n"
                "Please analyze this file and its relevance to Cobalt rebase.")
            print(f"[OK] Attached file: {file_rel} ({len(content)} bytes)")
          else:
            print(f"[ERROR] File not found: {file_rel}")
            continue

      print("\n[Thinking...]")
      use_expert = current_model in ("expert", "claude", "sonnet",
                                     "claude-sonnet-4-6")
      use_pro = "pro" in current_model.lower() or use_expert
      res = engine.chat(
          user_input,
          history=history,
          mode=current_mode,
          use_pro=use_pro,
          failure_memory=latest_failure,
      )

      if res.get("status") == "SUCCESS":
        answer = res.get("response", "")
        print(f"\n{answer}\n")
        history.append({"role": "user", "content": user_input})
        history.append({"role": "model", "content": answer})
      else:
        err_resp = res.get("response")
        print(f"\n[ERROR] Request failed: {err_resp}\n")

    except (KeyboardInterrupt, EOFError):
      print("\nExiting Cobalt Reasoning Engine. Goodbye!")
      break


if __name__ == "__main__":
  main()
