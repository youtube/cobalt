---
name: cobalt-new-checkout
description: >-
  Guides the agent through setting up a new checkout of the Cobalt repository.
  Use when cloning and building Cobalt for the first time in a new workspace.
---

# Cobalt New Checkout Setup

This skill guides you through running the new checkout script for Cobalt.

## Steps

1. Run the script `cobalt_new_checkout.sh` at the root of the Cobalt repository.
2. The script is interactive. Use `send_input` to handle prompts for internal user status and GitHub username.
3. If the script fails, analyze the error, modify the script to fix the issue, and retry.
