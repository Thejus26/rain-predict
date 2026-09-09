---
name: feature
description: Manage current firmware feature workflow - load, start, test, review, explain, or complete
argument-hint: load|start|test|review|explain|complete
---

# Feature Workflow

Manages the complete lifecycle of a firmware feature, driver, algorithm, or communication module from specification to merge.

## Working File

[current-feature.md](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/current-feature.md)

### File Structure

`current-feature.md` contains the following sections:

- `# Current Feature` - H1 heading with feature name when active
- `## Status` - `Not Started` | `In Progress` | `Complete`
- `## Goals` - Measurable criteria and deliverables for the feature
- `## Notes` - Hardware constraints, register maps, memory limits, or power requirements
- `## History` - Completed features list (append only)

## Task

Execute the requested action: `$ARGUMENTS`

| Action | Description |
|---|---|
| `load` | Load a firmware feature spec from `context/features/` or inline description |
| `start` | Begin implementation, create git feature branch |
| `test` | Implement and execute unit tests / simulation tests |
| `review` | Verify all goals are met against `coding-standards.md` |
| `explain` | Document firmware architectural changes and data flow |
| `complete` | Stage, commit, merge to main, and reset `current-feature.md` |

See [actions/](actions/) for detailed instructions for each step.

If no action is provided, explain the available options.