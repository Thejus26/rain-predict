---
name: cleanup
description: Clean up project housekeeping tasks (add "run" to execute fixes)
argument-hint: run|check
---

Review the firmware codebase and project context for cleanup tasks:

1. **Feature Tracker Order**: Verify that the history in `context/current-feature.md` is strictly in chronological order (oldest to newest).
2. **Debug Logging & Printfs**: Find active debug print/log statements or raw `printf` calls left in production firmware code paths.
3. **Unused Includes**: Find unused `#include` header directives across `.c` and `.h` files.
4. **Stale TODOs**: Identify unresolved `TODO`, `FIXME`, or `HACK` comments.
5. **Orphaned Source Files**: Detect orphaned or unreferenced `.c`, `.h`, or configuration files not included in the build system.
6. **Context Alignment**: Verify that context documents (`project-overview.md`, `coding-standards.md`, `ai-interaction.md`) accurately reflect current hardware, pinouts, and architecture.
7. **Type Standardization**: Identify non-standard bare C types (`int`, `long`, `short`, `unsigned`) that should use explicit `<stdint.h>` types (`uint8_t`, `int16_t`, `uint32_t`).
8. **Header Guards**: Find missing, mismatched, or non-standard `#ifndef MODULE_H` include guards in all header files.

**Mode: $ARGUMENTS**

If no argument or argument is "check":
- Only report findings with line numbers and file paths; do not modify any files.
- Categorize what WOULD be cleaned up.

If the argument is "run" or "fix":
- First, report all findings in a numbered list.
- Then ask: "Which items would you like me to fix? (enter numbers like 1,3,5 or 'all' or 'none')"
- Wait for user response before making any changes.
- Only modify the specific items requested by the user.
- Summarize all completed fixes clearly.
