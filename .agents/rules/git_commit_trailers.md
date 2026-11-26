---
trigger: always_on
description: "Guidelines for formatting Git commit trailers (RFC 2822 / git interpret-trailers). Apply when composing commit messages and when creating or editing PR descriptions."
---

# Git Commit Trailers

When composing Git commit messages that include metadata such as tags or conversation identifiers, format them using standard Git trailers (RFC 2822 / `git interpret-trailers` format) rather than critique-style `KEY=VALUE` pairs.

## Format Specification

- **Syntax**: Format metadata as `Key: Value` pairs (e.g., `Tag: agy`, `Conv: <conversation-id>`).
- **Bug Trailer Format**: Bug trailers must specify the raw numeric issue ID only (e.g., `Bug: 123456789`). **Do NOT prefix bug numbers with `b/`, `b:`, `http://b/`, or `#`** (e.g., do not write `Bug: b/123456789` or `Bug: #123456789`). Multiple bugs should be placed on separate lines (e.g., `Bug: 123456789\nBug: 987654321`).
- **Placement**: Place trailers at the very end of the commit message in a single trailer block.
- **Ordering**: `Tag` and `Conv` trailers must always come **first** in the trailer block. Any other trailers (e.g., `Bug:`) must come after.
- **Separation**: Separate the trailer block from the commit subject and body by at least one empty line.
- **No Trailing Text**: Do not place any explanatory text, comments, or trailing lines after the trailer block.

## Example

```gitcommit
Add feature X to service Y

Implement feature X with full test coverage and error handling.

Tag: agy
Conv: 3631585a-1f0f-442f-9b8e-96a5a4f5fae4
Bug: 123456789
```
