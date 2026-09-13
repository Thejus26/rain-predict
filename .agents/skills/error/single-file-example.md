---
name: error
description: Manage error logging, debugging, and fix lifecycle - log, fix, or complete
argument-hint: log|fix|complete
---

## Context

Error records and incident logs are managed in:
- [context/errors/](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/errors/)
- [context/errors/README.md](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/errors/README.md)

## File Structure

Individual error reports follow `ERR-XXX-YYYY-MM-DD-slug.md` with sections:
- `# Error Report: ERR-XXX - <Title>`
- `## Metadata` (Error ID, Date & Time, Commit SHA, Sprint/Task, Severity, Impacted Files)
- `## 1. Description & Symptoms`
- `## 2. Root Cause Analysis`
- `## 3. Resolution & Code Changes`
- `## 4. Verification & Prevention Guidelines`

The index ledger (`context/errors/README.md`) maintains:
- `## Chronological Index of Errors` - Append-only Markdown table tracking ID, Date/Time, Commit, Component, Title/Summary, and Report Link
- `## Error Classification by Category` - Mermaid pie chart tracking distribution

## Task

Execute the requested action: `$ARGUMENTS`

---

### If action is "log":

1. Check `$ARGUMENTS` (after "log") or infer from recent compiler errors, failed tests, or user input.
2. Read `context/errors/README.md` to find the latest error ID and increment it (e.g. `ERR-020` -> `ERR-021`).
3. Create `context/errors/ERR-{NNN}-{YYYY-MM-DD}-{slug}.md` using current timestamp and standard report layout.
4. Append entry row to `## Chronological Index of Errors` table in `context/errors/README.md`.
5. Update category count in `## Error Classification by Category` pie chart if applicable.
6. Display confirmation and suggest running `/error fix ERR-{NNN}`.

---

### If action is "fix":

1. Locate the target error report in `context/errors/` from `$ARGUMENTS` or select the latest unresolved error.
2. Review symptoms, root cause, and impacted files.
3. Create and checkout a dedicated fix branch: `fix/ERR-{NNN}-{slug}`.
4. Implement minimal, targeted code changes adhering to `context/coding-standards.md`.
5. Update `## 3. Resolution & Code Changes` in the error report with explanation and unified diff.
6. Verify fix through unit tests and static checks.
7. Present diff summary and suggest running `/error complete`.

---

### If action is "complete":

1. Stage all code changes and the error report (`git add`).
2. Commit with conventional format: `fix(<subsystem>): resolve ERR-{NNN} <summary>`.
3. Retrieve commit SHA and update `context/errors/ERR-{NNN}-*.md` and `context/errors/README.md` with commit SHA link.
4. Amend commit to include ledger updates: `git commit --amend --no-edit`.
5. Checkout base branch (`master` or `main`), merge the fix branch, and push to origin (`git push origin master`).
6. Delete the local fix branch (`git branch -d fix/ERR-{NNN}-{slug}`).
7. Output completion summary.

---

If no action provided, explain the available options: `log`, `fix`, `complete`.
