# Complete Action

1. **Verify & Stage Changes**:
   - Verify code compiles without warnings and all unit/host tests pass.
   - Stage modified source files, test files, and the error report:
     ```bash
     git add <impacted-files> context/errors/
     ```

2. **Commit the Fix**:
   - Commit using Conventional Commits format (per `context/ai-interaction.md`):
     ```bash
     git commit -m "fix(<subsystem>): resolve ERR-{NNN} <short description>"
     ```

3. **Update Error Ledger with Commit SHA**:
   - Retrieve the commit SHA (`git rev-parse HEAD` and `git rev-parse --short HEAD`).
   - Update `context/errors/ERR-{NNN}-*.md`:
     - Update **Commit SHA** in the `## Metadata` table to `[`<short-sha>`](https://github.com/Thejus26/rain-predict/commit/<full-sha>)`.
   - Update `context/errors/README.md`:
     - Update the corresponding row in `## Chronological Index of Errors` with the commit SHA link.
   - Stage and record the ledger update:
     ```bash
     git add context/errors/
     git commit --amend --no-edit
     ```

4. **Merge to Base Branch**:
   - Determine the base branch (`master` or `main`).
   - Switch to the base branch:
     ```bash
     git checkout master
     ```
   - Merge the fix branch:
     ```bash
     git merge fix/ERR-{NNN}-{slug}
     ```

5. **Push to Remote**:
   - Push all changes to origin:
     ```bash
     git push origin master
     ```
   - If the fix branch was pushed to remote earlier, delete the remote branch:
     ```bash
     git push origin --delete fix/ERR-{NNN}-{slug}
     ```

6. **Clean Up Local Branch**:
   - Delete the merged local fix branch:
     ```bash
     git branch -d fix/ERR-{NNN}-{slug}
     ```

7. **Report Completion**:
   - Display a summary of the resolved error ID, commit SHA link, merged branch, and updated error ledger status.
