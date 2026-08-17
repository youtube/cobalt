# Automated Cobalt Chromium Rebase & Self-Healing Pipeline

An autonomous, multi-phase AI-driven engineering pipeline powered by Google Cloud Vertex AI and Gemini. Designed to resolve merge conflicts, repair GN build definitions, and heal C++/Java compilation breaks across Chromium milestone upgrades (e.g. M138 to M139) for Cobalt

---

## 1. Architecture Overview

The pipeline executes in three automated, self-healing phases:

```
+-----------------------------------------------------------------------------+
| Phase 1: Unified Conflict Resolution (resolve_conflicts.py)                 |
|   - Resolves DEPS merge conflicts & validates Python AST syntax             |
|   - Resolves C++, Java, GN, and config file conflict blocks                 |
|   - Fast-path adopts upstream (--theirs) for binary/fuzz corpus files       |
|   - Self-healing gclient sync loop                                          |
+-----------------------------------------------------------------------------+
                                     |
                                     v
+-----------------------------------------------------------------------------+
| Phase 2: GN Generation & Header Verification (run_rebase_pipeline.py)       |
|   - Executes `cobalt/build/gn.py -p <platform> -C <build_type> --check`     |
|   - Iteratively diagnoses and repairs visibility & template forwarding      |
+-----------------------------------------------------------------------------+
                                     |
                                     v
+-----------------------------------------------------------------------------+
| Phase 3: Compiler Self-Healing Loop (autoninja_loop.py)                     |
|   - Invokes `autoninja -C out/<dir> <target>`                               |
|   - Parses Clang/GCC error diagnostics                                      |
|   - Generates surgical SEARCH/REPLACE code patches via Vertex AI            |
|   - Escalates to Pro model (gemini-2.5-pro) on repeat diagnostics           |
+-----------------------------------------------------------------------------+
                                     |
                                     v
+-----------------------------------------------------------------------------+
| Post-Rebase: Knowledge Bank Sync & Report Generation                        |
|   - Records working code patches to out/memory/knowledge_bank.json          |
|   - Uploads knowledge bank to Google Cloud Storage (GCS) if configured      |
|   - Generates final execution summary in results/M139_rebase_summary.md     |
+-----------------------------------------------------------------------------+
```

---

## 2. Directory Structure

```
.github/rebase/
├── README.md              # Documentation and operational guide
├── run_rebase_pipeline.py # End-to-end multi-phase orchestrator
├── autoninja_loop.py      # autoninja compiler self-healing loop
├── resolve_conflicts.py   # Multi-file merge conflict resolver & AST verifier
├── reasoning_engine.py    # Direct Vertex AI client (google.genai SDK)
├── deploy.py              # Vertex AI Reasoning Engine lifecycle manager
├── rebase_memory.py       # Knowledge bank interface with GCS sync
├── test_rebase_suite.py   # Comprehensive unit test suite (7/7 passing)
├── test_api_connection.py # Vertex AI connectivity & authentication diagnostic
├── skills/                # Declarative domain instructions (Markdown)
│   ├── cobalt_rebase.md       # Master guidelines, Code Search & Gitiles
│   ├── compiler_healing.md    # Compiler & linker break patterns
│   ├── gn_healing.md          # GN build rules & visibility restrictions
│   └── conflict_resolution.md # DEPS AST & merge conflict rules
└── results/               # Generated reports
    └── M139_rebase_summary.md
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

## 4. Vertex AI Reasoning Engine Deployment (`deploy.py`)

To deploy, update, or list managed Reasoning Engine instances on Vertex AI:

* **Deploy**:
  ```bash
  python3 .github/rebase/deploy.py deploy \
    --project-id "$GCP_PROJECT" \
    --location "us-central1" \
    --staging-bucket "gs://your-staging-bucket"
  ```

* **Update**:
  ```bash
  python3 .github/rebase/deploy.py update \
    --resource-id "<REASONING_ENGINE_ID>" \
    --project-id "$GCP_PROJECT" \
    --location "us-central1" \
    --staging-bucket "gs://your-staging-bucket"
  ```

* **List**:
  ```bash
  python3 .github/rebase/deploy.py list \
    --project-id "$GCP_PROJECT" \
    --location "us-central1"
  ```

---

## 5. Domain Skills (`skills/`)

Prompts and domain rules are completely decoupled from Python code. To adjust rebase heuristics or add new patterns for future Chromium rolls (e.g. M140), edit the markdown files in `skills/`:

* **`skills/cobalt_rebase.md`**: Master behavior preservation principles, Starboard macros (`USE_STARBOARD_MEDIA`, `IS_COBALT`), and investigation workflows using Chromium Code Search (`source.chromium.org`) and Gitiles (`chromium.googlesource.com`).
* **`skills/compiler_healing.md`**: C++/Java header splits (e.g. `base/notimplemented.h`, `base/timer/elapsed_timer.h`), method signature updates, and linker stubs.
* **`skills/gn_healing.md`**: GN visibility rules and template variable forwarding.
* **`skills/conflict_resolution.md`**: Upstream roll priority, DEPS syntax rules, and multi-turn tool commands.

---

## 6. Long-Term Knowledge Bank & GCS Sync

Successful code fixes are recorded in `out/memory/knowledge_bank.json` and fed into future Gemini prompts as few-shot working examples.

To persist the knowledge bank across ephemeral build machines via GCS:
```bash
export GCS_MEMORY_URI="gs://your-bucket-name/rebase_memory/knowledge_bank.json"
```
The pipeline will automatically **pull** existing memory at startup and **push** new fixes upon completion.

---

## 7. Running Unit Tests

To run the local unit test suite (verifies conflict extractors, AST validation, diagnostic parsers, and GN diff application):

```bash
python3 .github/rebase/test_rebase_suite.py
```
