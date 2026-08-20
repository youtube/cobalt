# Automated Cobalt Chromium Rebase & Self-Healing Pipeline

An autonomous, multi-phase AI-driven engineering pipeline powered by Google Cloud Vertex AI and Gemini. Designed to resolve merge conflicts, repair GN build definitions, and heal C++/Java compilation breaks across Chromium milestone upgrades (e.g. M138 to M139) for Cobalt.

---

## 1. Architecture Overview

The pipeline executes in sequential, self-healing phases:

```
+-----------------------------------------------------------------------------+
| Phase 1: Unified Conflict Resolution (resolve_conflicts.py)                 |
|   - Resolves DEPS merge conflicts & validates Python AST syntax             |
|   - Resolves C++, Java, GN, and config file conflict blocks                 |
|   - Fast-path adopts upstream (--theirs) for binary/fuzz corpus files       |
+-----------------------------------------------------------------------------+
                                     |
                                     v
+-----------------------------------------------------------------------------+
| Phase 2: Toolchain & Dependency Sync (gclient sync -D)                      |
|   - Synchronizes Clang, Rust, NDK, node_modules, and CIPD packages          |
|   - Self-healing retry with --force --reset                                 |
+-----------------------------------------------------------------------------+
                                     |
                                     v
+-----------------------------------------------------------------------------+
| Phase 3: GN Generation & Header Verification (cobalt/build/gn.py)           |
|   - Executes `cobalt/build/gn.py -p <platform> -C <build_type> --check`     |
|   - Resolves bridge targets (group/component) and duplicate argument imports|
+-----------------------------------------------------------------------------+
                                     |
                                     v
+-----------------------------------------------------------------------------+
| Phase 4: Compiler Self-Healing Loop (autoninja_loop.py)                     |
|   - Invokes `autoninja -k 1 -C out/<dir> <target>`                          |
|   - Third-party source protection guardrail (routes fixes to BUILD.gn)      |
|   - Generates surgical SEARCH/REPLACE code patches via Vertex AI            |
|   - Escalates to Pro model (gemini-2.5-pro) on repeat diagnostics           |
+-----------------------------------------------------------------------------+
                                     |
                                     v
+-----------------------------------------------------------------------------+
| Phase 5: Knowledge Bank Sync & Report Generation                            |
|   - Records working code patches to out/memory/knowledge_bank.json          |
|   - Uploads knowledge bank to Google Cloud Storage (GCS) if configured      |
|   - Generates final execution summary in results/M140_rebase_summary.md     |
+-----------------------------------------------------------------------------+
```

---

## 2. Directory Structure

```
.github/rebase/
│
├── reasoning_engine/                   # [DEPLOYED TO VERTEX AI]
│   ├── __init__.py                     # Package initialization
│   ├── engine.py                       # CobaltReasoningEngine service definition
│   ├── deploy.py                       # Vertex AI deployment & lifecycle CLI
│   ├── requirements.txt                # Container runtime dependencies
│   └── skills/                         # Declarative domain instructions (Markdown)
│       ├── cobalt_rebase.md            # Master guidelines & behavior preservation
│       ├── compiler_healing.md         # Compiler & linker break repair heuristics
│       ├── gn_healing.md               # GN build rules & visibility repair
│       └── conflict_resolution.md      # DEPS AST & merge conflict rules
│
├── run_rebase_pipeline.py              # [CI RUNNER] End-to-end multi-phase orchestrator
├── autoninja_loop.py                   # [CI RUNNER] autoninja compiler healing loop
├── resolve_conflicts.py                # [CI RUNNER] Multi-file conflict resolver
├── rebase_memory.py                    # [CI RUNNER] Memory bank interface & GCS sync
├── test_rebase_suite.py                # [CI RUNNER] Comprehensive unit test suite
├── test_api_connection.py            # [CI RUNNER] Vertex AI connectivity diagnostic
├── results/                            # [ARTIFACTS] Generated summaries
│   └── M139_rebase_summary.md
└── README.md                           # Documentation & operational manual
```

---

## 3. Quick Start & Execution

### Prerequisites
1. **Google Cloud Authentication**:
   ```bash
   gcloud auth application-default login
   export GCP_PROJECT="your-gcp-project-id"
   export GCP_LOCATION="us-central1"
   ```
2. **Environment**:
   Ensure `depot_tools` is in your `PATH`.

---

### Running the End-to-End Pipeline

To execute all three phases sequentially for Android (`cobalt_apk`):
```bash
python3 .github/rebase/run_rebase_pipeline.py \
  --platform android-arm \
  --build-type devel \
  --target cobalt_apk \
  --model gemini-2.5-flash
```

For Linux Desktop:
```bash
python3 .github/rebase/run_rebase_pipeline.py \
  --platform linux-x64x11 \
  --build-type devel \
  --target cobalt \
  --model gemini-2.5-flash
```

---

### Running Individual Phases

* **Phase 1 Only (Conflict Resolution)**:
  ```bash
  python3 .github/rebase/resolve_conflicts.py
  ```

* **Phase 3 Only (Compiler Self-Healing)**:
  ```bash
  python3 .github/rebase/autoninja_loop.py \
    --out-dir android-arm_devel \
    --target cobalt_apk
  ```

---

## 4. Vertex AI Reasoning Engine Deployment (`reasoning_engine/deploy.py`)

To deploy, update, or list managed Reasoning Engine instances on Google Cloud Vertex AI:

* **Deploy**:
  ```bash
  python3 .github/rebase/reasoning_engine/deploy.py deploy \
    --project-id "$GCP_PROJECT" \
    --location "us-central1" \
    --staging-bucket "gs://your-staging-bucket"
  ```

* **Update**:
  ```bash
  python3 .github/rebase/reasoning_engine/deploy.py update \
    --resource-id "<REASONING_ENGINE_ID>" \
    --project-id "$GCP_PROJECT" \
    --location "us-central1" \
    --staging-bucket "gs://your-staging-bucket"
  ```

* **List**:
  ```bash
  python3 .github/rebase/reasoning_engine/deploy.py list \
    --project-id "$GCP_PROJECT" \
    --location "us-central1"
  ```

---

## 5. Domain Skills (`reasoning_engine/skills/`)

Prompts and domain rules are decoupled from Python code. To adjust rebase heuristics or add new patterns for future Chromium rolls (e.g. M140), edit the markdown files in `reasoning_engine/skills/`:

* **`reasoning_engine/skills/cobalt_rebase.md`**: Master behavior preservation principles, Starboard macros (`USE_STARBOARD_MEDIA`, `IS_COBALT`), and investigation workflows using Chromium Code Search (`source.chromium.org`) and Gitiles (`chromium.googlesource.com`).
* **`reasoning_engine/skills/compiler_healing.md`**: C++/Java header splits (e.g. `base/notimplemented.h`, `base/timer/elapsed_timer.h`), method signature updates, and linker stubs.
* **`reasoning_engine/skills/gn_healing.md`**: GN visibility rules and template variable forwarding.
* **`reasoning_engine/skills/conflict_resolution.md`**: Upstream roll priority, DEPS syntax rules, and multi-turn tool commands.

---

## 6. Long-Term Knowledge Bank & GCS Sync

Successful code fixes are recorded in `out/memory/knowledge_bank.json` and fed into future Gemini prompts as few-shot working examples.

To persist the knowledge bank across ephemeral build machines via GCS:
```bash
export GCS_MEMORY_URI="gs://your-bucket-name/rebase_memory/knowledge_bank.json"
```
The pipeline will automatically **pull** existing memory at startup and **push** new fixes upon completion using the native `google-cloud-storage` client library.

---

## 7. Running Unit Tests

To run the local unit test suite (verifies conflict extractors, AST validation, diagnostic parsers, and GN diff application):

```bash
python3 .github/rebase/test_rebase_suite.py
```
