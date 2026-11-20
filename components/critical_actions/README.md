# Critical Action History Component

Component that manages the critical action history

## Use Cases
- Storing a history of sensitive, critical user actions (such as settings changes, download triggers, or form fills) that could have security or privacy implications when done by an AI agent.
- Shared between multiple embedders and platforms (Desktop Chrome, ChromeOS, and potentially mobile).

## Directory Structure
- `core/`: The platform-independent SQLite database implementation (`CriticalActionDatabase`), data structures (`CriticalActionEntry`), and logic.
