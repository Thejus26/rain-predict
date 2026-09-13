---
name: error
description: Manage error logging, debugging, and fix lifecycle - log, fix, or complete
argument-hint: log|fix|complete
---

# Error Management Workflow

Manages the complete lifecycle of bugs, compiler errors, linker faults, algorithmic discrepancies, and formatting issues from initial logging and root cause investigation to branch creation, fixing, and remote completion.

## Working Directory & Files

- [context/errors/](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/errors/) - Folder containing individual error report records (`ERR-XXX-YYYY-MM-DD-slug.md`)
- [context/errors/README.md](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/errors/README.md) - Immutable chronological ledger and index table of all logged errors

### Error Report Structure

Each error file (`ERR-XXX-YYYY-MM-DD-slug.md`) contains the following sections:

- `# Error Report: ERR-XXX - <Title>` - H1 heading with error ID and title
- `## Metadata` - Table containing Error ID, Date & Time, Commit SHA, Sprint/Task, Severity, and Impacted Files
- `## 1. Description & Symptoms` - Exact error diagnostics, compiler outputs, or failure traces
- `## 2. Root Cause Analysis` - Deep-dive technical explanation of the underlying failure mechanism
- `## 3. Resolution & Code Changes` - Fix strategy, resolution steps, and code diff
- `## 4. Verification & Prevention Guidelines` - Prevention checklist, test validation, and architectural safeguards

## Task

Execute the requested action: `$ARGUMENTS`

| Action | Description |
|---|---|
| `log` | Log an error, compiler diagnostic, or bug into `context/errors/` and update `README.md` index |
| `fix` | Create a dedicated `fix/` git branch, analyze the error report, and implement the fix |
| `complete` | Stage and commit the fix, update error report/ledger with commit SHA, merge to base branch, and push to origin |

See [actions/](actions/) for detailed instructions for each step.

If no action is provided, explain the available options: `log`, `fix`, `complete`.
