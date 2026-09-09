---
name: feature
description: Manage current firmware feature workflow - load, start, test, review, explain, or complete
argument-hint: load|start|test|review|explain|complete
---

## Context

Read the current feature from:
[current-feature.md](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/current-feature.md)

## File Structure

`current-feature.md` has these sections:

- `# Current Feature` - H1 heading with feature name when active
- `## Status` - `Not Started` | `In Progress` | `Complete`
- `## Goals` - Bullet points of what success looks like
- `## Notes` - Hardware constraints, register maps, memory, or power specs
- `## History` - Completed features (append only)

## Task

Execute the requested action: `$ARGUMENTS`

---

### If action is "load":

1. Check `$ARGUMENTS` (after "load"):
   - If it looks like a filename (single word, no spaces): Look for `context/features/{name}.md`
   - If it's multiple words: Use as inline feature description, generate goals
   - If empty: Error - "load" requires a spec filename or feature description
2. Update `current-feature.md`:
   - Update H1 heading to include feature name (e.g., `# Current Feature: Rain Trend Algo`)
   - Write goals as bullet points under `## Goals`
   - Write any additional notes/context under `## Notes`
   - Set Status to `Not Started`
3. Confirm spec loaded and show the feature summary

---

### If action is "start":

1. Read `current-feature.md` - verify Goals are populated
2. If empty, error: "Run /feature load first"
3. Set Status to `In Progress`
4. Create and checkout the feature branch (derive name from H1 heading, e.g., `feature/rain-trend-algo`)
5. List the goals, then implement them one by one according to `context/coding-standards.md`

---

### If action is "test":

1. Read `current-feature.md` to understand what was implemented
2. Identify driver logic, numerical routines, state machines, or protocol encoders added/modified
3. Check if host unit tests exist
4. For modules with testable logic:
   - Create or update test files using host test runners (Unity/Ceedling/Python simulation)
   - Test happy path, extreme sensor limits, checksum failures, and bus timeouts
   - Verify numerical stability without dynamic allocation
5. Run the test harness to verify all tests pass
6. Report test execution results and coverage

---

### If action is "review":

1. Read `current-feature.md` to understand the goals
2. Review all code changes made for this feature
3. Check against `context/coding-standards.md`:
   - ✅ Goals met
   - ❌ Goals missing or incomplete
   - 🛡️ Memory safety (zero `malloc`, `NULL` checks, static buffers)
   - ⚡ Power & timing (no busy-waits, proper sleep transitions)
   - ⚠️ Status code handling and remote bus timeout guards
   - 🚫 Scope creep (code beyond goals)
   - 🧪 Adequate test coverage for new code
4. Final verdict: Ready to complete or needs changes

---

### If action is "explain":

1. Read `current-feature.md` to understand what was implemented
2. Run `git diff main --name-only` to get list of files changed
3. For each file created or modified:
   - Show the file path (`.c`, `.h`, `.ld`, Makefile)
   - Give a 1-2 sentence explanation of what it does / what changed
   - Highlight any key functions, structs, or register configurations
4. End with a brief summary of how the firmware components fit together

Output format:

## Files Changed

**path/to/driver_bme280.c** (new)
Brief explanation of what this driver does and why it was added.

**path/to/rain_algo.c** (modified)
What changed and why.

## How It All Connects

Brief summary of the data and control flow between these files.

---

### If action is "complete":

1. Run a final review to ensure compilation passes without warnings (`-Wall -Werror`)
2. Stage all changes
3. Commit with a conventional commit message based on the feature
4. Merge into `main`
5. Switch back to `main` branch
6. Reset `current-feature.md`:
   - Change H1 back to `# Current Feature`
   - Clear Goals and Notes sections
   - Set Status to `Not Started`
7. Add feature summary to the END of History

---

If no action provided, explain the available options: `load`, `start`, `test`, `review`, `explain`, `complete`