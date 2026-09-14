# Complete Action

1. Stage all changes and commit with a descriptive message:
   - Ensure clean staging: only stage relevant firmware source/headers, tests, docs, and context specs.
   - Never stage local virtual environments, tool caches, or private graph outputs.
2. Switch to main (or master) and merge the feature branch (no push yet)
3. Delete the local feature branch
4. Reset current-feature.md:
   - Change H1 back to `# Current Feature`
   - Clear Goals and Notes sections (keep placeholder comments)
   - Add feature summary to the END of History
5. Commit the reset: `chore: reset current-feature.md after completing [feature]`
6. Push main/master to origin ONCE (single push with all changes)
7. If feature branch was previously pushed, delete it from origin