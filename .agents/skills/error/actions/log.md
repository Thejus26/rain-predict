# Log Action

1. **Parse Arguments & Error Details**:
   - Check `$ARGUMENTS` (after "log"):
     - If inline text provided: Extract error title, impacted files, subsystem, severity, and description.
     - If empty: Infer details from the latest build output, compiler diagnostics, test failure logs, or prompt the user for details.
   - **Blast Radius & Impact Analysis (Local Knowledge Graph)**:
     - If `graphify-out/graph.json` is available, query failing symbols, functions, or structs (`graphify query "<failing_symbol>"`) to automatically discover all upstream callers and downstream dependencies.
     - Automatically include all dependent header and source files in the impacted files list.

2. **Determine Next Error ID**:
   - Read `context/errors/README.md` and inspect `context/errors/` directory.
   - Find the highest existing error number (e.g., `ERR-020`) and increment by 1 to get `ERR-{NNN}` (e.g., `ERR-021`).

3. **Generate Error Report File**:
   - Create `context/errors/ERR-{NNN}-{YYYY-MM-DD}-{slug}.md` using today's date and a kebab-case slug of the title.
   - Populate standard sections:
     - `# Error Report: ERR-{NNN} - <Title>`
     - `## Metadata` (Error ID, Date & Time with timezone, current commit SHA link, Sprint/Task, Severity, Impacted Files)
     - `## 1. Description & Symptoms` (exact error output, compiler diagnostic, test failure trace, or behavior)
     - `## 2. Root Cause Analysis` (explanation of why the issue occurred)
     - `## 3. Resolution & Code Changes` (proposed fix strategy or initial code diff; marked *Pending Implementation* if unfixed)
     - `## 4. Verification & Prevention Guidelines` (prevention rules, testing recommendations, and coding guidelines)

4. **Update Ledger Index**:
   - Open `context/errors/README.md`.
   - Append a new row to the `## Chronological Index of Errors` table:
     ```markdown
     | **`ERR-{NNN}`** | YYYY-MM-DD HH:MM:SS | [`<short-sha>`](https://github.com/Thejus26/rain-predict/commit/<full-sha>) | <Subsystem> | **<Title>**: <Summary>. | [ERR-{NNN} Report](file:///C:/Users/ENERGY%20SAVER/Documents/Learning/Job%20Projects/rain-predict/context/errors/ERR-{NNN}-{YYYY-MM-DD}-{slug}.md) |
     ```
   - Increment the relevant category count in the `## Error Classification by Category` Mermaid pie chart.

5. **Confirm and Guide**:
   - Output confirmation showing the generated Error ID, report file link, and prompt:
     `"Error logged as ERR-{NNN}. Run /error fix ERR-{NNN} to begin fixing."`
