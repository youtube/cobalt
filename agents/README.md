# Chromium Coding Agents

This directory provides a centralized location for files related to AI coding
agents (e.g. `gemini-cli`) used for development within the Chromium source tree.

The goal is to provide a scalable and organized way to share prompts and tools
among developers, accommodating the various environments (Linux, Mac, Windows)
and agent types in use.

<<<<<<< HEAD
=======
Googler-only docs: http://go/chrome-coding-with-ai-agents

>>>>>>> parent of 7d60f81f606 (CONFLICTED Chromium Cherry pick: Revert Cobalt.)
## Directory Structure

### Prompts

<<<<<<< HEAD
Prompts are located in the `prompts/` subdirectory. They are categorized as
follows:

- `prompts/common.GEMINI.md`: Contains common `GEMINI.md` file that defines the
  agent's core behavior, available tools, and system instructions for Chromium.
- `prompts/tasks/`: Contains prompts for specific, self-contained tasks (e.g.,
  "refactor this component," "add a new feature flag"). Each task belongs in its
  own directory with common metadata.
- `prompts/templates/`: Contains reusable prompt snippets that can be included
  in other prompts, such as instructions for using a specific tool.

```
src/agents/
└── prompts/
    ├── common.GEMINI.md             (general Chromium system prompt)
    ├── tasks/
    │   └── a_complex_task/
    │       └── README.md
    │       └── prompt.md
    └── templates/
        ├── specialized-tool.md
        └── platform-specific.md
```

### MCPs (Model Context Protocols)

MCPs are helper servers that provide a stable, tool-like interface for an AI
agent to use. Instead of giving an agent broad shell access, we can provide an
MCP that wraps a command-line tool or infrastructure service (like LUCI). This
is safer and more reliable.

MCPs will be located in the `mcp/` subdirectory.

```
src/agents/
└── mcp/
    ├── local_server_to_wrap_tool/
    │   ├── gemini-extension.json (MCP server should be near the code it wraps)
    │   ├── GEMINI.md
    │   └── README.md
    └── remote_server_configuration/
        ├── gemini-extension.json
        ├── GEMINI.md
        └── README.md
```
=======
Shared `GEMINI.md` prompts. See [`//agents/prompts/README.md`].

[`//agents/prompts/README.md`]: /agents/prompts/README.md

### Extensions & MCP Servers

Chrome-approved extensions & MCP servers. See [`//agents/extensions/README.md`].

Use `agents/extensions/install.py` to list and configure available servers.

[`//agents/extensions/README.md`]: /agents/extensions/README.md
>>>>>>> parent of 7d60f81f606 (CONFLICTED Chromium Cherry pick: Revert Cobalt.)

## Contributing

Please freely add self-contained task prompts and prompt templates that match
the format of the existing examples.

<<<<<<< HEAD
New MCP server configurations should be for owned and supported MCP servers
and include OWNERS.
=======
New MCP server configurations should be for owned and supported MCP servers and
include OWNERS.
>>>>>>> parent of 7d60f81f606 (CONFLICTED Chromium Cherry pick: Revert Cobalt.)

Changes to `common.GEMINI.md` should be done carefully as it's meant to be used
broadly.
