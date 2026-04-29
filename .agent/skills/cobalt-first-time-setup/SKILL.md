---
name: cobalt-first-time-setup
description: >-
  Guides the agent through the first-time setup for Cobalt development.
  Use when setting up a new machine or environment for Cobalt.
---

# Cobalt First Time Setup

This skill guides you through running the first-time setup script for Cobalt.

## Steps

1. Run the script `//third_party/cobalt/cobalt_first_time_setup.sh` in a google3 workspace.
2. The script is interactive. Use `send_input` to handle prompts for Cloudtop, Name, LDAP, etc.
3. If the script fails, analyze the error, modify the script to fix the issue, and retry.
4. Upon completion, suggest running the `cobalt-new-checkout` skill.
