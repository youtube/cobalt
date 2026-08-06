# Git Operational Directives for Agent Delegation

## Dedicated Git Operations Subagent
All Git committing, staging, and merging operations MUST be delegated to the dedicated `git-committer` subagent. The Main Orchestrator Agent must never execute Git modifying operations directly.

## 1. Linear Commit Policy (No Amending)
- When saving new changes or iterations, agents must commit changes on top normally using `git commit -m "<msg>"`.
- Do NOT use `git commit --amend` or alter historical commit objects.

## 2. Upstream Update Policy (No Rebasing)
- When pulling upstream changes from `main`, agents must use `git fetch` and `git merge origin/main`.
- Do NOT execute `git rebase` under any circumstances.

## 3. Remote Publication Policy (No Unapproved Pushes)
- Agents and subagents must NEVER execute `git push` or publish commits to remote repositories without obtaining explicit, confirmed authorization from the user.
