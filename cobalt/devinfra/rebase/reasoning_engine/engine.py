#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Vertex AI Reasoning Engine Service for Cobalt Chromium Rebase.

This module is packaged and deployed to Google Cloud Vertex AI to execute
remote reasoning, prompt construction, and LLM calls.
Local resolvers communicate with this hosted service via ReasoningEngineClient.
"""

import contextvars
import os
import sys
import time
from typing import Any, Dict, List, Optional
import warnings

from google import genai
from google.genai import types

# Suppress google.genai informational warnings about AFC on direct model calls
warnings.filterwarnings("ignore", message=".*automatic function calling.*")
warnings.filterwarnings("ignore", category=UserWarning, module="google.genai")

# Disable mTLS endpoint on Cloudtop to ensure clean Vertex AI transport.
os.environ["GOOGLE_API_USE_CLIENT_CERTIFICATE"] = "false"
os.environ["GOOGLE_API_USE_MTLS_ENDPOINT"] = "never"

SKILLS_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "skills",
)
_SKILL_CACHE: Dict[str, str] = {}

# Token usage of the model calls made while serving the current query(). A
# ContextVar keeps concurrent queries on the hosted engine apart. It cannot be
# pickled, so deploy.py re-creates it by name when the engine is deployed.
_QUERY_USAGE: contextvars.ContextVar[List[Dict[str, Any]]] = (
    contextvars.ContextVar("query_usage"))


def _record_usage(model: str, prompt: Optional[int],
                  completion: Optional[int]) -> None:
  """Adds one model call's token counts to the current query's usage."""
  usage = _QUERY_USAGE.get(None)
  if usage is None:
    return
  prompt = prompt or 0
  completion = completion or 0
  usage.append({
      "model": model,
      "prompt_tokens": prompt,
      "completion_tokens": completion,
      "total_tokens": prompt + completion,
  })


# Shared investigation-tool protocol injected into agent prompts.
#
# Rationale: prompts that only illustrated TOOL_ directives inline within prose
# (e.g. "output TOOL_ commands (e.g. `TOOL_READ_FILE: ...`, `TOOL_GREP: ...`)")
# taught models to emit directives as inline, backtick-wrapped tokens. Observed
# failures included a dozen speculative directives in one response and several
# directives concatenated onto a single line with a `FILE:` patch header glued
# to the end, which the resolver could not parse or apply. These rules are
# stated explicitly and identically everywhere so the contract is unambiguous.
MAX_TOOL_CALLS_PER_TURN = 3

_TOOL_CALL_RULES_HEADER = "TOOL CALL PROTOCOL (MANDATORY):\n"

# Batching rule: caps speculative directive dumps.
_TOOL_CALL_BUDGET_RULE = (
    f"- Request AT MOST {MAX_TOOL_CALLS_PER_TURN} TOOL_ directives per "
    "response. Request only what you will actually reason over; you will be "
    "called again with the results and may request more. Do NOT speculatively "
    "enumerate directives.\n")

# Formatting rule: keeps directives machine-parseable.
_TOOL_CALL_FORMAT_RULE = (
    "- Each TOOL_ directive must be on its OWN line, starting at the first "
    "character of that line, in plain text. No backticks, no code fences, no "
    "markdown bullets or numbering, no 'Tool Call:' prefix, and no trailing "
    "prose on the same line.\n")

# Exclusivity rule: only Tier-1 workers emit patch blocks.
_TOOL_CALL_EXCLUSIVITY_RULE = (
    "- EITHER investigate OR patch, never both. A single response contains "
    "either TOOL_ directives or exactly one patch block (SEARCH/REPLACE or "
    "DELETE). Never emit a TOOL_ directive in the same response as a patch "
    "block; a response containing a patch block ends the investigation.\n")

_TOOL_CALL_EXAMPLES = ("Correct:\n"
                       "TOOL_READ_FILE: media/mojo/mojom/BUILD.gn 1-60\n"
                       "TOOL_GREP: use_starboard_media "
                       "media/mojo/mojom/BUILD.gn\n"
                       "Incorrect (rejected):\n"
                       "- `TOOL_READ_FILE: a.gn 1-60`, `TOOL_GREP: foo`\n"
                       "TOOL_GREP: fooTOOL_READ_FILE: a.gn 1 60\n")

# Applies to every agent that emits directives, including the Tier-2 Architect.
TOOL_CALL_FORMAT_RULES = (
    _TOOL_CALL_RULES_HEADER + _TOOL_CALL_BUDGET_RULE + _TOOL_CALL_FORMAT_RULE +
    _TOOL_CALL_EXAMPLES)

# Tier-1 worker protocol: adds mutual exclusivity with patch blocks.
TOOL_CALL_PROTOCOL = (
    _TOOL_CALL_RULES_HEADER + _TOOL_CALL_BUDGET_RULE + _TOOL_CALL_FORMAT_RULE +
    _TOOL_CALL_EXCLUSIVITY_RULE + _TOOL_CALL_EXAMPLES)


def load_skill(skill_name: str, skills_dir: Optional[str] = None) -> str:
  """Loads and caches domain instructions from a skill markdown file."""
  base_dir = skills_dir or SKILLS_DIR
  fname = skill_name if skill_name.endswith(".md") else f"{skill_name}.md"
  cache_key = os.path.join(base_dir, fname)

  if cache_key in _SKILL_CACHE:
    return _SKILL_CACHE[cache_key]

  if not os.path.isfile(cache_key):
    raise FileNotFoundError(
        f"Required skill file not found: {cache_key}. "
        "Ensure skills directory is present and contains valid .md skills.")

  with open(cache_key, "r", encoding="utf-8") as f:
    content = f.read().strip()
    if content.startswith("---"):
      parts = content.split("---", 2)
      if len(parts) >= 3:
        content = parts[2].strip()
    _SKILL_CACHE[cache_key] = content
    return content


# pylint: disable=unused-argument
class CobaltReasoningEngine:
  """Hosted Vertex AI Reasoning Engine service for Cobalt rebase tasks."""

  def __init__(
      self,
      *,
      project_id: Optional[str] = None,
      location: str = "global",
      flash_model: str,
      expert_model: str,
      expert_location: Optional[str] = None,
      skills_dir: Optional[str] = None,
      **kwargs,
  ):
    self.project_id = (
        project_id or os.environ.get("GCP_PROJECT") or
        os.environ.get("GOOGLE_CLOUD_PROJECT"))
    self.location = location
    if not flash_model or not expert_model:
      raise ValueError("flash_model and expert_model are required.")
    self.flash_model = flash_model
    self.expert_model = expert_model
    self.expert_location = (
        expert_location or os.environ.get("EXPERT_LOCATION") or "global")
    self.skills_dir = skills_dir or SKILLS_DIR
    self.anthropic_client: Any = None
    self.skill_cache: Dict[str, str] = {
        "cobalt_rebase":
            load_skill("cobalt_rebase", self.skills_dir),
        "cobalt_rebase_patterns":
            load_skill("cobalt_rebase_patterns", self.skills_dir),
        "conflict_resolution":
            load_skill("conflict_resolution", self.skills_dir),
        "gn_healing":
            load_skill("gn_healing", self.skills_dir),
        "compiler_healing":
            load_skill("compiler_healing", self.skills_dir),
    }
    self.client: Optional[genai.Client] = None

  def set_up(self):
    """Initializes Google GenAI client in remote Vertex AI container."""
    loc = "global" if ("gemini-3" in self.flash_model.lower() or
                       self.location == "global") else self.location
    self.client = genai.Client(
        vertexai=True,
        project=self.project_id,
        location=loc,
    )

  def _get_client(self) -> genai.Client:
    """Returns active client instance, initializing if needed."""
    if self.client is None:
      self.set_up()
    return self.client

  def _get_anthropic_client(self) -> Any:
    """Lazy loads Anthropic client on Vertex AI Model Garden."""
    if self.anthropic_client is None:
      try:
        from anthropic import AnthropicVertex  # pylint: disable=import-outside-toplevel
        anthropic_region = ("us-east5" if self.expert_location == "global" else
                            self.expert_location)
        self.anthropic_client = AnthropicVertex(
            project_id=self.project_id,
            region=anthropic_region,
        )
      except Exception as e:  # pylint: disable=broad-exception-caught
        print(
            "  [REASONING_ENGINE] Notice: Could not init AnthropicVertex: "
            f"{e}",
            file=sys.stderr,
        )
        self.anthropic_client = None
    return self.anthropic_client

  def _normalize_anthropic_model_name(self, model_name: str) -> str:
    """Normalizes friendly model aliases to official publisher IDs."""
    m = model_name.strip().lower()
    if m in ("claude-sonnet-5", "sonnet-5", "sonnet 5"):
      return "claude-sonnet-5"
    if m in ("claude-opus-5", "opus-5", "opus 5"):
      return "claude-opus-5"
    return model_name

  def _generate_expert_content(
      self,
      contents: Any,
      system_instruction: str,
      expert_model: Optional[str] = None,
  ) -> Optional[str]:
    """Generates content via the Tier-2 expert model (Gemini or Claude)."""
    expert_name = expert_model or self.expert_model

    # 1. Anthropic Claude on Vertex AI (e.g. Claude Sonnet 5)
    if "claude" in expert_name.lower():
      aclient = self._get_anthropic_client()
      if aclient is not None:
        prompt_text = str(contents) if not isinstance(contents,
                                                      str) else contents
        normalized_model = self._normalize_anthropic_model_name(expert_name)
        for attempt in range(1, 4):
          try:
            print(
                f"  [REASONING_ENGINE] [EXPERT_TIER] Dispatching to Anthropic "
                f"{normalized_model} (attempt {attempt}/3)...",
                file=sys.stderr,
            )
            resp = aclient.messages.create(
                model=normalized_model,
                max_tokens=8192,
                system=system_instruction,
                messages=[{
                    "role": "user",
                    "content": prompt_text
                }],
            )
            if resp.usage is not None:
              _record_usage(normalized_model, resp.usage.input_tokens,
                            resp.usage.output_tokens)
            text_parts = []
            for block in resp.content:
              if hasattr(block, "text") and block.text:
                text_parts.append(block.text)
              elif getattr(block, "type", "") == "text":
                text_parts.append(getattr(block, "text", ""))
            full_text = "\n".join(text_parts).strip("\n")
            if full_text:
              return full_text
          except Exception as e:  # pylint: disable=broad-exception-caught
            print(
                f"  [REASONING_ENGINE] Warning: Anthropic attempt "
                f"{attempt}/3 failed: {e}",
                file=sys.stderr,
            )
            if attempt < 3:
              time.sleep(attempt * 4.0)
            else:
              print(
                  "  [REASONING_ENGINE] Falling back to Gemini Expert...",
                  file=sys.stderr,
              )

    # 2. Gemini thinking expert; a non-Gemini expert falls back to the
    # (Gemini) workhorse model.
    target_gemini_model = (
        expert_name if "gemini" in expert_name.lower() else self.flash_model)
    print(
        f"  [REASONING_ENGINE] [EXPERT_TIER] Dispatching to "
        f"{target_gemini_model} with thinking...",
        file=sys.stderr,
    )
    cfg = types.GenerateContentConfig(
        system_instruction=system_instruction,
        thinking_config=types.ThinkingConfig(thinking_budget=2048),
    )
    resp = self._generate_content_with_retry(
        model=target_gemini_model,
        contents=contents,
        config=cfg,
        tier_label="",
    )
    return resp.text.strip("\n") if resp and resp.text else ""

  def _get_skill(self, name: str) -> str:
    """Retrieves skill instructions from cache or disk."""
    if name in self.skill_cache:
      return self.skill_cache[name]
    return load_skill(name, self.skills_dir)

  def _generate_content_with_retry(
      self,
      model: str,
      contents: Any,
      config: types.GenerateContentConfig,
      *,
      max_retries: int = 5,
      initial_backoff: float = 4.0,
      tier_label: Optional[str] = None,
  ) -> Optional[types.GenerateContentResponse]:
    """Calls generate_content with exponential backoff for 429/503 errors."""
    client = self._get_client()
    backoff = initial_backoff
    if tier_label is None:
      if self.expert_model and model == self.expert_model:
        tier_label = "EXPERT_TIER"
      else:
        tier_label = "WORKHORSE_TIER"
    if tier_label:
      print(
          f"  [REASONING_ENGINE] [{tier_label}] Dispatching to {model} "
          "(thinking tokens enabled)...",
          file=sys.stderr,
      )
    for attempt in range(1, max_retries + 1):
      try:
        resp = client.models.generate_content(
            model=model,
            contents=contents,
            config=config,
        )
        meta = resp.usage_metadata
        if meta is not None:
          _record_usage(model, meta.prompt_token_count,
                        (meta.candidates_token_count or 0) +
                        (meta.thoughts_token_count or 0))
        return resp
      except Exception as e:  # pylint: disable=broad-exception-caught
        error_msg = str(e)
        if any(
            code in error_msg
            for code in ("429", "ResourceExhausted", "503", "Unavailable")):
          if attempt == max_retries:
            print(
                f"  [REASONING_ENGINE] Rate limit exceeded ({model}) after "
                f"{attempt} attempts: {e}",
                file=sys.stderr,
            )
            return None
          print(
              f"  [REASONING_ENGINE] Rate limit/busy ({model}) on attempt "
              f"{attempt}. Retrying in {backoff:.1f}s...",
              file=sys.stderr,
          )
          time.sleep(backoff)
          backoff *= 2.0
        else:
          print(
              f"  [REASONING_ENGINE] API error ({model}): {e}",
              file=sys.stderr,
          )
          if attempt == max_retries:
            return None
          time.sleep(backoff)
          backoff *= 1.5
    return None

  def _create_generation_config(
      self,
      chosen_model: str,
      system_instruction: str,
      temperature: float = 0.1,
  ) -> types.GenerateContentConfig:
    """Constructs model-specific GenerateContentConfig."""
    if "gemini-3." in chosen_model or "thinking" in chosen_model.lower():
      return types.GenerateContentConfig(
          system_instruction=system_instruction,
          thinking_config=types.ThinkingConfig(thinking_budget=1024),
      )
    return types.GenerateContentConfig(
        system_instruction=system_instruction,
        temperature=temperature,
    )

  def query(self, action: str = "resolve_conflict", **kwargs) -> Dict[str, Any]:
    """Primary query dispatcher for Vertex AI Reasoning Engine.

    The result carries "usage": one entry per model call made for it.
    """
    usage: List[Dict[str, Any]] = []
    token = _QUERY_USAGE.set(usage)
    try:
      result = self._dispatch(action, **kwargs)
    finally:
      _QUERY_USAGE.reset(token)
    result["usage"] = usage
    return result

  def _dispatch(self, action: str, **kwargs) -> Dict[str, Any]:
    if action in ("generate_expert_guidance", "expert_guidance"):
      return self.generate_expert_guidance(**kwargs)
    if action == "resolve_conflict":
      return self.resolve_conflict(**kwargs)
    if action in ("heal_gn", "heal_gn_error"):
      return self.heal_gn_error(**kwargs)
    if action in ("heal_compiler", "heal_compiler_break",
                  "heal_compiler_error"):
      return self.heal_compiler_error(**kwargs)
    if action == "judge_commit":
      return self.judge_commit(**kwargs)
    raise ValueError(f"Unknown Reasoning Engine action: {action}")

  def judge_commit(
      self,
      *,
      phase: str = "",
      trigger: str = "",
      outcome: str = "",
      diff: str = "",
      rationales: str = "",
      flash_model: Optional[str] = None,
      **kwargs,
  ) -> Dict[str, Any]:
    """Decides whether pending build fixes are one complete, commit-worthy fix.

    Called by the Phase 2-4 loop when the error that prompted the pending
    patches has changed after a build (the deterministic rule cannot tell
    "fixed" from "same problem, different message"). The workhorse model
    sees the triggering diagnostic, the new build outcome, the pending diff
    and the per-patch rationales, and answers with JSON:
      {"decision": "COMMIT" | "HOLD", "subject": ..., "body": ...,
       "reason": ...}
    subject/body are a reviewer-facing commit message in the style of the
    hand-landed roll fixes ("<file>: Adapt to ..."); the pipeline appends
    its own Trigger / Why / Model trailer.
    """
    del kwargs
    chosen_model = flash_model or self.flash_model
    sys_inst = (
        "You review changes made by an automated Chromium-to-Cobalt rebase "
        "agent and decide whether they should become one git commit now.\n"
        "COMMIT when the pending changes are a complete, coherent fix for the "
        "triggering error: the error is gone and the new outcome is a "
        "different problem or a passing build. HOLD when the new error is the "
        "same underlying problem with a different message, when the change "
        "only moved or silenced the error, or when the fix is clearly half "
        "done (e.g. a header changed but its .cc not yet).\n"
        "Respond with ONLY a JSON object: "
        "{\"decision\": \"COMMIT\" or \"HOLD\", "
        "\"subject\": \"<file>: <what was fixed>, at most 72 characters\", "
        "\"body\": \"1-3 sentences: what upstream changed and why this is the "
        "right Cobalt-side fix\", \"reason\": \"one sentence for the log\"}. "
        "No markdown fences, no extra text.")
    rationales = rationales or "(none)"
    diff = diff or "(empty)"
    prompt = (f"Phase: {phase}\n\n"
              f"--- Triggering error (before the pending changes) ---\n"
              f"{trigger}\n\n"
              f"--- Outcome of the build after the pending changes ---\n"
              f"{outcome}\n\n"
              f"--- Rationale recorded for each pending patch ---\n"
              f"{rationales}\n\n"
              f"--- Pending diff ---\n{diff}\n")
    resp = self._generate_content_with_retry(
        model=chosen_model,
        contents=prompt,
        config=self._create_generation_config(chosen_model, sys_inst),
    )
    return {
        "status": "SUCCESS" if resp and resp.text else "ERROR",
        "judgement": resp.text.strip() if resp and resp.text else "",
        "model_used": chosen_model,
    }

  def generate_expert_guidance(
      self,
      target: str = "",
      diagnostics: str = "",
      source_contexts: str = "",
      *,
      error_trace: str = "",
      file_context: str = "",
      target_file: str = "",
      trajectory_history: str = "",
      working_diff: str = "",
      investigation_history: str = "",
      raw_log: str = "",
      all_diagnostics: str = "",
      mode: str = "compiler",
      expert_model: Optional[str] = None,
      **kwargs,
  ) -> Dict[str, Any]:
    """Tier-2 Senior Architect (Gemini 3.8 Flash / Claude).

    Analyzes full failure trajectory, session git diffs, raw logs, and
    diagnostics to produce strategic root-cause guidance and actionable
    refactoring directives for Tier-1 (Worker Agent).
    """
    eff_target = target or target_file or "cobalt"
    eff_diag = diagnostics or error_trace
    eff_ctx = source_contexts or file_context
    chosen_expert = expert_model or self.expert_model

    rebase_skill = self._get_skill("cobalt_rebase")
    domain_skill = self._get_skill(
        "conflict_resolution" if mode == "conflict" else (
            "gn_healing" if mode == "gn" else "compiler_healing"))
    patterns_skill = self._get_skill("cobalt_rebase_patterns")

    sys_inst = (
        "You are the Senior Principal Chromium and Cobalt Systems Architect.\n"
        "Your role is NOT to write syntax diffs, but to strategically diagnose "
        "conflicts, complex build failures, V8/Blink/Starboard refactorings, "
        "and multi-iteration loops.\n\n"
        "You have OMNISCIENT visibility into the entire agent session:\n"
        "1. Complete chronological trajectory of every change, patch, and "
        "compiler/gn error Tier-1 Worker encountered.\n"
        "2. Complete Git diff of modifications made in the working tree "
        "across all iterations.\n"
        "3. Raw compiler/build output logs and all extracted diagnostics.\n"
        "4. Offending source code and investigation tool results.\n\n"
        "Responsibilities:\n"
        "- Analyze why the worker agent is stuck or cycling in a loop.\n"
        "- Diagnose whether prior patches introduced unintended side effects, "
        "incorrect stub signatures, or broke other translation units.\n"
        "- Perform deep root-cause analysis: identify what upstream changed.\n"
        "- Upstream Roll Commits: a roll lands as 'Revert Cobalt', then "
        "'Update to <milestone>' (pure upstream), then 'CONFLICTED Cherry "
        "pick ...' (Cobalt re-applied). Use TOOL_UPSTREAM_DIFF to see what "
        "upstream changed, and TOOL_COBALT_DIFF to see what Cobalt adds on "
        "top. When a symbol vanishes after a roll, compare BOTH: upstream "
        "may have moved it to a different build target, leaving Cobalt's "
        "customization attached to the old one.\n"
        "- If you need to inspect files, symbols, or git history, output:\n"
        "TOOL_FIND_FILE: <pattern>\n"
        "TOOL_GREP: <symbol>\n"
        "TOOL_READ_FILE: <path> <start>-<end>\n"
        "TOOL_UPSTREAM_DIFF: <filepath>\n"
        "TOOL_COBALT_DIFF: <filepath>\n"
        "TOOL_GIT_LOG: <count> <filepath>\n"
        "TOOL_GIT_SHOW: <commit_hash>\n"
        "TOOL_GIT_DIFF: <rev1>..<rev2>\n"
        "TOOL_READ_PR: <pr_number>\n"
        "TOOL_PR_DIFF: <pr_number>\n"
        "TOOL_GET_HISTORY: <count | all | iteration_number | filepath>\n"
        "- Change History: By default you are shown the last 10 change "
        "records. If you need to inspect earlier records or a specific "
        "iteration that caused the issue, call TOOL_GET_HISTORY: <count> or "
        "TOOL_GET_HISTORY: iteration <number>.\n"
        "- Explain why Tier 1 is stuck and list anti-patterns to avoid.\n"
        "- Provide clear, concrete, step-by-step instructions for Tier 1:\n"
        "  * For conflicts: state whether to choose HEAD, incoming (theirs), "
        "or combine both.\n"
        "  * For build breaks: first-party file(s) to edit, precise logic, "
        "type substitution, or header include to apply.\n\n"
        f"{TOOL_CALL_FORMAT_RULES}\n"
        f"--- Rebase Guidelines ---\n{rebase_skill}\n\n"
        f"--- Domain Skill ---\n{domain_skill}\n\n"
        f"--- Cobalt Rebase Patterns & Stubs Hygiene ---\n{patterns_skill}\n")

    diff_section = (f"--- Git Diff of Modifications in Current Session ---\n"
                    f"{working_diff[:200000]}\n\n" if working_diff else "")
    raw_log_section = (f"--- Full Raw Build Output Log (Tail) ---\n"
                       f"```\n{raw_log[:40000]}\n```\n\n" if raw_log else "")
    all_diags_section = (f"--- All Extracted Diagnostics in Current Build ---\n"
                         f"{all_diagnostics[:40000]}\n\n"
                         if all_diagnostics else "")
    traj_section = (f"--- Prior Iteration Attempt Trajectory ---\n"
                    f"{trajectory_history}\n\n" if trajectory_history else "")
    inv_section = (f"--- Investigation Tool Results ---\n"
                   f"{investigation_history}\n\n"
                   if investigation_history else "")

    if mode == "conflict":
      prompt = (
          f"File Path: {eff_target}\n"
          f"Mode: Merge Conflict Resolution\n\n"
          f"Conflict Block:\n--------------------\n{eff_diag}\n"
          f"--------------------\n\n"
          f"Surrounding File Context:\n{eff_ctx}\n\n"
          f"{traj_section}"
          f"{diff_section}"
          f"{inv_section}"
          "Architectural Guidance Request:\n"
          "1. Analyze what HEAD (our branch) vs incoming (theirs) are trying "
          "to do.\n"
          "2. State whether to take HEAD, take incoming, or combine both.\n"
          "3. Provide step-by-step guidance for Tier-1 Worker to generate the "
          "resolved code.")
    else:
      prompt = (
          f"Build Target: {eff_target}\n"
          f"Mode: {mode}\n\n"
          f"Compiler/Build Diagnostics:\n--------------------\n{eff_diag}\n"
          f"--------------------\n\n"
          f"{all_diags_section}"
          f"{traj_section}"
          f"{diff_section}"
          f"{raw_log_section}"
          f"{inv_section}"
          f"Offending Source Code:\n{eff_ctx}\n\n"
          "Architectural Directive Request:\n"
          "Analyze root cause and provide clear Strategic Guidance & "
          "Directives for Tier-1 Worker (or TOOL_ command if investigation "
          "needed).")

    guidance = self._generate_expert_content(
        prompt, sys_inst, expert_model=chosen_expert)
    return {
        "status": "SUCCESS" if guidance else "ERROR",
        "guidance": (guidance or "").strip(),
        "model_used": chosen_expert,
    }

  def resolve_conflict(
      self,
      file_path: str,
      language: str,
      raw_conflict: str,
      *,
      context_before: str = "",
      context_after: str = "",
      git_context: str = "",
      investigation_history: str = "",
      instruction: str = "",
      expert_guidance: str = "",
      use_expert: bool = False,
      expert_model: Optional[str] = None,
      flash_model: Optional[str] = None,
      **kwargs,
  ) -> Dict[str, Any]:
    """Resolves source/DEPS merge conflicts on Vertex AI."""
    chosen_model = ((expert_model or self.expert_model) if use_expert else
                    (flash_model or self.flash_model))
    rebase_skill = self._get_skill("cobalt_rebase")
    conflict_skill = self._get_skill("conflict_resolution")

    investigation_section = (
        f"--- Investigation Tool Results ---\n{investigation_history}\n\n"
        if investigation_history else "")
    expert_section = (
        f"=== PRE-FLIGHT SENIOR ARCHITECT STRATEGIC GUIDANCE ===\n"
        f"{expert_guidance}\n"
        f"======================================================\n\n"
        if expert_guidance else "")

    sys_inst = (
        f"You are an expert Chromium and Cobalt engineer ({language}).\n\n"
        f"--- General Rebase Guidelines ---\n{rebase_skill}\n\n"
        f"--- Conflict Resolution Skill ---\n{conflict_skill}\n")
    task_inst = instruction or (
        "Task: Resolve this specific merge conflict. Preserve all Cobalt "
        "macros (#if BUILDFLAG(USE_STARBOARD_MEDIA), #if BUILDFLAG(IS_COBALT), "
        "#if defined(STARBOARD)), custom variables (checkout_cobalt_internal, "
        "checkout_copybara), platform shims, and Cobalt runtime behavior "
        "while adopting upstream updates.\nReturn ONLY the exact replacement "
        "code for the block.")

    prompt = (f"Target File: {file_path} ({language})\n"
              f"{expert_section}"
              f"{git_context}\n\n"
              f"Context before conflict:\n{context_before}\n\n"
              f"Conflicted Block to Resolve:\n{raw_conflict}\n\n"
              f"Context after conflict:\n{context_after}\n\n"
              f"{investigation_section}"
              f"{task_inst}")

    if use_expert:
      replacement_text = self._generate_expert_content(prompt, sys_inst)
      return {
          "status": "SUCCESS" if replacement_text else "ERROR",
          "replacement": (replacement_text or "").strip("\n"),
          "model_used": chosen_model,
      }

    resp = self._generate_content_with_retry(
        model=chosen_model,
        contents=prompt,
        config=self._create_generation_config(chosen_model, sys_inst),
    )
    return {
        "status": "SUCCESS" if resp and resp.text else "ERROR",
        "replacement": resp.text.strip("\n") if resp and resp.text else "",
        "model_used": chosen_model,
    }

  def heal_gn_error(
      self,
      error_trace: str,
      file_context: str = "",
      *,
      attempt_history: str = "",
      investigation_history: str = "",
      expert_guidance: str = "",
      use_expert: bool = False,
      expert_model: Optional[str] = None,
      flash_model: Optional[str] = None,
      **kwargs,
  ) -> Dict[str, Any]:
    """Diagnoses and fixes GN generation errors on Vertex AI."""
    chosen_model = ((expert_model or self.expert_model) if use_expert else
                    (flash_model or self.flash_model))
    rebase_skill = self._get_skill("cobalt_rebase")
    gn_skill = self._get_skill("gn_healing")

    investigation_section = (
        f"--- Investigation Tool Results ---\n{investigation_history}\n\n"
        if investigation_history else "")
    expert_section = (
        f"=== PRE-FLIGHT SENIOR ARCHITECT STRATEGIC GUIDANCE ===\n"
        f"{expert_guidance}\n"
        f"======================================================\n\n"
        if expert_guidance else "")

    sys_inst = ("You are an expert Chromium and Cobalt GN build engineer.\n\n"
                f"--- General Rebase Guidelines ---\n{rebase_skill}\n\n"
                f"--- GN Healing Skill ---\n{gn_skill}\n")
    prompt = (
        f"GN Build Error:\n--------------------\n{error_trace}\n"
        "--------------------\n\n"
        f"{expert_section}"
        f"{investigation_section}"
        f"Prior Attempt History:\n{attempt_history}\n\n"
        f"Relevant File Definitions:\n{file_context}\n\n"
        "Instructions:\n"
        "- If you need to inspect files or search paths, output TOOL_ "
        "directives, one per line:\n"
        "TOOL_READ_FILE: <path> <start>-<end>\n"
        "TOOL_GREP: <query> [path]\n"
        "TOOL_FIND_FILE: <pattern>\n"
        "- Otherwise output the final SEARCH / REPLACE block:\n"
        "FILE: <relative_filepath>\n"
        "<<<<<<< SEARCH\n"
        "<exact lines to replace WITHOUT line numbers>\n"
        "=======\n"
        "<fixed replacement lines WITHOUT line numbers>\n"
        ">>>>>>> REPLACE\n\n"
        f"{TOOL_CALL_PROTOCOL}\n"
        "CRITICAL RULES:\n"
        "- DO NOT attach line numbers (e.g. `1060: `) inside SEARCH or "
        "REPLACE blocks. Include only clean code lines.\n"
        "- Code formatting/linting is not required; automated tools handle "
        "formatting post-patch.")

    if use_expert:
      patch_text = self._generate_expert_content(
          prompt, sys_inst, expert_model=chosen_model)
      return {
          "status": "SUCCESS" if patch_text else "ERROR",
          "patch": (patch_text or "").strip(),
          "model_used": chosen_model,
      }

    resp = self._generate_content_with_retry(
        model=chosen_model,
        contents=prompt,
        config=self._create_generation_config(chosen_model, sys_inst),
    )
    return {
        "status": "SUCCESS" if resp and resp.text else "ERROR",
        "patch": resp.text.strip() if resp and resp.text else "",
        "model_used": chosen_model,
    }

  def heal_compiler_error(
      self,
      target: str = "",
      diagnostics: str = "",
      source_contexts: str = "",
      *,
      error_trace: str = "",
      file_context: str = "",
      target_file: str = "",
      history: str = "",
      investigation_history: str = "",
      expert_guidance: str = "",
      use_expert: bool = False,
      expert_model: Optional[str] = None,
      flash_model: Optional[str] = None,
      **kwargs,
  ) -> Dict[str, Any]:
    """Diagnoses and repairs C++/Java compilation errors on Vertex AI."""
    eff_target = target or target_file or "cobalt"
    eff_diag = diagnostics or error_trace
    eff_ctx = source_contexts or file_context

    chosen_model = ((expert_model or self.expert_model) if use_expert else
                    (flash_model or self.flash_model))
    rebase_skill = self._get_skill("cobalt_rebase")
    compiler_skill = self._get_skill("compiler_healing")
    patterns_skill = self._get_skill("cobalt_rebase_patterns")

    history_section = (f"Prior Attempt History:\n{history}\n\n"
                       if history else "")
    investigation_section = (
        f"--- Investigation Tool Results ---\n{investigation_history}\n\n"
        if investigation_history else "")
    expert_section = (
        f"=== TIER-2 ARCHITECT GUIDANCE ({chosen_model.upper()}) ===\n"
        f"{expert_guidance}\n"
        f"============================================================\n\n"
        "CRITICAL: Adhere strictly to the Senior Architect's guidance above "
        "when writing your SEARCH/REPLACE or DELETE block.\n\n"
        if expert_guidance else "")

    is_gn_target = (
        eff_target.endswith((".gn", ".gni")) or target_file.endswith(
            (".gn", ".gni")))
    gn_skill_text = self._get_skill("gn_healing") if is_gn_target else ""
    gn_skill_section = (f"\n\n--- GN Healing Skill ---\n{gn_skill_text}"
                        if is_gn_target else "")

    sys_inst = (
        "You are an expert Chromium and Cobalt systems engineer.\n\n"
        f"--- General Rebase Guidelines ---\n{rebase_skill}\n\n"
        f"--- Compiler Healing Skill ---\n{compiler_skill}\n\n"
        f"--- Cobalt Rebase Patterns & Stubs Hygiene ---\n{patterns_skill}"
        f"{gn_skill_section}\n")
    prompt = (
        f"autoninja build for \"{eff_target}\" failed.\n\n"
        f"{expert_section}"
        f"Compiler Diagnostics:\n--------------------\n{eff_diag}\n"
        "--------------------\n\n"
        f"{history_section}"
        f"{investigation_section}"
        f"Offending Source Code Excerpts:\n{eff_ctx}\n\n"
        "Instructions:\n"
        "- When encountering undeclared identifiers, unknown types, or "
        "incomplete types, DO NOT guess namespaces. Use TOOL_GREP to "
        "find the defining header in Chromium:\n"
        "  TOOL_GREP: <symbol>\n"
        "- If you need to inspect referencing BUILD.gn files or read "
        "headers, output TOOL_ directives, one per line:\n"
        "TOOL_READ_FILE: <path> <start>-<end>\n"
        "TOOL_GREP: <query> [path]\n"
        "TOOL_FIND_FILE: <pattern>\n"
        "- Otherwise output the final SEARCH / REPLACE or DELETE block:\n"
        "  * To replace / add include / modify BUILD.gn:\n"
        "  FILE: <relative_filepath>\n"
        "  <<<<<<< SEARCH\n"
        "  <exact lines to replace WITHOUT line numbers>\n"
        "  =======\n"
        "  <fixed replacement lines WITHOUT line numbers>\n"
        "  >>>>>>> REPLACE\n"
        "  * To delete obsolete code:\n"
        "  FILE: <relative_filepath>\n"
        "  <<<<<<< DELETE\n"
        "  <exact lines to delete WITHOUT line numbers>\n"
        "  >>>>>>> DELETE\n\n"
        f"{TOOL_CALL_PROTOCOL}\n"
        "CRITICAL RULES:\n"
        "- DO NOT attach line numbers (e.g. `1060: `) inside SEARCH or "
        "REPLACE blocks. Include only clean code lines.\n"
        "- Code formatting/linting is not required; automated tools handle "
        "formatting post-patch.")

    if use_expert:
      patch_text = self._generate_expert_content(
          prompt, sys_inst, expert_model=chosen_model)
      return {
          "status": "SUCCESS" if patch_text else "ERROR",
          "patch": (patch_text or "").strip(),
          "model_used": chosen_model,
      }

    resp = self._generate_content_with_retry(
        model=chosen_model,
        contents=prompt,
        config=self._create_generation_config(chosen_model, sys_inst),
    )
    return {
        "status": "SUCCESS" if resp and resp.text else "ERROR",
        "patch": resp.text.strip() if resp and resp.text else "",
        "model_used": chosen_model,
    }
