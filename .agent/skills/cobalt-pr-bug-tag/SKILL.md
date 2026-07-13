---
name: cobalt-pr-bug-tag
description: Guides the agent to explicitly search for related bugs and append "Bug: <id>" to commit messages and pull request descriptions when making Cobalt PRs.
---

# Skill: Cobalt PR Bug Tagging

This skill ensures that all Cobalt pull requests (PRs) and commits explicitly reference the associated bug or issue ID in their description.

## When to Use
Use this skill whenever you are preparing to:
- Commit code changes (`git commit`) in the Cobalt repository.
- Create a pull request (`gh pr create`) for the Cobalt repository.
- Draft or update commit/PR descriptions for user review.

---

## Execution Steps

### 1. Identify the Related Bug ID
Before creating a commit or pull request, actively search for the associated Buganizer ID (`b/...` or numerical ID like `495203133`) or issue number. Check the following sources in order:
1. **User Request & Transcript**: Review recent messages in the conversation for explicit mentions of a bug number (e.g., "Bug: 123456789", "b/123456789", or "#123456789").
2. **Branch Name**: Check the current git branch name (`git branch --show-current`) for bug numbers (e.g., `fix/b-123456789` or `bug-123456789`).
3. **Recent Commits**: Inspect `git log` or comments in modified files for related bug links or numbers.
4. **Ask the User**: If no bug ID can be found after checking the above sources, explicitly ask the user for the relevant Bug ID before finalizing the PR creation, or confirm if the PR should proceed without one.

### 2. Format the Tag
Once the Bug ID is identified, format it as follows:
```text
Bug: <id>
```
*(Note: `Bug: #<id>` or `Bug: b/<id>` are also acceptable depending on the specific tracking format provided by the user).*

### 3. Place at the Bottom of the Description
In accordance with Cobalt CL and PR tagging rules, place the `Bug:` tag at the very bottom of the commit message and pull request description, after all human-readable content and explanations.

It should group with other required metadata tags (such as `TAG=agy` and `CONV=<conversation_id>`).

#### Example Description Format:
```text
android: Fix NullPointerException in lifecycle init

Ensures that the initialization provider is properly handled when merging manifests across dependencies.

Fixes crash reported on device startup.

Bug: 495203133
TAG=agy
CONV=77dc1de4-dd7c-479a-9937-22c955c12d6a
```

---

## Critical Rules
- **Never place tag lines at the top**: All metadata tags (`Bug:`, `TAG=`, `CONV=`) MUST be on separate lines at the very end of the description block.
- **Do not invent Bug IDs**: Never hallucinate or guess a numerical ID. If unsure, verify with the user.
