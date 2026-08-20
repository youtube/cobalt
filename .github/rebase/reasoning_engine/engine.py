#!/usr/bin/env python3
# pylint: disable=duplicate-code
"""Vertex AI Reasoning Engine Agent for Cobalt Chromium Rebase.

Provides unified conflict resolution, GN configuration healing, and
compiler self-healing via Google Cloud Vertex AI using google.genai SDK.
All domain instructions are dynamically loaded from markdown skill files
under the skills/ directory.
"""

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


class CobaltReasoningEngine:
  """Unified Vertex AI Reasoning Engine interface for Cobalt rebase."""

  def __init__(
      self,
      *,
      resource_id: Optional[str] = None,
      project_id: Optional[str] = None,
      location: str = "global",
      flash_model: Optional[str] = None,
      pro_model: str = "gemini-2.5-pro",
      skills_dir: Optional[str] = None,
  ):
    self.resource_id = (
        resource_id or os.environ.get("REASONING_ENGINE_ID") or
        os.environ.get("REASONING_ENGINE_RESOURCE_ID"))
    self.project_id = (
        project_id or os.environ.get("GCP_PROJECT") or
        os.environ.get("GOOGLE_CLOUD_PROJECT"))
    self.location = location
    self.flash_model = flash_model or "gemini-3.7-flash"
    self.pro_model = pro_model
    self.skills_dir = skills_dir or SKILLS_DIR
    self.skill_cache: Dict[str, str] = {
        "cobalt_rebase":
            load_skill("cobalt_rebase", self.skills_dir),
        "conflict_resolution":
            load_skill("conflict_resolution", self.skills_dir),
        "gn_healing":
            load_skill("gn_healing", self.skills_dir),
        "compiler_healing":
            load_skill("compiler_healing", self.skills_dir),
    }
    self.client: Optional[genai.Client] = None
    self.remote_engine: Any = None

  def set_up(self):
    """Initializes Google GenAI client in remote Vertex AI container."""
    self.client = genai.Client(
        vertexai=True,
        project=self.project_id,
        location=self.location,
    )

  def _get_remote_engine(self) -> Any:
    """Initializes and returns remote Vertex AI Reasoning Engine proxy."""
    if self.remote_engine is None and self.resource_id:
      try:
        import vertexai  # pylint: disable=import-outside-toplevel
        from vertexai.preview import reasoning_engines  # pylint: disable=import-outside-toplevel
        vertexai.init(project=self.project_id, location=self.location)
        res_name = self.resource_id
        if not res_name.startswith("projects/"):
          res_name = (f"projects/{self.project_id}/locations/{self.location}/"
                      f"reasoningEngines/{self.resource_id}")
        print(
            "  [REASONING_ENGINE] Connecting to hosted Reasoning Engine: "
            f"{res_name}",
            file=sys.stderr,
        )
        self.remote_engine = reasoning_engines.ReasoningEngine(res_name)
      except Exception as e:  # pylint: disable=broad-exception-caught
        print(
            "  [REASONING_ENGINE] Failed to connect to remote Reasoning "
            f"Engine: {e}. Falling back to direct Vertex AI calls.",
            file=sys.stderr,
        )
        self.remote_engine = None
    return self.remote_engine

  def _get_client(self) -> genai.Client:
    """Returns active client instance, initializing if needed."""
    if self.client is None:
      self.set_up()
    return self.client

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
    for attempt in range(1, max_retries + 1):
      try:
        return client.models.generate_content(
            model=model,
            contents=contents,
            config=config,
        )
      except Exception as e:  # pylint: disable=broad-exception-caught
        err_str = str(e)
        if any(term in err_str
               for term in ["429", "RESOURCE_EXHAUSTED", "503", "UNAVAILABLE"]):
          print(
              f"  [REASONING_ENGINE] Vertex AI rate limit (attempt {attempt}/"
              f"{max_retries}). Backing off for {backoff:.1f}s...",
              file=sys.stderr,
          )
          time.sleep(backoff)
          backoff *= 2.0
        else:
          print(f"  [REASONING_ENGINE] Vertex AI error: {e}", file=sys.stderr)
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
    if action == "resolve_conflict":
      return self.resolve_conflict(**kwargs)
    if action in ("heal_gn", "heal_gn_error"):
      return self.heal_gn_error(**kwargs)
    if action in ("heal_compiler", "heal_compiler_break",
                  "heal_compiler_error"):
      return self.heal_compiler_error(**kwargs)
    if action == "chat":
      return self.chat(**kwargs)
    raise ValueError(f"Unknown Reasoning Engine action: {action}")

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
      use_pro: bool = False,
  ) -> Dict[str, Any]:
    """Resolves source/DEPS merge conflicts on Vertex AI."""
    remote = self._get_remote_engine()
    if remote is not None:
      try:
        return remote.query(
            action="resolve_conflict",
            file_path=file_path,
            language=language,
            raw_conflict=raw_conflict,
            context_before=context_before,
            context_after=context_after,
            git_context=git_context,
            past_experience=past_experience,
            investigation_history=investigation_history,
            instruction=instruction,
            use_pro=use_pro,
        )
      except Exception as e:  # pylint: disable=broad-exception-caught
        print(
            f"  [REASONING_ENGINE] Remote Reasoning Engine query failed: {e}. "
            "Falling back to direct Vertex AI call.",
            file=sys.stderr,
        )

    chosen_model = self.pro_model if use_pro else self.flash_model
    rebase_skill = self._get_skill("cobalt_rebase")
    conflict_skill = self._get_skill("conflict_resolution")

    past_lessons_section = (
        f"--- Past Successful Lessons ---\n{past_experience}\n\n"
        if past_experience else "")
    investigation_section = (
        f"--- Investigation Tool Results ---\n{investigation_history}\n\n"
        if investigation_history else "")

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
              f"{git_context}\n\n"
              f"{past_lessons_section}"
              f"Context before conflict:\n{context_before}\n\n"
              f"Conflicted Block to Resolve:\n{raw_conflict}\n\n"
              f"Context after conflict:\n{context_after}\n\n"
              f"{investigation_section}"
              f"{task_inst}")
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

  def heal_gn_error(
      self,
      error_trace: str,
      file_context: str = "",
      *,
      attempt_history: str = "",
      past_experience: str = "",
      investigation_history: str = "",
      use_pro: bool = False,
  ) -> Dict[str, Any]:
    """Diagnoses and fixes GN generation errors on Vertex AI."""
    remote = self._get_remote_engine()
    if remote is not None:
      try:
        return remote.query(
            action="heal_gn_error",
            error_trace=error_trace,
            file_context=file_context,
            attempt_history=attempt_history,
            past_experience=past_experience,
            investigation_history=investigation_history,
            use_pro=use_pro,
        )
      except Exception as e:  # pylint: disable=broad-exception-caught
        print(
            f"  [REASONING_ENGINE] Remote Reasoning Engine query failed: {e}. "
            "Falling back to direct Vertex AI call.",
            file=sys.stderr,
        )

    chosen_model = self.pro_model if use_pro else self.flash_model
    rebase_skill = self._get_skill("cobalt_rebase")
    gn_skill = self._get_skill("gn_healing")

    investigation_section = (
        f"--- Investigation Tool Results ---\n{investigation_history}\n\n"
        if investigation_history else "")

    sys_inst = ("You are an expert Chromium and Cobalt GN build engineer.\n\n"
                f"--- General Rebase Guidelines ---\n{rebase_skill}\n\n"
                f"--- GN Healing Skill ---\n{gn_skill}\n")
    prompt = (f"GN Build Error:\n--------------------\n{error_trace}\n"
              "--------------------\n\n"
              f"{investigation_section}"
              f"Past Successful Lessons:\n{past_experience}\n\n"
              f"Prior Attempt History:\n{attempt_history}\n\n"
              f"Relevant File Definitions:\n{file_context}\n\n"
              "Instructions:\n"
              "- If you need to inspect files or search paths, output a single "
              "TOOL_ command (e.g. `TOOL_READ_FILE: <path> <start>-<end>` or "
              "`TOOL_FIND_FILE: <pattern>`).\n"
              "- Otherwise output the final SEARCH / REPLACE block:\n"
              "FILE: <relative_filepath>\n"
              "<<<<<<< SEARCH\n"
              "<exact lines to replace>\n"
              "=======\n"
              "<fixed replacement lines>\n"
              ">>>>>>> REPLACE")
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
      target: str,
      diagnostics: str,
      source_contexts: str = "",
      *,
      past_experience: str = "",
      investigation_history: str = "",
      use_pro: bool = False,
  ) -> Dict[str, Any]:
    """Diagnoses and repairs C++/Java compilation errors on Vertex AI."""
    remote = self._get_remote_engine()
    if remote is not None:
      try:
        return remote.query(
            action="heal_compiler_error",
            target=target,
            diagnostics=diagnostics,
            source_contexts=source_contexts,
            past_experience=past_experience,
            investigation_history=investigation_history,
            use_pro=use_pro,
        )
      except Exception as e:  # pylint: disable=broad-exception-caught
        print(
            f"  [REASONING_ENGINE] Remote Reasoning Engine query failed: {e}. "
            "Falling back to direct Vertex AI call.",
            file=sys.stderr,
        )
    chosen_model = self.pro_model if use_pro else self.flash_model
    rebase_skill = self._get_skill("cobalt_rebase")
    compiler_skill = self._get_skill("compiler_healing")

    investigation_section = (
        f"--- Investigation Tool Results ---\n{investigation_history}\n\n"
        if investigation_history else "")

    sys_inst = (
        "You are an expert Chromium and Cobalt C++/Java compiler engineer.\n\n"
        f"--- General Rebase Guidelines ---\n{rebase_skill}\n\n"
        f"--- Compiler Healing Skill ---\n{compiler_skill}\n")
    prompt = (f"autoninja build for \"{target}\" failed.\n\n"
              f"Compiler Diagnostics:\n--------------------\n{diagnostics}\n"
              "--------------------\n\n"
              f"{investigation_section}"
              f"Past Successful Rebase Lessons:\n{past_experience}\n\n"
              f"Offending Source Code Excerpts:\n{source_contexts}\n\n"
              "Instructions:\n"
              "- If you need to inspect referencing BUILD.gn files, headers, "
              "or search for relocated APIs, output a single TOOL_ command "
              "(e.g. `TOOL_READ_FILE: <path> <start>-<end>` or "
              "`TOOL_GREP: <symbol>`).\n"
              "- Otherwise output the final SEARCH / REPLACE block:\n"
              "FILE: <relative_filepath>\n"
              "<<<<<<< SEARCH\n"
              "<exact lines to replace>\n"
              "=======\n"
              "<fixed replacement lines>\n"
              ">>>>>>> REPLACE")
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
  ) -> Dict[str, Any]:
    """Interactive conversational interface to Reasoning Engine."""
    chosen_model = self.pro_model if use_pro else self.flash_model
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
