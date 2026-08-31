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
remote reasoning, prompt construction, memory retrieval, and LLM calls.
Local resolvers communicate with this hosted service via ReasoningEngineClient.
"""

import json
import os
import re
import sys
import time
from typing import Any, Dict, List, Optional
import urllib.error
import urllib.request
import warnings

import google.auth
import google.auth.transport.requests
from google import genai
from google.cloud import storage
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
      flash_model: Optional[str] = None,
      expert_model: Optional[str] = None,
      expert_provider: Optional[str] = None,
      expert_location: Optional[str] = None,
      pro_model: Optional[str] = None,
      skills_dir: Optional[str] = None,
      gcs_memory_uri: Optional[str] = None,
      **kwargs,
  ):
    self.project_id = (
        project_id or os.environ.get("GCP_PROJECT") or
        os.environ.get("GOOGLE_CLOUD_PROJECT"))
    self.location = location
    self.flash_model = flash_model or "gemini-3.7-flash"
    self.expert_model = (
        expert_model or pro_model or os.environ.get("EXPERT_MODEL") or
        "claude-sonnet-5")
    self.pro_model = pro_model or self.expert_model
    self.expert_provider = (
        expert_provider or os.environ.get("EXPERT_PROVIDER") or
        ("anthropic" if "claude" in self.expert_model.lower() else
         ("glm" if "glm" in self.expert_model.lower() else "gemini")))
    self.expert_location = (
        expert_location or os.environ.get("EXPERT_LOCATION") or "global")
    self.glm_api_key = (
        os.environ.get("GLM_API_KEY") or os.environ.get("OPENAI_API_KEY"))
    self.glm_base_url = (
        os.environ.get("GLM_BASE_URL") or os.environ.get("OPENAI_BASE_URL") or
        "https://open.bigmodel.cn/api/paas/v4")
    self.skills_dir = skills_dir or SKILLS_DIR
    self.gcs_memory_uri = (
        gcs_memory_uri or os.environ.get("GCS_MEMORY_URI") or
        "gs://lxn-test/rebase_memory/knowledge_bank.json")
    self.memory_cache: Optional[List[Dict[str, Any]]] = None
    self.storage_client: Any = None
    self.anthropic_client: Any = None
    self.skill_cache: Dict[str, str] = {
        "cobalt_rebase":
            load_skill("cobalt_rebase", self.skills_dir),
        "conflict_resolution":
            load_skill("conflict_resolution", self.skills_dir),
        "gn_healing":
            load_skill("gn_healing", self.skills_dir),
        "compiler_healing":
            load_skill("compiler_healing", self.skills_dir),
        "roll_history":
            load_skill("roll_history", self.skills_dir),
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

  def _get_storage_client(self) -> Any:
    """Lazy loads Google Cloud Storage client in cloud or local environment."""
    if self.storage_client is None:
      try:
        self.storage_client = storage.Client(project=self.project_id)
      except Exception:  # pylint: disable=broad-exception-caught
        self.storage_client = None
    return self.storage_client

  def _load_memory(self) -> List[Dict[str, Any]]:
    """Loads knowledge memory from GCS or returns cached memory."""
    if self.memory_cache is not None:
      return self.memory_cache
    self.memory_cache = []
    if not self.gcs_memory_uri or not self.gcs_memory_uri.startswith("gs://"):
      return self.memory_cache

    client = self._get_storage_client()
    if client is None:
      return self.memory_cache

    try:
      path_part = self.gcs_memory_uri[5:]
      if "/" not in path_part:
        return self.memory_cache
      bucket_name, blob_name = path_part.split("/", 1)
      bucket = client.bucket(bucket_name)
      blob = bucket.blob(blob_name)
      if blob.exists():
        data = blob.download_as_text(encoding="utf-8")
        loaded = json.loads(data)
        if isinstance(loaded, list):
          self.memory_cache = loaded
    except Exception as e:  # pylint: disable=broad-exception-caught
      print(
          f"  [REASONING_ENGINE] Notice: Could not read memory from GCS: {e}",
          file=sys.stderr,
      )
    return self.memory_cache

  def get_past_experience(
      self,
      query: str,
      max_items: int = 3,
      **kwargs,
  ) -> str:
    """Retrieves relevant past fixes from knowledge bank."""
    memory = self._load_memory()
    if not memory:
      return ""

    q_lower = query.lower()
    q_words = set(re.findall(r"\w+", q_lower))
    scored = []
    for item in memory:
      desc = item.get("issue_description", "").lower()
      target = item.get("target_file", "").lower()
      combined = f"{desc} {target}"
      score = sum(1 for w in q_words if w in combined)
      if score > 0:
        scored.append((score, item))

    scored.sort(key=lambda x: x[0], reverse=True)
    top_items = [item for _, item in scored[:max_items]]
    if not top_items:
      return ""

    blocks = []
    for idx, item in enumerate(top_items, 1):
      desc = item.get("issue_description", "")
      sol = item.get("solution_diff", "")
      tf = item.get("target_file", "")
      target_line = f"Target File: {tf}\n" if tf else ""
      blocks.append(f"Example #{idx}:\n{target_line}Issue: {desc}\nFix:\n{sol}")
    return "\n\n".join(blocks)

  def record_successful_fix(
      self,
      issue_description: str,
      solution_diff: str,
      target_file: str = "",
      **kwargs,
  ) -> bool:
    """Records a verified fix into GCS knowledge memory bank."""
    memory = self._load_memory()
    for item in memory:
      if item.get("solution_diff") == solution_diff:
        return True

    record = {
        "timestamp": time.time(),
        "target_file": target_file,
        "issue_description": issue_description,
        "solution_diff": solution_diff,
    }
    memory.append(record)
    self.memory_cache = memory

    if self.gcs_memory_uri and self.gcs_memory_uri.startswith("gs://"):
      client = self._get_storage_client()
      if client:
        try:
          path_part = self.gcs_memory_uri[5:]
          if "/" in path_part:
            bucket_name, blob_name = path_part.split("/", 1)
            bucket = client.bucket(bucket_name)
            blob = bucket.blob(blob_name)
            blob.upload_from_string(
                json.dumps(memory, indent=2),
                content_type="application/json",
            )
            return True
        except Exception as e:  # pylint: disable=broad-exception-caught
          print(
              "  [REASONING_ENGINE] Warning: Could not write memory to GCS: "
              f"{e}",
              file=sys.stderr,
          )
    return True

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

  def _generate_openai_compatible_content(
      self,
      model: str,
      contents: Any,
      system_instruction: str,
      temperature: float = 0.1,
  ) -> Optional[str]:
    """Generates content via Vertex AI Model Garden GLM-5.2 MaaS."""
    headers = {"Content-Type": "application/json"}
    eff_model = model

    # 1. If using Vertex AI Model Garden GLM MaaS (zai-org/glm-5.2-maas)
    if "zai-org" in model.lower() or "glm" in model.lower():
      try:
        credentials, _ = google.auth.default(
            scopes=["https://www.googleapis.com/auth/cloud-platform"])
        auth_req = google.auth.transport.requests.Request()
        credentials.refresh(auth_req)
        token = credentials.token
        headers["Authorization"] = f"Bearer {token}"
      except Exception as e:  # pylint: disable=broad-exception-caught
        print(
            "  [REASONING_ENGINE] Notice: Could not acquire Cloud token "
            f"for GLM MaaS: {e}",
            file=sys.stderr,
        )
        if self.glm_api_key:
          headers["Authorization"] = f"Bearer {self.glm_api_key}"

      reg = (
          self.expert_location
          if self.expert_location not in ("us-east5", "") else "global")
      endpoint = (
          os.environ.get("GLM_ENDPOINT") or
          ("https://aiplatform.googleapis.com/v1/projects/"
           f"{self.project_id}/locations/{reg}/"
           "endpoints/openapi/chat/completions"))
      eff_model = "zai-org/glm-5.2-maas"
    else:
      if not self.glm_api_key:
        print(
            "  [REASONING_ENGINE] Notice: GLM_API_KEY/OPENAI_API_KEY not set.",
            file=sys.stderr,
        )
        return None
      headers["Authorization"] = f"Bearer {self.glm_api_key}"
      endpoint = f'{self.glm_base_url.rstrip("/")}/chat/completions'

    prompt_text = str(contents) if not isinstance(contents, str) else contents
    payload = {
        "model": eff_model,
        "messages": [
            {
                "role": "system",
                "content": system_instruction
            },
            {
                "role": "user",
                "content": prompt_text
            },
        ],
        "temperature": temperature,
        "max_tokens": 8192,
    }
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        endpoint,
        data=data,
        headers=headers,
        method="POST",
    )
    try:
      print(
          f"  [REASONING_ENGINE] [EXPERT_TIER] Dispatching to GLM/OpenAI "
          f"{eff_model} via {endpoint}...",
          file=sys.stderr,
      )
      with urllib.request.urlopen(req, timeout=120) as resp:
        res_data = json.loads(resp.read().decode("utf-8"))
        if "choices" in res_data and res_data["choices"]:
          msg = res_data["choices"][0].get("message", {})
          text = msg.get("content") or msg.get("reasoning_content") or ""
          if text:
            cleaned = re.sub(
                r"<think>.*?</think>", "", text, flags=re.DOTALL).strip()
            if not cleaned and ("<think>" in text or "</think>" in text):
              cleaned = re.sub(r"</?think>", "", text).strip()
            return cleaned or text
    except Exception as e:  # pylint: disable=broad-exception-caught
      print(
          f"  [REASONING_ENGINE] Warning: GLM/OpenAI query failed: {e}. "
          "Falling back to Gemini Expert...",
          file=sys.stderr,
      )
    return None

  def _generate_expert_content(
      self,
      contents: Any,
      system_instruction: str,
      expert_model: Optional[str] = None,
      temperature: float = 0.1,
  ) -> Optional[str]:
    """Generates content via Tier-2 Expert LLM (Sonnet 5, GLM 5.2)."""
    expert_name = expert_model or self.expert_model or "claude-sonnet-5"
    provider = ("anthropic" if "claude" in expert_name.lower() else
                ("glm" if "glm" in expert_name.lower() else "gemini"))

    # 1. Anthropic Claude (e.g. Claude Sonnet 5 on Vertex AI)
    if provider == "anthropic" or "claude" in expert_name.lower():
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
            text_parts = []
            for block in resp.content:
              if hasattr(block, "text") and block.text:
                text_parts.append(block.text)
              elif getattr(block, "type", "") == "text":
                text_parts.append(getattr(block, "text", ""))
            full_text = "\n".join(text_parts).strip()
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

    # 2. GLM / OpenAI Compatible Service (e.g. GLM-5.2)
    if self.expert_provider == "glm" or "glm" in expert_name.lower():
      glm_resp = self._generate_openai_compatible_content(
          model=expert_name,
          contents=contents,
          system_instruction=system_instruction,
          temperature=temperature,
      )
      if glm_resp:
        return glm_resp

    # 3. Gemini Thinking Expert (e.g. gemini-3.7-flash)
    target_gemini_model = (
        expert_name if "gemini" in expert_name.lower() else
        (self.flash_model
         if "gemini" in self.flash_model.lower() else "gemini-3.7-flash"))
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
    )
    return resp.text.strip() if resp and resp.text else ""

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
  ) -> Optional[types.GenerateContentResponse]:
    """Calls generate_content with exponential backoff for 429/503 errors."""
    client = self._get_client()
    backoff = initial_backoff
    print(
        f"  [REASONING_ENGINE] [WORKHORSE_TIER] Dispatching to {model} "
        "(thinking tokens enabled)...",
        file=sys.stderr,
    )
    for attempt in range(1, max_retries + 1):
      try:
        return client.models.generate_content(
            model=model,
            contents=contents,
            config=config,
        )
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

  def generate_content(
      self,
      contents: Any,
      system_instruction: str = "",
      *,
      model: Optional[str] = None,
      temperature: float = 0.1,
  ) -> Optional[types.GenerateContentResponse]:
    """Generates content via Vertex AI with automatic rate-limit retries."""
    chosen_model = model or self.flash_model
    config = self._create_generation_config(
        chosen_model, system_instruction, temperature=temperature)
    return self._generate_content_with_retry(
        model=chosen_model,
        contents=contents,
        config=config,
    )

  def query(self, action: str = "resolve_conflict", **kwargs) -> Dict[str, Any]:
    """Primary query dispatcher for Vertex AI Reasoning Engine."""
    if action in ("generate_expert_guidance", "expert_guidance"):
      return self.generate_expert_guidance(**kwargs)
    if action in ("generate_comparative_review", "comparative_review"):
      return {"review": self.generate_comparative_review(**kwargs)}
    if action == "resolve_conflict":
      return self.resolve_conflict(**kwargs)
    if action in ("heal_gn", "heal_gn_error"):
      return self.heal_gn_error(**kwargs)
    if action in ("heal_compiler", "heal_compiler_break",
                  "heal_compiler_error"):
      return self.heal_compiler_error(**kwargs)
    if action in ("record_successful_fix", "record_fix"):
      return {"success": self.record_successful_fix(**kwargs)}
    if action in ("get_past_experience", "load_past_experience"):
      return {"experience": self.get_past_experience(**kwargs)}
    if action == "chat":
      return self.chat(**kwargs)
    raise ValueError(f"Unknown Reasoning Engine action: {action}")

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
      mode: str = "compiler",
      expert_model: Optional[str] = None,
      **kwargs,
  ) -> Dict[str, Any]:
    """Tier-2 Senior Architect (Claude Sonnet 5 / Gemini / GLM).

    Analyzes full failure trajectory, session git diffs, and diagnostics
    to produce strategic root-cause guidance and actionable refactoring
    directives for Tier-1 (Worker Agent).
    """
    eff_target = target or target_file or "cobalt"
    eff_diag = diagnostics or error_trace
    eff_ctx = source_contexts or file_context
    chosen_expert = expert_model or self.expert_model or "claude-sonnet-5"

    rebase_skill = self._get_skill("cobalt_rebase")
    domain_skill = self._get_skill(
        "conflict_resolution" if mode == "conflict" else (
            "gn_healing" if mode == "gn" else "compiler_healing"))
    roll_history_skill = self._get_skill("roll_history")

    sys_inst = (
        "You are the Senior Principal Chromium and Cobalt Systems Architect.\n"
        "Your role is NOT to write syntax diffs, but to strategically diagnose "
        "conflicts, complex build failures, V8/Blink/Starboard refactorings, "
        "and multi-iteration loops.\n\n"
        "You will receive:\n"
        "1. Trajectory of what Tier-1 Worker attempted so far.\n"
        "2. Git diff of modifications made in working tree this session.\n"
        "3. Current conflict block or compiler/GN diagnostic and source "
        "context.\n\n"
        "Responsibilities:\n"
        "- Perform deep root-cause analysis: identify what upstream changed.\n"
        "- Upstream Roll Commits: Commit #3 contains pure Chromium changes. "
        "Use TOOL_UPSTREAM_DIFF to inspect upstream Chromium evolutions.\n"
        "- If you need to inspect files, symbols, or git history, output:\n"
        "    TOOL_FIND_FILE: <pattern>\n"
        "    TOOL_GREP: <symbol>\n"
        "    TOOL_READ_FILE: <path> <start>-<end>\n"
        "    TOOL_UPSTREAM_DIFF: <filepath>\n"
        "    TOOL_GIT_LOG: <count> <filepath>\n"
        "    TOOL_GIT_SHOW: <commit_hash>\n"
        "    TOOL_GIT_DIFF: <rev1>..<rev2>\n"
        "    TOOL_READ_PR: <pr_number>\n"
        "    TOOL_PR_DIFF: <pr_number>\n"
        "- Explain why Tier 1 is stuck and list anti-patterns to avoid.\n"
        "- Provide clear, concrete, step-by-step instructions for Tier 1:\n"
        "  * For conflicts: state whether to choose HEAD, incoming (theirs), "
        "or combine both.\n"
        "  * For build breaks: first-party file(s) to edit, precise logic, "
        "type substitution, or header include to apply.\n\n"
        f"--- Rebase Guidelines ---\n{rebase_skill}\n\n"
        f"--- Domain Skill ---\n{domain_skill}\n\n"
        f"--- Historical Ground-Truth Roll References (M139-M141) ---\n"
        f"{roll_history_skill}\n")

    diff_section = (f"--- Git Diff of Modifications in Current Session ---\n"
                    f"{working_diff[:16384]}\n\n" if working_diff else "")
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
          f"{traj_section}"
          f"{diff_section}"
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
      past_experience: str = "",
      investigation_history: str = "",
      instruction: str = "",
      expert_guidance: str = "",
      use_expert: bool = False,
      expert_model: Optional[str] = None,
      **kwargs,
  ) -> Dict[str, Any]:
    """Resolves source/DEPS merge conflicts on Vertex AI."""
    chosen_model = (expert_model or
                    self.expert_model) if use_expert else self.flash_model
    rebase_skill = self._get_skill("cobalt_rebase")
    conflict_skill = self._get_skill("conflict_resolution")

    effective_past = (
        past_experience or
        self.get_past_experience(f"{file_path} {raw_conflict}"))
    past_lessons_section = (
        f"--- Past Successful Lessons ---\n{effective_past}\n\n"
        if effective_past else "")
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
              f"{past_lessons_section}"
              f"Context before conflict:\n{context_before}\n\n"
              f"Conflicted Block to Resolve:\n{raw_conflict}\n\n"
              f"Context after conflict:\n{context_after}\n\n"
              f"{investigation_section}"
              f"{task_inst}")

    if use_expert:
      replacement_text = self._generate_expert_content(prompt, sys_inst)
      return {
          "status": "SUCCESS" if replacement_text else "ERROR",
          "replacement": (replacement_text or "").strip(),
          "model_used": chosen_model,
      }

    resp = self._generate_content_with_retry(
        model=chosen_model,
        contents=prompt,
        config=self._create_generation_config(chosen_model, sys_inst),
    )
    return {
        "status": "SUCCESS" if resp and resp.text else "ERROR",
        "replacement": resp.text.strip() if resp and resp.text else "",
        "model_used": chosen_model,
    }

  def heal_gn_error(
      self,
      error_trace: str,
      file_context: str = "",
      *,
      attempt_history: str = "",
      past_experience: str = "",
      investigation_history: str = "",
      expert_guidance: str = "",
      use_expert: bool = False,
      expert_model: Optional[str] = None,
      **kwargs,
  ) -> Dict[str, Any]:
    """Diagnoses and fixes GN generation errors on Vertex AI."""
    chosen_model = (expert_model or
                    self.expert_model) if use_expert else self.flash_model
    rebase_skill = self._get_skill("cobalt_rebase")
    gn_skill = self._get_skill("gn_healing")

    effective_past = (
        past_experience or self.get_past_experience(f"gn {error_trace}"))
    past_lessons_section = (f"Past Successful Lessons:\n{effective_past}\n\n"
                            if effective_past else "")
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
        f"{past_lessons_section}"
        f"Prior Attempt History:\n{attempt_history}\n\n"
        f"Relevant File Definitions:\n{file_context}\n\n"
        "Instructions:\n"
        "- If you need to inspect files or search paths, output TOOL_ "
        "commands (e.g. `TOOL_READ_FILE: <path> <start>-<end>`, "
        "`TOOL_GREP: <query> [path]`, `TOOL_FIND_FILE: <pattern>`).\n"
        "- Otherwise output the final SEARCH / REPLACE block:\n"
        "FILE: <relative_filepath>\n"
        "<<<<<<< SEARCH\n"
        "<exact lines to replace WITHOUT line numbers>\n"
        "=======\n"
        "<fixed replacement lines WITHOUT line numbers>\n"
        ">>>>>>> REPLACE\n\n"
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
      past_experience: str = "",
      investigation_history: str = "",
      expert_guidance: str = "",
      use_expert: bool = False,
      expert_model: Optional[str] = None,
      **kwargs,
  ) -> Dict[str, Any]:
    """Diagnoses and repairs C++/Java compilation errors on Vertex AI."""
    eff_target = target or target_file or "cobalt"
    eff_diag = diagnostics or error_trace
    eff_ctx = source_contexts or file_context
    eff_inv = investigation_history or history

    chosen_model = (expert_model or
                    self.expert_model) if use_expert else self.flash_model
    rebase_skill = self._get_skill("cobalt_rebase")
    compiler_skill = self._get_skill("compiler_healing")

    effective_past = (
        past_experience or self.get_past_experience(f"{eff_target} {eff_diag}"))
    past_lessons_section = (
        f"Past Successful Rebase Lessons:\n{effective_past}\n\n"
        if effective_past else "")
    investigation_section = (
        f"--- Investigation Tool Results ---\n{eff_inv}\n\n" if eff_inv else "")
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

    sys_inst = ("You are an expert Chromium and Cobalt systems engineer.\n\n"
                f"--- General Rebase Guidelines ---\n{rebase_skill}\n\n"
                f"--- Compiler Healing Skill ---\n{compiler_skill}"
                f"{gn_skill_section}\n")
    prompt = (
        f"autoninja build for \"{eff_target}\" failed.\n\n"
        f"{expert_section}"
        f"Compiler Diagnostics:\n--------------------\n{eff_diag}\n"
        "--------------------\n\n"
        f"{investigation_section}"
        f"{past_lessons_section}"
        f"Offending Source Code Excerpts:\n{eff_ctx}\n\n"
        "Instructions:\n"
        "- When encountering undeclared identifiers, unknown types, or "
        "incomplete types, DO NOT guess namespaces. Use TOOL_GREP to "
        "find the defining header in Chromium:\n"
        "  TOOL_GREP: <symbol>\n"
        "- If you need to inspect referencing BUILD.gn files or read "
        "headers, output TOOL_ commands (e.g. `TOOL_READ_FILE: <path> "
        "<start>-<end>`, `TOOL_GREP: <query>`, `TOOL_FIND_FILE: <pattern>`).\n"
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

  def chat(
      self,
      message: str,
      history: Optional[List[Dict[str, str]]] = None,
      *,
      mode: str = "rebase",
      use_pro: bool = False,
      failure_memory: str = "",
      expert_model: Optional[str] = None,
      **kwargs,
  ) -> Dict[str, Any]:
    """Interactive conversational interface to Reasoning Engine."""
    chosen_model = (
        expert_model or (self.pro_model if use_pro else self.flash_model))
    skill_map = {
        "rebase": "cobalt_rebase",
        "gn": "gn_healing",
        "compiler": "compiler_healing",
        "conflict": "conflict_resolution",
    }
    skill_key = skill_map.get(mode, "cobalt_rebase")
    skill_doc = self._get_skill(skill_key)
    rebase_doc = self._get_skill("cobalt_rebase")

    failure_section = (
        f"\n--- Latest Pipeline Failure State ---\n{failure_memory}\n"
        if failure_memory and "No recent" not in failure_memory else "")

    sys_inst = (
        f"You are the Cobalt Chromium Rebase Reasoning Engine ({mode} mode).\n"
        "You assist engineers in analyzing rebase conflicts, GN build breaks, "
        "toolchain issues, and compiler failures.\n\n"
        f"--- Base Instructions ---\n{rebase_doc}\n\n"
        f"--- Active Domain Skill ({mode}) ---\n{skill_doc}\n"
        f"{failure_section}")

    contents: List[Any] = []
    if history:
      for turn in history:
        contents.append(
            types.Content(
                role=turn.get("role", "user"),
                parts=[types.Part.from_text(text=turn.get("content", ""))],
            ))
    contents.append(
        types.Content(
            role="user",
            parts=[types.Part.from_text(text=message)],
        ))

    resp = self._generate_content_with_retry(
        model=chosen_model,
        contents=contents,
        config=self._create_generation_config(
            chosen_model, sys_inst, temperature=0.2),
    )
    return {
        "status": "SUCCESS" if resp and resp.text else "ERROR",
        "response": resp.text.strip() if resp and resp.text else "",
        "model_used": chosen_model,
    }
