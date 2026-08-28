---
name: apply-remove
description: Specialized skill for surgically removing components from Cobalt using AI-powered automation. Use this to apply the "Subtraction Method" to BUILD.gn and C++ files at a specific pivot point.
---

# Apply Remove Skill

This skill provides a high-leverage workflow for removing dependencies from Cobalt by combining local context gathering with Gemini's reasoning.

## When to use
- You have identified a component to remove (the "target").
- You have identified where to break the dependency (the "pivot").
- You want to automatically guard headers and symbols in C++ and update `BUILD.gn`.

## Workflow

### 1. Gather Context
Run the `gemini_remove.py` script to gather all relevant file context (GN targets and C++ files).
- **Command:** `python3 cobalt/tools/automate_remove/apply-remove/scripts/gemini_remove.py --pivot //pivot:target --target //target:to_remove`

### 2. Apply Modifications (Automated)
Once the script outputs the context JSON, the agent (Gemini) will:
1. **Update BUILD.gn**: Locate the pivot target and add an `if (is_cobalt)` subtraction block.
   - **Constraint**: Must be placed AFTER the original `deps`/`sources` list to avoid GN errors.
2. **Guard Headers**: Wrap `#include` statements in the discovered C++ files with `#if !BUILDFLAG(IS_COBALT)`.
   - **Constraint**: Merge adjacent guards into a single block.
   - **Constraint**: Append `// nogncheck` to guarded lines.
   - **Constraint**: Ensure `#include "build/build_config.h"` is present if not already there.
3. **Guard Symbols**: Identify and wrap usage of the component's symbols (e.g., interface registrations).

## Tooling
- `scripts/gemini_remove.py`: Context gatherer that identifies relevant files and targets.
