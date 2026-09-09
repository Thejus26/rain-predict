# Review Action

1. Read `context/current-feature.md` to understand the target deliverables and constraints.
2. Review all code changes made on the feature branch.
3. Check against `context/coding-standards.md`:
   - ✅ Goals fully met and verified
   - ❌ Goals missing or incomplete
   - 🛡️ Memory & pointer safety (zero `malloc`, `NULL` checks, bounded buffers)
   - ⚡ Power & timing compliance (no busy-waits, proper sleep mode handling)
   - ⚠️ Status code handling and remote bus timeout guards
   - 🚫 Scope creep (unnecessary changes or dependencies)
4. Final verdict: **Ready to complete** or **Action items required**.