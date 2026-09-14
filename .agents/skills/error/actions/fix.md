# Fix Action

1. **Locate Target Error Report**:
   - Check `$ARGUMENTS` (after "fix"):
     - If an Error ID or report name is provided (e.g., `ERR-020` or `ERR-021`): Load `context/errors/ERR-{NNN}-*.md`.
     - If empty: Find the latest or highest-numbered error report in `context/errors/` that has pending resolution.
   - Read the error report to understand symptoms, root cause, and impacted files.

2. **Create Fix Branch**:
   - Verify working tree is clean.
   - Create and checkout a dedicated branch named `fix/ERR-{NNN}-{slug}` (e.g., `fix/ERR-021-missing-prototype`):
     ```bash
     git checkout -b fix/ERR-{NNN}-{slug}
     ```

3. **Implement the Fix**:
   - Inspect upstream and downstream callers:
     - If local knowledge graph is available, query the callers of the functions/structs being modified (`graphify query "<symbol>"`) to ensure the fix does not break assumptions in calling modules.
   - Apply minimal, targeted corrections to the impacted source, header, build, or test files.
   - Enforce project guidelines from `context/coding-standards.md`:
     - Standard `<stdint.h>` types (`uint8_t`, `int16_t`, `uint32_t`, etc.)
     - Memory safety: zero dynamic memory allocation (`malloc`/`free` prohibited)
     - Strict C99 compliance with `-Wall -Wextra -Werror`
     - Proper header guards and static function prototypes
     - Safe status code propagation (`status_t`)

4. **Update Error Report Document**:
   - Update `## 3. Resolution & Code Changes` in `context/errors/ERR-{NNN}-*.md`:
     - Detail the concrete resolution steps taken.
     - Include the unified diff of changes made.
   - Update `## 4. Verification & Prevention Guidelines` if additional prevention checks were identified.

5. **Verify**:
   - Execute unit tests or build verification where available to ensure the fix works and no regressions are introduced.

6. **Prompt Next Step**:
   - Present a concise diff summary of the fix.
   - Advise running `/error complete` to commit the fix, update the error ledger, and push.
