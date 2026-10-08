"""Investigation tools the model can call while fixing a failure.

Parses TOOL_* directives out of model responses and dispatches each one to a
read-only handler: file reads, git grep / log / diff / show, gh PR lookups,
roll-commit diffs and the current session's change history.
"""

import dataclasses
import os
import re
import subprocess
from typing import TYPE_CHECKING, Callable, Dict, List, Optional

from repo_guards import get_clean_build_env, resolve_repo_file_path

if TYPE_CHECKING:
  from base_resolver import AgentChangeRecord

# Precompiled pattern matching investigation tool directives (e.g.,
# TOOL_READ_FILE: ...).
#
# Models occasionally emit several directives run together on a single line
# without separators (e.g. "TOOL_GREP: fooTOOL_READ_FILE: bar 1 20"), or glue a
# trailing "FILE:" patch header onto the last directive. The negative lookahead
# terminates each match at the next directive or patch header so malformed
# batches still parse into individual commands.
_TOOL_CMD_TERMINATORS = r"TOOL_[A-Z_]+:|FILE:|TARGET FILE:|<<<<<<<|>>>>>>>"
# No leading word-boundary anchor: in run-on responses a directive begins
# immediately after an alphanumeric character (e.g. "...mojomTOOL_READ_FILE:"),
# where \b does not hold and every directive after the first would be lost.
_TOOL_CMD_PATTERN = re.compile(r"(TOOL_[A-Z_]+:\s*"
                               r"(?:(?!" + _TOOL_CMD_TERMINATORS + r")"
                               r"[^\n<`])+)")

# Upper bound on investigation directives honored from a single model response.
# Prevents a speculative dump of a dozen commands from stalling the loop.
_MAX_TOOL_CMDS_PER_TURN = 3


def sanitize_filepath_token(raw_target: str) -> str:
  """Extracts a clean, valid repository file path token from model output.

  Strips surrounding backticks, quotes, parenthetical/inline commentary,
  and trailing punctuation.
  """
  clean = raw_target.strip().strip("`'\"[]()<>")
  tokens = re.split(r"[\s\(\[\#]+", clean)
  if tokens and tokens[0]:
    token = tokens[0].strip("`'\"[]()<>,;:")
    return token.lstrip("./")
  return clean.lstrip("./")


def extract_tool_commands(text: str,
                          max_commands: int = _MAX_TOOL_CMDS_PER_TURN
                         ) -> List[str]:
  """Extracts TOOL_ commands, stripping think tags/backticks/preambles.

  Tolerates malformed batches: directives may be wrapped in backticks, prefixed
  with markdown bullets or "Tool Call:", or run together on a single line with
  no separators. At most `max_commands` directives are honored per response so
  a speculative dump of a dozen commands cannot stall the investigation loop.
  """
  clean = re.sub(r"<think>.*?</think>", "", text, flags=re.DOTALL)
  clean = re.sub(r"</?think>.*$", "", clean, flags=re.MULTILINE)
  clean = re.sub(r"</?think>", "", clean)

  # If model has already provided a full SEARCH/REPLACE block or unified diff,
  # do not treat it as an investigation tool command.
  if (("<<<<<<< SEARCH" in clean and ">>>>>>> REPLACE" in clean) or
      ("<<<<<<< DELETE" in clean and ">>>>>>> DELETE" in clean) or
      re.search(r"^@@\s+-\d+.*?\s+\+\d+.*?@@", clean, re.MULTILINE)):
    return []

  # Drop markdown bullets / "Tool Call:" preambles so directives start cleanly.
  clean = re.sub(
      r"^(?:[-*]\s+)?(?:Tool Call:\s*|Tool:\s*)",
      "",
      clean,
      flags=re.IGNORECASE | re.MULTILINE,
  )

  commands: List[str] = []
  seen = set()
  # Scan the whole response rather than one match per line: models sometimes
  # concatenate directives without newlines.
  for match in _TOOL_CMD_PATTERN.finditer(clean):
    cmd = re.sub(r"[`'\"]+$", "", match.group(1)).strip()
    cmd = cmd.strip("`'\"").strip()
    if not cmd or cmd in seen:
      continue
    seen.add(cmd)
    commands.append(cmd)
    if len(commands) >= max_commands:
      break
  return commands


# Directives that occupy a line of their own, ignoring indentation.
_TOOL_CMD_LINE_PATTERN = re.compile(r"^[ \t]*(TOOL_[A-Z_]+:[^\n]*)$",
                                    re.MULTILINE)


def extract_line_anchored_tool_commands(
    text: str,
    max_commands: int = _MAX_TOOL_CMDS_PER_TURN,
) -> List[str]:
  """Extracts TOOL_ directives that occupy a line of their own.

  Use this when the response payload is source code; use
  extract_tool_commands when it is prose.

  extract_tool_commands scans anywhere in the text, which is required to
  split run-on directives in prose but misfires on code: an indented
  C++ 'case TOOL_TIP:' parses as a directive named TOOL_TIP. Requiring
  the directive to own its line rejects those while still accepting the
  indented requests that a strictly column-0 anchor would miss.
  """
  commands: List[str] = []
  seen = set()
  for match in _TOOL_CMD_LINE_PATTERN.finditer(text or ""):
    cmd = match.group(1).strip().strip("`'\"").strip()
    if not cmd or cmd in seen:
      continue
    seen.add(cmd)
    commands.append(cmd)
    if len(commands) >= max_commands:
      break
  return commands


def _run_in_repo(
    cmd: List[str],
    repo_path: str,
    env: Optional[Dict[str, str]] = None,
) -> subprocess.CompletedProcess:
  """Runs a read-only command in the repo, capturing text output."""
  return subprocess.run(
      cmd,
      cwd=repo_path,
      capture_output=True,
      text=True,
      errors="replace",
      env=env,
      check=False,
  )


def _tool_output(cmd: List[str], repo_path: str, *, label: str, limit: int,
                 empty_msg: str) -> str:
  """Returns cmd's stdout truncated to `limit`, or `empty_msg` if empty.

  Tool output is fed back to the model, so failures are reported as text
  instead of raised.
  """
  try:
    res = _run_in_repo(cmd, repo_path)
  except Exception as e:  # pylint: disable=broad-exception-caught
    return f"[ERROR] {label} failed: {e}"
  return res.stdout[:limit] if res.stdout else empty_msg


def find_roll_commit(repo_path: str, conflicted: bool) -> Optional[str]:
  """Locates a commit from the most recent Chromium autoroll.

  An autoroll lands as three commits:

    1. "Revert Cobalt."                              (upstream baseline)
    2. "Update to <milestone>."                      (pure upstream changes)
    3. "CONFLICTED Cherry pick ...: Update to <milestone>."
                                                     (Cobalt re-applied)

  Diffing #2 answers "what did upstream change?". Diffing #3 answers "what
  does Cobalt add on top of upstream, and did it still land correctly?".

  Args:
    repo_path: Repository to search.
    conflicted: When True return commit #3, otherwise return commit #2.

  Returns:
    The commit SHA, or None if no matching roll commit exists.
  """
  # Both subjects contain "Update to <milestone>", so the pure upstream
  # subject is anchored with ^ to exclude the cherry-picks.
  pattern = ("^CONFLICTED Cherry pick.*Update to [0-9]"
             if conflicted else "^Update to [0-9]")
  try:
    log_res = _run_in_repo(
        ["git", "log", "-n20", f"--grep={pattern}", "--format=%H %s"],
        repo_path)
  except OSError:
    return None

  for line in log_res.stdout.splitlines():
    parts = line.split(" ", 1)
    if len(parts) != 2:
      continue
    # %H %s always begins with the SHA, so the subject must be inspected
    # explicitly rather than testing the start of the whole line.
    subject = parts[1]
    if conflicted == subject.startswith("CONFLICTED"):
      return parts[0]
  return None


def show_roll_diff(
    repo_path: str,
    sha: str,
    target_path: str,
    label: str,
) -> str:
  """Renders `git show` for a roll commit, optionally scoped to one file."""
  cmd = (["git", "show", "--stat", "-p", sha, "--", target_path]
         if target_path else ["git", "show", "--stat", sha])
  return _tool_output(
      cmd,
      repo_path,
      label=f"{label[0].upper()}{label[1:]} diff",
      limit=8000,
      empty_msg=f"No {label} changes in {sha} for: {target_path}")


@dataclasses.dataclass(frozen=True)
class ToolContext:
  """Ambient state a tool handler may need beyond its own argument string."""

  repo_path: str
  session_changes: Optional[List["AgentChangeRecord"]] = None


def split_clean_tokens(raw: str) -> List[str]:
  """Splits on whitespace, stripping quoting and punctuation noise.

  Models routinely wrap paths in backticks or angle brackets and append
  trailing punctuation (e.g. "`foo/bar.h`," or "<foo/bar.h>"). Only the
  leading and trailing characters are stripped, so an interior colon in a
  "path:line-range" token is preserved for the caller to interpret.
  """
  return [t.strip("`'\"<>,;:") for t in raw.split() if t.strip("`'\"<>,;:")]


def _tool_read_file(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_READ_FILE: <path> [line_range] - reads a file or a line range."""
  tokens = split_clean_tokens(args)
  if not tokens:
    return "[ERROR] No file path provided."
  target_rel = tokens[0]
  line_range = ""
  for t in tokens[1:]:
    if re.match(r"^\d+-\d+$", t) or re.match(r"^\d+\.\.\d+$", t):
      line_range = t.replace("..", "-")
      break
  if ":" in target_rel and not line_range:
    parts = target_rel.split(":", 1)
    target_rel = parts[0]
    if (re.match(r"^\d+-\d+$", parts[1]) or
        re.match(r"^\d+\.\.\d+$", parts[1])):
      line_range = parts[1].replace("..", "-")

  target_abs = resolve_repo_file_path(target_rel, ctx.repo_path)
  if not os.path.isfile(target_abs):
    return f"[ERROR] File does not exist: {target_rel}"
  try:
    with open(target_abs, "r", encoding="utf-8", errors="replace") as f:
      lines = f.readlines()
    if line_range and "-" in line_range:
      s_str, e_str = line_range.split("-", 1)
      s_line = max(1, int(s_str))
      e_line = min(len(lines), int(e_str))
      selected = lines[s_line - 1:e_line]
      numbered = [f"{s_line + i}: {l}" for i, l in enumerate(selected)]
      return "".join(numbered)
    if len(lines) <= 250:
      numbered = [f"{i + 1}: {l}" for i, l in enumerate(lines)]
      return "".join(numbered)
    preview = lines[:150]
    numbered = [f"{i + 1}: {l}" for i, l in enumerate(preview)]
    numbered.append(
        f"\n[... Truncated {len(lines) - 150} lines. Use TOOL_READ_FILE: "
        f"{target_rel} <start>-<end> to view specific lines]\n")
    return "".join(numbered)
  except Exception as e:  # pylint: disable=broad-exception-caught
    return f"[ERROR] Could not read {target_rel}: {e}"


def _tool_grep(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_GREP: <query> [path_or_glob] - git grep across source files."""
  query = ""
  path_filter = ""
  q_match = re.match(r'^([\'"])(.*?)\1(?:\s+(.*))?$', args)
  if q_match:
    query = q_match.group(2).strip()
    path_filter = (q_match.group(3) or "").strip().strip("`'\"")
  else:
    parts = args.split(None, 1)
    if len(parts) == 2:
      query = parts[0].strip("`'\"")
      path_filter = parts[1].strip("`'\"")
    elif len(parts) == 1:
      query = parts[0].strip("`'\"")
    else:
      return "[ERROR] No query provided to TOOL_GREP."

  query = re.sub(r"\s*\(\s*\)\s*$", "", query).strip()
  if not query:
    return "[ERROR] Empty query in TOOL_GREP."

  grep_cmd = ["git", "grep", "-n", "-I", "--max-count=15", "-e", query]
  if path_filter:
    grep_cmd.extend(["--", path_filter])
  else:
    grep_cmd.extend([
        "--",
        "*.gn",
        "*.gni",
        "*.h",
        "*.cc",
        "*.cpp",
        "*.inc",
        "*.java",
        "*.rs",
        "*.py",
    ])
  try:
    res = _run_in_repo(grep_cmd, ctx.repo_path)
    lines = res.stdout.splitlines()[:30]
    text = "\n".join(lines)
    filt_desc = path_filter or "codebase"
    return (text[:6000].strip()
            if text else f"No matches found for: {query} (filter: {filt_desc})")
  except Exception as e:  # pylint: disable=broad-exception-caught
    return f"[ERROR] Grep failed: {e}"


def _tool_find_file(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_FIND_FILE: <pattern> - locates files by name fragment."""
  tokens = split_clean_tokens(args)
  pattern = tokens[0] if tokens else args.strip("`'\"")
  if not pattern.startswith("*") and not pattern.endswith("*"):
    pattern = f"*{pattern}*"
  try:
    res = _run_in_repo(
        ["find", ".", "-iname", pattern, "-not", "-path", "*/.*"],
        ctx.repo_path)
    lines = [l.lstrip("./") for l in res.stdout.splitlines()[:25]]
    return "\n".join(lines) if lines else f"No matches found for: {pattern}"
  except Exception as e:  # pylint: disable=broad-exception-caught
    return f"[ERROR] Find failed: {e}"


def _tool_list_dir(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_LIST_DIR: <dir_path> - lists directory entries."""
  tokens = split_clean_tokens(args)
  dir_rel = tokens[0] if tokens else "."
  dir_abs = resolve_repo_file_path(dir_rel, ctx.repo_path)
  if not os.path.isdir(dir_abs):
    return f"[ERROR] Directory does not exist: {dir_rel}"
  try:
    entries = sorted(os.listdir(dir_abs))[:50]
    formatted = []
    for e in entries:
      full_e = os.path.join(dir_abs, e)
      is_d = "/" if os.path.isdir(full_e) else ""
      formatted.append(f"{e}{is_d}")
    return "\n".join(formatted) if formatted else "(Directory is empty)"
  except Exception as e:  # pylint: disable=broad-exception-caught
    return f"[ERROR] List directory failed: {e}"


def _tool_git_show(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_GIT_SHOW: <ref> - shows a commit or object."""
  tokens = split_clean_tokens(args)
  ref = tokens[0] if tokens else args
  if not ref or ref.startswith("-"):
    return f"[ERROR] TOOL_GIT_SHOW expects a ref, got: {ref!r}"
  return _tool_output(
      ["git", "show", "--no-color", "--no-ext-diff", "--no-textconv", ref],
      ctx.repo_path,
      label="Git show",
      limit=4000,
      empty_msg=f"Could not show ref: {ref}")


def _gh_pr(args: str, ctx: ToolContext, verb: str, extra: List[str],
           limit: int) -> str:
  """Runs `gh pr <verb> <number> <extra...>` for the PR number in args."""
  match = re.search(r"(\d+)", args)
  if not match:
    return f"[ERROR] Could not extract PR number from: {args}"
  pr_target = match.group(1)
  try:
    res = _run_in_repo(["gh", "pr", verb, pr_target] + extra, ctx.repo_path)
  except Exception as e:  # pylint: disable=broad-exception-caught
    return f"[ERROR] gh pr {verb} failed: {e}"
  return (res.stdout[:limit]
          if res.stdout else f"Could not {verb} PR: {pr_target} ({res.stderr})")


def _tool_read_pr(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_READ_PR: <number> - fetches PR metadata via the gh CLI."""
  return _gh_pr(args, ctx, "view", ["--json", "number,title,body,commits"],
                8000)


def _tool_pr_diff(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_PR_DIFF: <number> - fetches a PR diff via the gh CLI."""
  return _gh_pr(args, ctx, "diff", [], 16384)


# Read-only git diff flags the model may pass to TOOL_GIT_DIFF (plus -U<n>).
_GIT_DIFF_ALLOWED_FLAGS = frozenset({
    "--cached",
    "--staged",
    "--stat",
    "--numstat",
    "--shortstat",
    "--name-only",
    "--name-status",
    "--ignore-all-space",
    "-w",
})


def _tool_git_diff(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_GIT_DIFF: <args> - runs git diff with caller-supplied arguments.

  Arguments come from the model, so only refs, paths, "--" and the read-only
  flags in _GIT_DIFF_ALLOWED_FLAGS are accepted. Anything else starting with
  "-" is rejected (e.g. --output=<path> would write an arbitrary file).
  """
  diff_args = args.split()
  options = diff_args[:diff_args.index("--")] if "--" in diff_args else (
      diff_args)
  for x in options:
    if (x.startswith("-") and x not in _GIT_DIFF_ALLOWED_FLAGS and
        not re.fullmatch(r"-U\d+|--unified=\d+", x)):
      return f"[ERROR] TOOL_GIT_DIFF option not allowed: {x}"
  return _tool_output(
      ["git", "diff", "--no-color", "--no-ext-diff", "--no-textconv"] +
      diff_args,
      ctx.repo_path,
      label="Git diff",
      limit=16384,
      empty_msg=f"Git diff empty or failed for: {diff_args}")


def _tool_git_log(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_GIT_LOG: [count] [path] - shows recent commits, optionally scoped."""
  arg_list = args.split()
  count = 5
  if arg_list and arg_list[0].isdigit():
    count = int(arg_list[0])
    arg_list = arg_list[1:]
  target_path = " ".join(arg_list)
  cmd = ["git", "log", f"-n{count}", "--oneline"]
  if target_path:
    cmd.extend(["--", target_path])
  return _tool_output(
      cmd,
      ctx.repo_path,
      label="Git log",
      limit=4000,
      empty_msg=f"No git log found for: {target_path}")


def _roll_diff(args: str, ctx: ToolContext, conflicted: bool, label: str,
               missing_msg: str) -> str:
  """Shows one roll commit's diff, optionally scoped to the path in args."""
  target_path = sanitize_filepath_token(args)
  sha = find_roll_commit(ctx.repo_path, conflicted=conflicted)
  if not sha:
    return missing_msg
  return show_roll_diff(ctx.repo_path, sha, target_path, label)


def _tool_upstream_diff(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_UPSTREAM_DIFF: <path> - shows the pure Chromium roll's changes."""
  return _roll_diff(
      args, ctx, False, "upstream",
      "Could not find upstream roll commit ('Update to <milestone>')")


def _tool_cobalt_diff(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_COBALT_DIFF: <path> - shows Cobalt's re-applied delta on the roll."""
  return _roll_diff(args, ctx, True, "Cobalt",
                    ("Could not find Cobalt cherry-pick commit "
                     "('CONFLICTED Cherry pick ...: Update to <milestone>')"))


def _tool_gclient_sync(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_GCLIENT_SYNC - resyncs dependencies. Takes no arguments."""
  del args  # This directive carries no arguments.
  try:
    res = _run_in_repo(["gclient", "sync", "-D"], ctx.repo_path,
                       get_clean_build_env())
    out = f"{res.stdout}\n{res.stderr}".strip()
    if out:
      return out[:4000]
    return f"gclient sync completed with exit code {res.returncode}"
  except Exception as e:  # pylint: disable=broad-exception-caught
    return f"[ERROR] gclient sync failed: {e}"


def _tool_get_history(args: str, ctx: ToolContext) -> Optional[str]:
  """TOOL_GET_HISTORY: <count | all | iteration | start-end | filepath>.

  Returns None when the argument parses as numeric but matches no branch,
  letting the dispatcher fall through to the unknown-command error exactly
  as the original if-chain did.
  """
  session_changes = ctx.session_changes
  if not session_changes:
    return "[NOTICE] No recorded change history available in this session yet."

  if args.lower() in ("all", "full"):
    return (
        f"=== Full Change History ({len(session_changes)} records) ===\n\n" +
        "\n\n".join(r.to_prompt_str() for r in session_changes))

  clean_target = args.strip("`'\"")
  # A non-numeric argument is treated as a file path (or a substring of one).
  if not re.match(
      r"^(?:iteration|iter|#)?\s*\d+(?:\s*(?:-|to|\.\.)\s*\d+)?$",
      clean_target,
      re.IGNORECASE,
  ):
    matched_files = [
        r for r in session_changes if (clean_target in r.target_file or any(
            clean_target in f for f in r.modified_files))
    ]
    if matched_files:
      return (f"=== Change Records for '{clean_target}' "
              f"({len(matched_files)} records) ===\n\n" +
              "\n\n".join(r.to_prompt_str() for r in matched_files))
    return f"[NOTICE] No change records found matching file '{clean_target}'."

  # Iteration range: e.g. "1-5" or "1..5".
  m_range = re.search(r"(\d+)\s*(?:-|to|\.\.)\s*(\d+)", args)
  if m_range:
    s_iter = int(m_range.group(1))
    e_iter = int(m_range.group(2))
    matched = [r for r in session_changes if s_iter <= r.iteration <= e_iter]
    if not matched:
      return ("[NOTICE] No change records found in iteration range "
              f"{s_iter}-{e_iter}.")
    return (f"=== Change Records for Iterations {s_iter}-{e_iter} "
            f"({len(matched)} records) ===\n\n" +
            "\n\n".join(r.to_prompt_str() for r in matched))

  # Specific iteration, or a trailing count of recent records.
  m_iter = re.search(r"(?:iteration|iter|#)?\s*(\d+)", args, re.IGNORECASE)
  if m_iter:
    num = int(m_iter.group(1))
    if "iter" in args.lower():
      matched = [r for r in session_changes if r.iteration == num]
      if not matched:
        return f"[NOTICE] No change record found for iteration {num}."
      return (f"=== Change Record for Iteration {num} ===\n\n" +
              "\n\n".join(r.to_prompt_str() for r in matched))
    matched = (
        session_changes[-num:]
        if num < len(session_changes) else session_changes)
    return (f"=== Last {len(matched)} Change Records (out of "
            f"{len(session_changes)}) ===\n\n" +
            "\n\n".join(r.to_prompt_str() for r in matched))

  return None


# Maps a directive prefix to its handler. Prefixes are matched longest-first,
# so adding a shorter prefix later cannot shadow an existing longer one.
# TOOL_GCLIENT_SYNC is intentionally colon-less: it takes no arguments.
_TOOL_HANDLERS: Dict[str, Callable[[str, ToolContext], Optional[str]]] = {
    "TOOL_READ_FILE:": _tool_read_file,
    "TOOL_GREP:": _tool_grep,
    "TOOL_FIND_FILE:": _tool_find_file,
    "TOOL_LIST_DIR:": _tool_list_dir,
    "TOOL_GIT_SHOW:": _tool_git_show,
    "TOOL_READ_PR:": _tool_read_pr,
    "TOOL_PR_DIFF:": _tool_pr_diff,
    "TOOL_GIT_DIFF:": _tool_git_diff,
    "TOOL_GIT_LOG:": _tool_git_log,
    "TOOL_UPSTREAM_DIFF:": _tool_upstream_diff,
    "TOOL_COBALT_DIFF:": _tool_cobalt_diff,
    "TOOL_GCLIENT_SYNC": _tool_gclient_sync,
    "TOOL_GET_HISTORY:": _tool_get_history,
    "TOOL_CHANGE_HISTORY:": _tool_get_history,
    "TOOL_HISTORY:": _tool_get_history,
}


def execute_local_tool(
    cmd: str,
    repo_path: str,
    session_changes: Optional[List["AgentChangeRecord"]] = None,
) -> str:
  """Executes safe read-only multi-turn inspection tools for LLM.

  Dispatches to the handler registered in _TOOL_HANDLERS for the directive's
  prefix. The handler receives the text following the first colon, already
  stripped. A handler returning None means "not handled", which surfaces the
  unknown-command error.
  """
  clean_cmd = cmd.strip()
  ctx = ToolContext(repo_path=repo_path, session_changes=session_changes)

  for prefix in sorted(_TOOL_HANDLERS, key=len, reverse=True):
    if not clean_cmd.startswith(prefix):
      continue
    args = clean_cmd.split(":", 1)[1].strip() if ":" in clean_cmd else ""
    result = _TOOL_HANDLERS[prefix](args, ctx)
    if result is not None:
      return result
    break

  return f"[ERROR] Unknown tool command: {clean_cmd}"
