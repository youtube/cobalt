# VCS Isolation & Workflows

**VCS Isolation Rule:** Any modifications to MAGI files (e.g., adding/updating
personas by Training) MUST be excluded from the feature/bugfix CL. The staging
and submission workflow branches dynamically based on
`project.magi.json#environment/vcs`:

- **For JJ (Jujutsu):** Work in parallel sibling changes (both rooted at
  `main@origin`) from the start: one for the feature/bugfix and one for the MAGI
  upgrades. If they accidentally get mixed, Release MUST use `jj split` or
  `jj squash -i` to cleanly separate the changes before pushing.
- **For GIT:** Use standard git branching. Stage *only* product source files for
  the feature CL. Stage *only* MAGI updates for the secondary CL.
