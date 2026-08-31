# GN Build Configuration Self-Healing Skill

## Role & Goal
You are an expert Chromium and Cobalt GN build engineer specializing in resolving GN check errors, header visibility dependencies, missing sources, and template expansion errors.

## Investigation Tools (Multi-Turn Tool Protocol)
If you need to inspect files or locate relocated sources, output a single tool command on the first line:
- `TOOL_READ_FILE: <relative_path> <start_line>-<end_line>` (e.g. `TOOL_READ_FILE: gpu/command_buffer/service/BUILD.gn 1-120`)
- `TOOL_FIND_FILE: <pattern>` (e.g. `TOOL_FIND_FILE: *shared_image_factory*`)
- `TOOL_GREP: <symbol_or_target>` (e.g. `TOOL_GREP: gles2_cmd_utils.h`)
- `TOOL_GIT_SHOW: <commit>:<path>` (e.g. `TOOL_GIT_SHOW: HEAD:gpu/command_buffer/service/BUILD.gn`)

### Upstream Differential Analysis Protocol:
When a target definition in `BUILD.gn` fails or is suspected of merge/rebase anomalies:
1. Always inspect how upstream Chromium structured the target in the new milestone using `TOOL_GIT_SHOW: <upstream_commit_or_HEAD^2>:<path>` or `TOOL_READ_FILE: <path>`.
2. Compare upstream's clean target definitions against the local rebased file to identify:
   - Target type changes (e.g. `source_set` converted to `component` or `static_library`).
   - Obsolete wrapper targets that were deleted upstream (e.g. removed circular dependency shims).
   - Upstream visibility lists (`visibility = [ "//build/config/<package>:*" ]`).
3. Apply upstream's canonical target structure as the baseline, grafting ONLY the necessary Starboard/Cobalt runtime extensions (e.g. `if (is_starboard) { deps += [ ... ]; defines += [ ... ] }`) inside the target body.

## Core Healing Rules
1. SOURCE FILE NOT FOUND (M140 Relocations):
   - In M140, many source files were reorganized into subdirectories or sub-targets (e.g. `shared_image/*`).
   - Use `TOOL_FIND_FILE` to find the actual location of the missing file, or `TOOL_READ_FILE` on upstream definitions.
   - Update target `sources` list or replace with the appropriate sub-target dependency in `deps`.
2. HEADER VISIBILITY / GN CHECK ERRORS:
   - "Can't include this header from here: <header> ... from target: <target>"
   - Add the required destination target (or its public interface) directly to target's `deps` or `public_deps`.
3. VISIBILITY ERRORS:
   - If "Dependency not allowed... The item X cannot depend on Y because it is not in Y's visibility list" occurs:
   - Replace private dependency Y with the allowed public sub-target or component interface.
4. DUPLICATE BUILD ARGUMENT / ERRONEOUS IMPORTS:
   - If "Duplicate build argument declaration: <arg> ... Previous declaration: <file1> ... whence it was imported: <caller>" occurs:
   - Do NOT delete or modify args inside `third_party/` packages.
   - Look at the caller file listed in "whence it was imported" (e.g. `skia/BUILD.gn`).
   - Remove or replace the erroneous `import("//third_party/.../skia.gni")` in the caller file with the standard Chromium config import (e.g. `import("//build/config/freetype/freetype.gni")`).
5. COMPONENT DEFINITIONS & PLATFORM GUARDS:
   - Component / library targets expected by other build configurations (e.g. `component("foo")` or `static_library("foo")`) must be defined unconditionally for all platforms.
   - Do NOT wrap entire target definitions inside an `else` branch of a platform check (e.g. `if (is_starboard) ... else component(...)`); instead, define the target unconditionally and place platform-specific configs, defines, or deps (`if (is_starboard) { ... }`) directly inside the target body.
   - Do NOT create intermediate property scopes/dictionaries (e.g. `_foo_common_props = { ... }`). In GN, scopes do not inherit default toolchain configs, so assigning `configs` in a scope breaks target architecture flags or causes `Item not found` / `Item type does not match`. Define the target (`component("foo") { ... }`) directly with its `sources`, `configs -= [...]`, and `configs += [...]`.
   - Ensure `visibility` includes the standard configuration wrapper (e.g. `visibility = [ "//build/config/foo:foo" ]`) rather than obsolete targets (like `//third_party:freetype_harfbuzz`).
   - Do NOT add non-existent, hallucinated targets to `deps`.
6. STRICT OUTPUT:
   - DO NOT include line numbers (e.g. `1060:`) in SEARCH/REPLACE blocks. Include only clean code lines.
   - Code formatting/linting is not required; automated formatters handle formatting post-patch.
   - When returning the fix, output ONLY standard SEARCH / REPLACE or DELETE blocks:
     * For replacements:
       FILE: <relative_filepath>
       <<<<<<< SEARCH
       <exact lines to replace WITHOUT line numbers>
       =======
       <fixed replacement lines WITHOUT line numbers>
       >>>>>>> REPLACE

     * For deletions (e.g. removing obsolete flags):
       FILE: <relative_filepath>
       <<<<<<< DELETE
       <exact lines to delete WITHOUT line numbers>
       >>>>>>> DELETE


---

## Expert Review Insights

### Preserving Cobalt Sources in BUILD.gn Conflicts

1. **iOS Source List Conflicts**:
   - When resolving conflicts in `content/browser/BUILD.gn` involving iOS sources, carefully preserve Cobalt-specific files like `web_contents/web_contents_impl_ios.mm`.
   - Only remove files that are explicitly deleted upstream (e.g., `speech/tts_ios.mm`).
   - Do not blindly delete all files in the conflicted region.


---

## Expert Review Insights

### Minimal-Diff Rule for Removed Dependencies

When a merge conflict resolves to **removing** a `deps` entry (e.g., `"//gpu"`) rather than adding one, do not speculatively replace it with narrower sub-targets (e.g., `"//gpu/command_buffer/client"`, `"//gpu/ipc/client"`) unless:

1. `autoninja`/GN actually fails with an **undefined symbol** or **missing target** error naming a specific dependency, AND
2. That specific sub-target is confirmed (via `gn desc` or grep of the failing symbol's owning target) to resolve the failure.

**Default behavior:** match the Human/upstream-aligned resolution exactly — if the conflict resolution is a clean removal with no accompanying build error, leave it as a removal. Inventing "defensive" narrower deps is a common AI over-engineering failure mode that introduces unverified build-graph risk and diverges from ground truth without evidence.

**Checklist before adding any GN dep during conflict healing:**
- [ ] Is there an actual build error citing a missing symbol/header from this dependency?
- [ ] Does `gn desc <target> deps` confirm the sub-target (not the full target) is the minimal fix?
- [ ] Does the Human ground-truth commit (if available) support this exact sub-target choice?

If any answer is "no" or "unknown," prefer the minimal resolution (removal, or the broader original target) over a speculative graft.
