# AI Interaction Guidelines

## Communication

- Be concise, direct, and explain concepts simply (like I'm 15) using intuitive, real-world analogies and plain English
- Note: The local machine does not have CMake/C-compiler installed; all C compilation, linting, and unit test execution take place in GitHub Actions CI
- Explain non-obvious engineering and design decisions clearly
- Ask before large refactors or architectural changes
- Don't add features not specified in the project scope or requirements
- Never delete files without explicit confirmation

## Workflow

This is the standard workflow used for every feature, driver, algorithm, or fix:

1. **Document** - Document the feature/task in `context/current-feature.md`.
2. **Branch** - Create a new git branch for the feature/fix (e.g., `feature/[feature-name]` or `fix/[fix-name]`).
3. **Implement** - Implement the feature/fix as defined in `context/current-feature.md` adhering to `context/coding-standards.md`.
4. **Build & Test** - Verify the build compiles without warnings (`-Wall -Wextra -Werror`), run static analysis (`cppcheck`/linters), and execute unit/host tests where applicable.
5. **Iterate** - Refine and optimize logic, timing, memory footprints, or power modes as needed.
6. **Commit** - Only after the build passes and tests succeed.
7. **Merge** - Merge into `main`.
8. **Delete Branch** - Clean up the feature branch once merged.
9. **Review** - Review generated code periodically and on demand.
10. **Update Tracker** - Mark as completed in `context/current-feature.md` and add an entry to history.

> Do NOT commit without permission and until the build/verification passes. If the build or tests fail, resolve the issues first.

## Branching

- Create a new branch for every feature or fix.
- Name branches following the pattern: `feature/[feature-name]`, `fix/[fix-name]`, `driver/[sensor-name]`, `refactor/[module-name]`.
- Ask to delete the branch once merged into `main`.

## Commits

- Ask before committing (do not auto-commit).
- Use Conventional Commits format (`feat:`, `fix:`, `docs:`, `style:`, `refactor:`, `test:`, `chore:`).
- Keep commits atomic and focused (one logical change per commit).
- Never include AI attribution watermarks in commit messages.

## When Stuck

- If something is not resolving after 2–3 attempts, stop and explain the issue clearly.
- Avoid trial-and-error guessing with hardware registers or math routines.
- Ask for clarification if requirements, hardware pinouts, or protocols are ambiguous.

## Code Changes

- Make minimal, targeted changes to accomplish the task.
- Do not refactor unrelated modules unless explicitly asked.
- Avoid introducing unnecessary external dependencies or dynamic allocations.
- Maintain existing firmware patterns, naming conventions, and layer separation.

## Code Review & Quality Checklist

Review code periodically, specifically ensuring:

- **Safety & Memory**: Zero dynamic memory allocation (`malloc`/`free`), pointer `NULL` checks, array bounds checking, and stack usage sanity.
- **Reliability**: Status code checking (`status_t`), non-blocking timeouts on external communication buses (RS-485/Modbus, I2C, SPI, LoRaWAN), and proper watchdog servicing.
- **Power & Hardware**: No busy-wait delays in runtime loops, proper low-power mode transitions, and peripheral de-initialization before sleep.
- **Numerical Correctness**: Prevention of divide-by-zero, valid range clamping, and proper floating-point/fixed-point usage on Cortex-M4.