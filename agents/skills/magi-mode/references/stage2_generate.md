# Stage 2: Generate

*Note: Steps 1, 2, 4, 6, and 7 are ONLY executed if `task_type` is
`IMPLEMENTATION`.* *Step 3 (Select Modules) is executed for all task types to
initialize the State Block. Step 5 (Synthesize) is executed during the initial
generation for `IMPLEMENTATION` tasks, but its synthesis logic is also invoked
during Stage 3 (Refine) iteration for all task types if code changes are
generated.*

## Step 1: Scaffold (Implementation)

1. **Roughing In:** Invoke an Implementation sub-agent. The Implementation MUST
   read `project.magi.json` to understand the goal. Their mandate is to create
   necessary files, define class interfaces, set up Mojo pipes, and GN/DEPS
   rules. Leave implementation details empty or stubbed (e.g.,
   `NOTIMPLEMENTED()`). The Implementation MUST signal
   `next_stage: SCAFFOLDING`.

## Step 2: TDD Boundary (The Test Expert)

1. **Test-Driven Development:** After the Implementation completes the scaffold,
   invoke a Test Expert sub-agent to establish the testing boundaries. Their
   mandate is to add test files (`*_unittest.cc`), define the required test
   fixtures, and stub out the critical test cases based on the Implementation's
   scaffold. To ensure failure in Chromium's GTest framework (confirming TDD
   behavior), the Test Expert MUST insert `ADD_FAILURE() << "NOT IMPLEMENTED"`
   into the stubbed test cases. The Test Expert SHOULD signal
   `next_stage: PREPARATION`.
2. **Scaffold Verification:** Before proceeding to Step 3, the Orchestrator MUST
   attempt to build the scaffolded targets. If `build_targets` are defined in
   `project.magi.json`, the Orchestrator MUST verify that the scaffold compiles
   and that all newly added tests fail (confirming TDD behavior).
3. **Snapshot:** The Orchestrator records this state (e.g., as a local commit)
   as the "Base Scaffold" so all parallel Implementation Modules share the exact
   same multi-file API and test boundaries.

## Step 3: Select Modules (The Orchestrator)

1. **Needs Assessment:** The Orchestrator reads `project.magi.json` and the
   [ROUTING.md](../ROUTING.md) catalog to select the appropriate Scanners
   (Auditors) based on the execution path:
   - **FAST_PATH:** Select a single auditor (typically the Auditor).
   - **RIGOR_PATH:** Select the "Big Three" (Security, Performance, Auditor)
     plus any relevant domain modules.
2. **State Initialization:** The Orchestrator writes the initial State Block to
   `state_block.magi.json`. The `checklist` field is initialized with the
   **Union Set** of all checklist keys from every selected ruleset, set to
   `false`.
3. **State Transport Selection:** The Orchestrator selects `state_transport`
   based on the risk score (Scanner Count * Target Files):
   - **FILE_IO:** Use if risk score > 15.
   - **EPHEMERAL_WITH_LOGS:** Default for standard tasks.
4. **JSON Contract (`state_block.magi.json`):** See [EXAMPLES.md](../EXAMPLES.md)
   for a full example.

## Step 4: Implement (Implementation Modules)

1. **Parallel Implementation:** Invoke the selected sub-agents in parallel
   (`wait_for_previous: false`). Instruct each to implement the stubbed
   internals from the Base Scaffold.
2. **Mandates for Modules:**
   - **Production Code Focus:** Modules SHOULD focus primarily on implementing
     the production code logic.
   - **Production Hardening:** Modules MUST adhere to the **Production Hardening
     Checklist** (defined in [tooling_guidance.md](./tooling_guidance.md))
     during implementation.
   - **Domain Edge Cases:** If a module identifies specific edge cases or
     scenarios that need verification, they MUST add a stubbed test case in the
     test file (with both `ADD_FAILURE() << "NOT IMPLEMENTED"` and a descriptive
     TODO comment) rather than fully implementing the test.
   - **Test Hooks & Accessors:** Modules MUST provide any necessary public
     accessors, test-only hooks, or `friend` declarations in the production code
     that the Test Expert will need to verify internal state.
   - **Signature Integrity Lock:** Modules MUST NOT change scaffolded signatures
     (function names, parameters, or return types). If a module identifies a
     necessary API change, it MUST signal `next_stage: ESCALATION` and produce a
     detailed `conflict_report` for human review.
3. **File I/O:** Each sub-agent MUST read `project.magi.json` to ground their
   implementation in the actual requirements. They MUST securely save their
   draft to disk using the versioned naming convention
   `[filename].[persona].magi.[iteration]` (e.g., `host.cc.security.magi.1`).
   Sub-agents SHOULD signal `next_stage: SYNTHESIS` upon completion.

## Step 5: Synthesize (Synthesis)

1. **Conflict Resolution:** Synthesis MUST use a surgical 3-way merge strategy
   (Base Scaffold + Draft A + Draft B) rather than full-file overwrites to
   resolve conflicts between modules.
2. **Hardening Audit:** Synthesis MUST perform a final audit against the
   **Production Hardening Checklist** (defined in
   [tooling_guidance.md](./tooling_guidance.md)) during synthesis to ensure
   merged code maintains architectural integrity.
3. **Synthesis Build (Empirical Gate):** If `build_targets` are defined in
   `project.magi.json`, Synthesis MUST run the local build/test suite on "Draft
   A".
   - **Failure:** If the code fails to compile, Synthesis MUST loop back to
     internal refinement and fix the syntax/link errors. It MUST NOT signal
     `next_stage: TEST_FILLING` or `CRITIQUE` until the build is green.
   - **Success:** Once the build is verified, Synthesis MUST attach the build
     logs to the synthesis report before signaling `next_stage: TEST_FILLING` or
     `CRITIQUE`.

## Step 6: Implement Tests (The Test Expert)

1. Fill out the actual implementation of tests.

## Step 7: Verification Build (The Test Expert)

1. Run tests to verify they PASS. If they fail due to implementation bugs, loop
   back to Step 5 or 4.
