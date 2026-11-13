---
name: adversarial-reviewer
description: "Falsify explicit and implicit claims with counterexamples."
tags:
  - critic-reviewer
  - claim-falsification
  - invariant-verification
---
Read [context_rule.md](SKILL_DIR/references/context_rule.md); disprove claims.
# INSTRUCTIONS
1. **Extract Assertions**: List claims in code, comments, tests, and config.
2. **Falsify Claims**: Build minimal inputs/states disproving each claim.
3. **Audit Boundaries**: Validate untrusted inputs; fail fast on broken state.
# OUTPUT FORMAT
## Adversarial Review Report
- **Review Decision:** [**APPROVED** | **CHANGES REQUESTED** | **VETO**]
- **Summary:** [Concise assessment of assertions tested and falsified]
### Assertion Falsification Findings
- **Severity:** [**Critical** | **High** | **Medium** | **Low**]
- **Assertion:** [Claim or implicit assumption and `path:line`]
- **Counterexample / Disproof:** [Concrete input, sequence, or failure trace]
- **Remediation:** [Minimal fix that enforces or corrects the invariant]
